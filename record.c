#include "datrecord.h"

/* DAT Frame Definitions & Constraints */
#define LEADIN_FRAMES   600   /* 12 seconds @ standard DAT rates (IEC 61119 standard requires >= 9s) */
#define LEADOUT_FRAMES  300   /* 6 to 9 seconds */
#define START_ID_FRAMES 300   /* 9 seconds duration for active Start ID flags */

/* Compute Parity byte across bytes 0-6 of subcode pack */
unsigned char compute_pack_parity(const unsigned char *pack_bytes) {
    unsigned char parity = 0;
    int i;
    for (i = 0; i < 7; i++) {
        parity ^= pack_bytes[i];
    }
    return parity;
}

int write_dat_leadin(int tape_fd, int leadin_frames, unsigned int sample_rate, unsigned short channels)
{
    DTFRAME frame;
    int frame_no;

    fprintf(stdout, "Writing lead-in (%d frames)...\n", leadin_frames);
    if (leadin_frames < 1) {
        fprintf(stderr, "Invalid lead-in length: %d frames\n", leadin_frames);
        return 1;
    }

    for (frame_no = 0; frame_no < leadin_frames; frame_no++) {
        struct dttimepack *ptime;
        struct dttimepack *atime;
        unsigned long atime_frame;

        memset(&frame, 0, sizeof(frame));

        /* --- Main ID --- */
        frame.subcode.mid.fmtid        = DT_AUDIO_USE;
        frame.subcode.mid.emphasis     = DTM_PREEMPH_OFF;
        frame.subcode.mid.quantization = DTM_QUAN_16_LINEAR;
        frame.subcode.mid.numchans     = (channels == 4) ? DTM_NCHAN_FOUR : DTM_NCHAN_TWO;

        if (sample_rate == 48000)      frame.subcode.mid.sampfreq = DT_FREQ48000;
        else if (sample_rate == 44100) frame.subcode.mid.sampfreq = DT_FREQ44100;
        else if (sample_rate == 32000) frame.subcode.mid.sampfreq = DT_FREQ32000;
        else {
            fprintf(stderr, "Invalid DAT lead-in sample rate: %u\n", sample_rate);
            return 1;
        }

        /* --- Sub ID (Lead-in PNO = 0BB) --- */
        frame.subcode.sid.ctrlid   = 0;
        frame.subcode.sid.dataid   = DT_AUDIO_USE;
        frame.subcode.sid.pno1     = 0x0;
        frame.subcode.sid.pno2     = 0x0B;
        frame.subcode.sid.pno3     = 0x0B;
        frame.subcode.sid.numpacks = 2;

        /* Pack 0: Program Time (Invalid during Lead-in) */
        ptime = (struct dttimepack *)&frame.subcode.packs[0];
        ptime->id   = DTP_PTIME;
        ptime->flag = 0;
        ptime->pno1 = 0;
        ptime->pno2 = DT_INVALID;
        ptime->pno3 = DT_INVALID;

        ptime->index.dhi = DT_INVALID;
        ptime->index.dlo = DT_INVALID;

        ptime->tc.hhi = DT_INVALID; ptime->tc.hlo = DT_INVALID;
        ptime->tc.mhi = DT_INVALID; ptime->tc.mlo = DT_INVALID;
        ptime->tc.shi = DT_INVALID; ptime->tc.slo = DT_INVALID;
        ptime->tc.fhi = DT_INVALID; ptime->tc.flo = DT_INVALID;
        ptime->parity = compute_pack_parity((unsigned char *)ptime);

        /* Pack 1: Absolute Time (Counts down to 00:00:00.00 at end of Lead-in) */
        atime = (struct dttimepack *)&frame.subcode.packs[1];
        atime->id   = DTP_ATIME;
        atime->flag = 0;
        atime->pno1 = 0;
        atime->pno2 = DT_INVALID;
        atime->pno3 = DT_INVALID;

        atime->index.dhi = DT_INVALID;
        atime->index.dlo = DT_INVALID;

        atime_frame = (unsigned long)(leadin_frames - frame_no - 1);
        DTframetotc(atime_frame, &atime->tc);
        atime->parity = compute_pack_parity((unsigned char *)atime);

        if (write(tape_fd, &frame, sizeof(frame)) != sizeof(frame)) {
            perror("Error writing DAT lead-in frame");
            return 1;
        }
    }
    return 0;
}

