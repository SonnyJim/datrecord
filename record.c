#include "datrecord.h"

/* Compute Parity byte across bytes 0-6 of subcode pack */
unsigned char compute_pack_parity(const unsigned char *pack_bytes) {
    unsigned char parity = 0;
    int i;
    for (i = 0; i < 7; i++) {
        parity ^= pack_bytes[i];
    }
    return parity;
}

/* Record WAV audio file to DAT tape drive */
int record_wav_to_dat(const char *wav_path, int tape_fd, int pno, int *running_frame_counter) {
    int wav_fd;
    wav_header_t raw_hdr;
    DTFRAME frame;
    ssize_t bytes_read;
    int track_frame_counter = 0; /* Tracks frames for THIS song only */

    /* Parsed WAV properties */
    unsigned int sample_rate;
    unsigned short channels;
    unsigned short bits_per_sample;
    size_t audio_bytes_per_frame;

    /* 1. Open and Parse WAV Header */
    wav_fd = open(wav_path, O_RDONLY);
    if (wav_fd < 0) {
        perror("Error opening WAV file");
        return 1;
    }

    if (read(wav_fd, &raw_hdr, WAV_HEADER_SIZE) != WAV_HEADER_SIZE) {
        fprintf(stderr, "Failed to read WAV header from %s.\n", wav_path);
        close(wav_fd);
        return 1;
    }

    if (strncmp(raw_hdr.riff, "RIFF", 4) != 0 || strncmp(raw_hdr.wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Invalid WAV file header in %s.\n", wav_path);
        close(wav_fd);
        return 1;
    }

    /* Swap Little-Endian WAV header values to Big-Endian */
    sample_rate     = swap_uint32(raw_hdr.sample_rate);
    channels        = swap_uint16(raw_hdr.channels);
    bits_per_sample = swap_uint16(raw_hdr.bits_per_sample);

    /* Determine actual audio data payload size per DAT frame based on rate */
    if (sample_rate == 48000) {
        audio_bytes_per_frame = DTDA_DATASIZE48K; /* 5760 bytes */
    } else if (sample_rate == 44100) {
        audio_bytes_per_frame = DTDA_DATASIZE44K; /* 5292 bytes */
    } else if (sample_rate == 32000) {
        audio_bytes_per_frame = DTDA_DATASIZE32K; /* 3840 bytes */
    } else {
        audio_bytes_per_frame = DTDA_DATASIZE44K;
    }

    printf("\n=== Program %d: %s (%u Hz, %u ch, %u-bit) ===\n",
           pno, wav_path, sample_rate, channels, bits_per_sample);

    /* 2. Streaming Loop */
    while ((bytes_read = read(wav_fd, frame.audio, audio_bytes_per_frame)) > 0) {
        if (bytes_read < DTDA_DATASIZE) {
            memset(frame.audio + bytes_read, 0, DTDA_DATASIZE - bytes_read);
        }

        memset(&frame.subcode, 0, sizeof(frame.subcode));

        /* --- Main ID (`mid`) Setup --- */
        frame.subcode.mid.fmtid = DT_AUDIO_USE;
        frame.subcode.mid.emphasis = DTM_PREEMPH_OFF;
        frame.subcode.mid.quantization = DTM_QUAN_16_LINEAR;
        frame.subcode.mid.numchans = (channels == 4) ? DTM_NCHAN_FOUR : DTM_NCHAN_TWO;

        if (sample_rate == 48000)      frame.subcode.mid.sampfreq = DT_FREQ48000;
        else if (sample_rate == 44100) frame.subcode.mid.sampfreq = DT_FREQ44100;
        else if (sample_rate == 32000) frame.subcode.mid.sampfreq = DT_FREQ32000;
        else                           frame.subcode.mid.sampfreq = DT_FREQ44100;

        /* Calculate BCD Program Number */
        int pno_bcd = ((pno / 100) % 10 << 8) | ((pno / 10) % 10 << 4) | (pno % 10);

        /* DAT Standard: Assert Start ID & PNO for the first 300 frames (~9 sec) */
        int is_start_id_active = (track_frame_counter < 300);

        /* --- Sub ID (`sid`) Setup --- */
        /* Set Bit 0 of ctrlid (0x01) for Start ID flag */
        frame.subcode.sid.ctrlid = is_start_id_active ? 0x01 : 0x00;
        frame.subcode.sid.dataid = DT_AUDIO_USE;

        if (is_start_id_active) {
            frame.subcode.sid.pno1 = (pno_bcd >> 8) & 0x0F;
            frame.subcode.sid.pno2 = (pno_bcd >> 4) & 0x0F;
            frame.subcode.sid.pno3 = pno_bcd & 0x0F;
        } else {
            /* 0x0A (or 0xAA BCD nibbles) indicates PNO is inactive outside track start */
            frame.subcode.sid.pno1 = 0x0A;
            frame.subcode.sid.pno2 = 0x0A;
            frame.subcode.sid.pno3 = 0x0A;
        }
        frame.subcode.sid.numpacks = 2;

        /* --- Pack 0: Program Time (P-Time resets per track) --- */
        struct dttimepack *prg_pack = (struct dttimepack *)&frame.subcode.packs[0];
        prg_pack->id   = 1; /* Program Time Pack ID */
        prg_pack->flag = 0;
        prg_pack->pno1 = (pno_bcd >> 8) & 0x07;
        prg_pack->pno2 = (pno_bcd >> 4) & 0x0F;
        prg_pack->pno3 = pno_bcd & 0x0F;
        prg_pack->index.dhi = 0;
        prg_pack->index.dlo = 1;

        /* Program Timecode (Track-relative) */
        DTframetotc(track_frame_counter, &prg_pack->tc);
        prg_pack->parity = compute_pack_parity((unsigned char *)prg_pack);

        /* --- Pack 1: Absolute Time (A-Time continues across total tape) --- */
        struct dttimepack *abs_pack = (struct dttimepack *)&frame.subcode.packs[1];
        abs_pack->id   = 2; /* Absolute Time Pack ID */
        abs_pack->flag = 0;
        abs_pack->pno1 = prg_pack->pno1;
        abs_pack->pno2 = prg_pack->pno2;
        abs_pack->pno3 = prg_pack->pno3;
        abs_pack->index = prg_pack->index;

        /* Absolute Timecode (Tape-wide) */
        DTframetotc(*running_frame_counter, &abs_pack->tc);
        abs_pack->parity = compute_pack_parity((unsigned char *)abs_pack);

        /* --- Write DTFRAME --- */
        if (write(tape_fd, &frame, sizeof(DTFRAME)) != sizeof(DTFRAME)) {
            perror("Error writing DTFRAME to tape");
            close(wav_fd);
            return 1;
        }

        (*running_frame_counter)++;
        track_frame_counter++;

        if (*running_frame_counter % 100 == 0) {
            int h, m, s, f;
            DTframetohmsf(*running_frame_counter, &h, &m, &s, &f);
            printf("\rTotal Frames: %d | Timecode: %02d:%02d:%02d.%02d",
                   *running_frame_counter, h, m, s, f);
            fflush(stdout);
        }
    }

    close(wav_fd);
    return 0;
}
