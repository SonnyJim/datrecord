#ifndef DATRECORD_H
#define DATRECORD_H

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

/* Inline Byte-swapping helpers for Little-Endian WAV data on Big-Endian IRIX (MIPS) */
static inline unsigned int swap_uint32(unsigned int val) {
    return ((val >> 24) & 0x000000FF) |
           ((val >> 8)  & 0x0000FF00) |
           ((val << 8)  & 0x00FF0000) |
           ((val << 24) & 0xFF000000);
}

static inline unsigned short swap_uint16(unsigned short val) {
    return (unsigned short)(((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00));
}

/* Parity calculation helper declaration */
unsigned char compute_pack_parity(const unsigned char *pack_bytes);

/* Core recording procedure declaration */
int record_wav_to_dat(const char *wav_path, const char *tape_path, int pno);

#endif /* DATRECORD_H */