int write_dat_leadout(int tape_fd, int leadout_frames, unsigned int sample_rate, unsigned short channels, int *running_frame_counter)
{
    DTFRAME frame;
    int frame_no;

    fprintf(stdout, "Writing leadout...\n");

    if (leadout_frames < 1 || running_frame_counter == NULL) {
        fprintf(stderr, "Invalid lead-out parameters\n");
        return 1;
    }

    for (frame_no = 0; frame_no < leadout_frames; frame_no++) {
        struct dttimepack *ptime;
        struct dttimepack *atime;

        memset(&frame, 0, sizeof(frame));

        frame.subcode.mid.fmtid        = DT_AUDIO_USE;
        frame.subcode.mid.emphasis     = DTM_PREEMPH_OFF;
        frame.subcode.mid.quantization = DTM_QUAN_16_LINEAR;
        frame.subcode.mid.numchans     = (channels == 4) ? DTM_NCHAN_FOUR : DTM_NCHAN_TWO;

        if (sample_rate == 48000)      frame.subcode.mid.sampfreq = DT_FREQ48000;
        else if (sample_rate == 44100) frame.subcode.mid.sampfreq = DT_FREQ44100;
        else if (sample_rate == 32000) frame.subcode.mid.sampfreq = DT_FREQ32000;
        else {
            fprintf(stderr, "Invalid DAT lead-out sample rate: %u\n", sample_rate);
            return 1;
        }

        /* Lead-out PNO = 0EE */
        frame.subcode.sid.ctrlid   = 0;
        frame.subcode.sid.dataid   = DT_AUDIO_USE;
        frame.subcode.sid.pno1     = 0x0;
        frame.subcode.sid.pno2     = 0x0E;
        frame.subcode.sid.pno3     = 0x0E;
        frame.subcode.sid.numpacks = 2;

        ptime = (struct dttimepack *)&frame.subcode.packs[0];
        ptime->id   = DTP_PTIME;
        ptime->flag = 0;
        ptime->pno1 = 0;
        ptime->pno2 = DT_INVALID;
        ptime->pno3 = DT_INVALID;

        ptime->index.dhi = DT_INVALID; ptime->index.dlo = DT_INVALID;
        ptime->tc.hhi = DT_INVALID; ptime->tc.hlo = DT_INVALID;
        ptime->tc.mhi = DT_INVALID; ptime->tc.mlo = DT_INVALID;
        ptime->tc.shi = DT_INVALID; ptime->tc.slo = DT_INVALID;
        ptime->tc.fhi = DT_INVALID; ptime->tc.flo = DT_INVALID;
        ptime->parity = compute_pack_parity((unsigned char *)ptime);

        atime = (struct dttimepack *)&frame.subcode.packs[1];
        atime->id   = DTP_ATIME;
        atime->flag = 0;
        atime->pno1 = 0;
        atime->pno2 = DT_INVALID;
        atime->pno3 = DT_INVALID;
        atime->index.dhi = DT_INVALID; atime->index.dlo = DT_INVALID;

        DTframetotc((unsigned long)*running_frame_counter, &atime->tc);
        atime->parity = compute_pack_parity((unsigned char *)atime);

        if (write(tape_fd, &frame, sizeof(frame)) != sizeof(frame)) {
            perror("Error writing DAT lead-out frame");
            return 1;
        }
        (*running_frame_counter)++;
    }
    return 0;
}

