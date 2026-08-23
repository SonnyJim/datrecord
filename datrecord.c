#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/types.h>
#include <dmedia/dataudio.h>

#define WAV_HEADER_SIZE 44
#define TAPE_DEV_DEFAULT "/dev/tape"

typedef struct {
    char           riff[4];
    unsigned int   overall_size;
    char           wave[4];
    char           fmt_chunk_marker[4];
    unsigned int   length_of_fmt;
    unsigned short format_type;
    unsigned short channels;
    unsigned int   sample_rate;
    unsigned int   byterate;
    unsigned short block_align;
    unsigned short bits_per_sample;
} wav_header_t;

/* Byte-swapping helpers for Little-Endian WAV data on Big-Endian IRIX (MIPS) */
static unsigned int swap_uint32(unsigned int val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

static unsigned short swap_uint16(unsigned short val) {
    return (unsigned short)(((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00));
}

/* Compute Parity byte across bytes 0-6 of subcode pack */
unsigned char compute_pack_parity(const unsigned char *pack_bytes) {
    unsigned char parity = 0;
    int i;
    for (i = 0; i < 7; i++) {
        parity ^= pack_bytes[i];
    }
    return parity;
}

int main(int argc, char *argv[]) {
    const char *wav_path = NULL;
    const char *tape_path = TAPE_DEV_DEFAULT;
    int tape_fd, wav_fd;
    struct mtop op;
    wav_header_t raw_hdr;
    DTFRAME frame;
    ssize_t bytes_read;
    int frame_count = 0;
    int pno = 1;

    /* Parsed WAV properties */
    unsigned int sample_rate;
    unsigned short channels;
    unsigned short bits_per_sample;
    size_t audio_bytes_per_frame;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wav> [tape_device] [program_number]\n", argv[0]);
        return 1;
    }

    wav_path = argv[1];
    if (argc >= 3) tape_path = argv[2];
    if (argc >= 4) pno = atoi(argv[3]);

    /* 1. Open and Parse WAV Header */
    wav_fd = open(wav_path, O_RDONLY);
    if (wav_fd < 0) {
        perror("Error opening WAV file");
        return 1;
    }

    if (read(wav_fd, &raw_hdr, WAV_HEADER_SIZE) != WAV_HEADER_SIZE) {
        fprintf(stderr, "Failed to read WAV header.\n");
        close(wav_fd);
        return 1;
    }

    if (strncmp(raw_hdr.riff, "RIFF", 4) != 0 || strncmp(raw_hdr.wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Invalid WAV file header.\n");
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

    /* 2. Open DAT Tape Drive in Audio Mode */
    tape_fd = open(tape_path, O_RDWR);
    if (tape_fd < 0) {
        perror("Error opening tape device");
        close(wav_fd);
        return 1;
    }

    memset(&op, 0, sizeof(op));
    op.mt_op = MTAUD;
    op.mt_count = 1;

    if (ioctl(tape_fd, MTIOCTOP, &op) < 0) {
        perror("ioctl(MTIOCTOP, MTAUD) failed");
        close(tape_fd);
        close(wav_fd);
        return 1;
    }

    printf("Streaming %s (%u Hz, %u ch, %u-bit) -> %s (Program %d)...\n",
           wav_path, sample_rate, channels, bits_per_sample, tape_path, pno);

    /* 3. Streaming Loop */
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

        /* Program Number Encoding (BCD) */
        int pno_bcd = ((pno / 100) % 10 << 8) | ((pno / 10) % 10 << 4) | (pno % 10);

        /* --- Sub ID (`sid`) Setup --- */
        frame.subcode.sid.ctrlid = 0;
        frame.subcode.sid.dataid = DT_AUDIO_USE;
        frame.subcode.sid.pno1 = (pno_bcd >> 8) & 0x0F;
        frame.subcode.sid.pno2 = (pno_bcd >> 4) & 0x0F;
        frame.subcode.sid.pno3 = pno_bcd & 0x0F;
        frame.subcode.sid.numpacks = 2;

        /* --- Pack 0: Program Time (Item ID 1) --- */
        struct dttimepack *prg_pack = (struct dttimepack *)&frame.subcode.packs[0];
        prg_pack->id   = 1;
        prg_pack->flag = 0;
        prg_pack->pno1 = (pno_bcd >> 8) & 0x07;
        prg_pack->pno2 = (pno_bcd >> 4) & 0x0F;
        prg_pack->pno3 = pno_bcd & 0x0F;

        prg_pack->index.dhi = 0;
        prg_pack->index.dlo = 1;

        DTframetotc(frame_count, &prg_pack->tc);
        prg_pack->parity = compute_pack_parity((unsigned char *)prg_pack);

        /* --- Pack 1: Absolute Time (Item ID 2) --- */
        struct dttimepack *abs_pack = (struct dttimepack *)&frame.subcode.packs[1];
        abs_pack->id   = 2;
        abs_pack->flag = 0;
        abs_pack->pno1 = prg_pack->pno1;
        abs_pack->pno2 = prg_pack->pno2;
        abs_pack->pno3 = prg_pack->pno3;

        abs_pack->index = prg_pack->index;
        abs_pack->tc    = prg_pack->tc;

        abs_pack->parity = compute_pack_parity((unsigned char *)abs_pack);

        /* --- Write DTFRAME --- */
        if (write(tape_fd, &frame, sizeof(DTFRAME)) != sizeof(DTFRAME)) {
            perror("Error writing DTFRAME to tape");
            break;
        }

        frame_count++;
        if (frame_count % 100 == 0) {
            int h, m, s, f;
            DTframetohmsf(frame_count, &h, &m, &s, &f);
            printf("\rFrames: %d | Timecode: %02d:%02d:%02d.%02d", frame_count, h, m, s, f);
            fflush(stdout);
        }
    }

    printf("\nFinished writing %d DAT frames.\n", frame_count);

    close(tape_fd);
    close(wav_fd);
    return 0;
}
