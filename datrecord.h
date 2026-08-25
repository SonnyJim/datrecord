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

/* Rigid structure packing for MIPS alignment rules */
#pragma pack(1)
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
#pragma pack(0)

/* 
 * Standard static C89/IRIX inline macros.
 * Handles both MIPSpro (CC / cc) and GCC environments smoothly.
 */
#if defined(__sgi) && !defined(__GNUC__)
#  define INLINE_FUNC static __inline
#else
#  define INLINE_FUNC static inline
#endif

INLINE_FUNC unsigned int swap_uint32(unsigned int val) {
    return ((val >> 24) & 0x000000FFU) |
           ((val >> 8)  & 0x0000FF00U) |
           ((val << 8)  & 0x00FF0000U) |
           ((val << 24) & 0xFF000000U);
}

INLINE_FUNC unsigned short swap_uint16(unsigned short val) {
    return (unsigned short)(((val >> 8) & 0x00FF) | ((val << 8) & 0xFF00));
}

/* Function declarations */
unsigned char compute_pack_parity(const unsigned char *pack_bytes);
int record_wav_to_dat(const char *wav_path, int tape_fd, int pno, int *frame_counter);

#endif /* DATRECORD_H */