int record_wav_to_dat(const char *wav_path, int tape_fd, int pno, int *running_frame_counter) {
    int wav_fd;
    wav_header_t raw_hdr;
    DTFRAME frame;
    ssize_t bytes_read;
    int track_frame_counter = 0;
    int pno_bcd;
    int is_start_id_active;
    struct dttimepack *prg_pack;
    struct dttimepack *abs_pack;

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

    sample_rate     = swap_uint32(raw_hdr.sample_rate);
    channels        = swap_uint16(raw_hdr.channels);
    bits_per_sample = swap_uint16(raw_hdr.bits_per_sample);

    if (sample_rate == 48000) {
        audio_bytes_per_frame = DTDA_DATASIZE48K;
    } else if (sample_rate == 44100) {
        audio_bytes_per_frame = DTDA_DATASIZE44K;
    } else if (sample_rate == 32000) {
        audio_bytes_per_frame = DTDA_DATASIZE32K;
    } else {
        fprintf(stderr, "Invalid sample rate %u: Needs to be 32kHz, 44.1kHz or 48kHz\n", sample_rate);
        close(wav_fd);
        return 1;
    }

    if (channels != 2 || bits_per_sample != 16) {
        fprintf(stderr, "Unsupported format: requires 2 channels, 16-bit PCM\n");
        close(wav_fd);
        return 1;
    }

    /* Handle Lead-In (pno=1 & at start of tape) / Lead-Out (pno=DT_INVALID) */
    if (pno == 1 && *running_frame_counter == 0) {
        write_dat_leadin(tape_fd, LEADIN_FRAMES, sample_rate, channels);
    } else if (pno == DT_INVALID) {
        write_dat_leadout(tape_fd, LEADOUT_FRAMES, sample_rate, channels, running_frame_counter);
        close(wav_fd);
        return 0;
    }

    printf("\n=== Program %d: %s (%u Hz, %u ch, %u-bit) ===\n",
           pno, wav_path, sample_rate, channels, bits_per_sample);

    /* 2. Audio & Subcode Streaming Loop */
    while ((bytes_read = read(wav_fd, frame.audio, audio_bytes_per_frame)) > 0) {
        if ((size_t)bytes_read < audio_bytes_per_frame) {
            memset(frame.audio + bytes_read, 0, audio_bytes_per_frame - bytes_read);
        }

        memset(&frame.subcode, 0, sizeof(frame.subcode));

        /* Main ID Setup */
        frame.subcode.mid.fmtid        = DT_AUDIO_USE;
        frame.subcode.mid.emphasis     = DTM_PREEMPH_OFF;
        frame.subcode.mid.quantization = DTM_QUAN_16_LINEAR;
        frame.subcode.mid.numchans     = (channels == 4) ? DTM_NCHAN_FOUR : DTM_NCHAN_TWO;

        if (sample_rate == 48000)      frame.subcode.mid.sampfreq = DT_FREQ48000;
        else if (sample_rate == 44100) frame.subcode.mid.sampfreq = DT_FREQ44100;
        else if (sample_rate == 32000) frame.subcode.mid.sampfreq = DT_FREQ32000;

        /* BCD Program Number Encoding */
        pno_bcd = ((pno / 100) % 10 << 8) | ((pno / 10) % 10 << 4) | (pno % 10);
        is_start_id_active = (track_frame_counter < START_ID_FRAMES);

        /* Sub ID Setup (PNO persists across the entire track) */
        frame.subcode.sid.ctrlid = is_start_id_active ? (DTS_START | DTS_PRIORITYID) : 0x00;
        frame.subcode.sid.dataid = DT_AUDIO_USE;
        frame.subcode.sid.pno1   = (pno_bcd >> 8) & 0x0F;
        frame.subcode.sid.pno2   = (pno_bcd >> 4) & 0x0F;
        frame.subcode.sid.pno3   = pno_bcd & 0x0F;
        frame.subcode.sid.numpacks = 2;

        /* Pack 0: Program Time (P-Time) */
        prg_pack = (struct dttimepack *)&frame.subcode.packs[0];
        prg_pack->id   = DTP_PTIME;
        prg_pack->flag = 0;
        prg_pack->pno1 = (pno_bcd >> 8) & 0x0F;
        prg_pack->pno2 = (pno_bcd >> 4) & 0x0F;
        prg_pack->pno3 = pno_bcd & 0x0F;
        prg_pack->index.dhi = 0;
        prg_pack->index.dlo = 1;

        DTframetotc(track_frame_counter, &prg_pack->tc);
        prg_pack->parity = compute_pack_parity((unsigned char *)prg_pack);

        /* Pack 1: Absolute Time (A-Time) */
        abs_pack = (struct dttimepack *)&frame.subcode.packs[1];
        abs_pack->id   = DTP_ATIME;
        abs_pack->flag = 0;
        abs_pack->pno1 = prg_pack->pno1;
        abs_pack->pno2 = prg_pack->pno2;
        abs_pack->pno3 = prg_pack->pno3;
        abs_pack->index = prg_pack->index;

        DTframetotc(*running_frame_counter, &abs_pack->tc);
        abs_pack->parity = compute_pack_parity((unsigned char *)abs_pack);

        /* Write DTFRAME directly to raw tape device */
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
