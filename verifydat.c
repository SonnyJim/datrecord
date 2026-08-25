/*
 * Originally from DATGoodies.tar.Z
 *
 * A simple program to verify that a DAT tape
 * has been correctly written.
 *
 * Doug Cook
 * Silicon Graphics, Inc., December 1993
 *
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * Modified Sonnyjim August 2026
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tpsc.h>
#include <sys/mtio.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <dataudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <audio.h>

extern int mediad_get_exclusiveuse(char *, char *);

int dat;
static char *datdev = "/dev/nrtape";

int get_firmware_revision(int fd, int *maj, int *min) {
    ct_g0inq_data_t info;
    char revbuf1[MAX_INQ_PRL + 1];

    if (ioctl(fd, MTSCSIINQ, &info) >= 0) {
        strncpy(revbuf1, (char *)info.id_prl, MAX_INQ_PRL);
        revbuf1[MAX_INQ_PRL] = '\0';
        *maj = atoi(strtok(revbuf1, "."));
        *min = atoi(strtok(NULL, "."));
        return 0;
    }
    return -1;
}

int rewind_dat(int fd) {
    struct mtop mtc;
    struct mtget mtg;

    if (ioctl(fd, MTIOCGET, &mtg) < 0) {
        perror("Couldn't issue MTIOCGET");
        return 0;
    }
    if ((mtg.mt_erreg & (CT_AUD_MED >> 16)) == 0) {
        printf("tape is not an audio tape\n");
        return 0;
    }
    printf("Rewinding tape\n");
    mtc.mt_op = MTREW;
    mtc.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &mtc) < 0) {
        perror("MTREW failed");
        return 0;
    }
    return 1;
}

int prepare_dat(int fd) {
    struct mtop mtc;

    mtc.mt_op = MTAUD;
    mtc.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &mtc) < 0) {
        if (errno != EAGAIN) {
            perror("Couldn't put DAT into audio mode");
            return 0;
        }
    }
    return 1;
}
int verify_dat(int fd) {
    DTFRAME frame;
    struct dttimepack *tpp;
    struct dttimecode *tcp;
    struct dttimecode exp_ptime;
    struct dttimecode exp_atime;
    char s1[32] = "--:--:--:--";
    char s2[32] = "--:--:--:--";
    int local_prog = -1;
    int li = 0;
    int x = 1;
    int i1, i2, local_idx = 0;
    int np;

    int ptime_synced = 0;
    int atime_synced = 0;
    int ptime_valid = 0;
    int atime_valid = 0;
    int leadout_count = 0;

    memset(&exp_ptime, 0, sizeof(struct dttimecode));
    memset(&exp_atime, 0, sizeof(struct dttimecode));

    while (x) {
        if ((x = read(fd, &frame, sizeof(DTFRAME))) < 0) {
            perror("read failed");
            return 0;
        }

        /* Detect Program Numbers vs Control Codes */
        if (frame.subcode.sid.pno3 != 0xa
            && frame.subcode.sid.pno3 != 0xb
            && frame.subcode.sid.pno3 != 0xe) {
            
            leadout_count = 0; /* Reset leadout tracker */

            np = DTpnotodec(frame.subcode.sid.pno1,
                frame.subcode.sid.pno2, frame.subcode.sid.pno3);
            if (np != local_prog) {
                printf("\nprogram %d   \n", np);
                memset(&exp_ptime, 0, sizeof(struct dttimecode));
                local_prog = np;
                ptime_synced = 0;
            }
        }
        else {
            if (!li && frame.subcode.sid.pno3 == 0xb) {
                li = 1;
                printf("lead-in...\n");
            }
            if (frame.subcode.sid.pno3 == 0xe) {
                leadout_count++;
		printf("leadout_count: %i\n", leadout_count);
                if (leadout_count > 30) { /* Require sustained lead-out */
                    printf("\nlead-out...\n");
                    return 1;
                }
            } else {
                leadout_count = 0;
            }
            continue;
        }

        /* Program Time (ptime) */
        tpp = (struct dttimepack *)&frame.subcode.packs[DTP_PTIME-1];
        tcp = &(tpp->tc);
        if ((i1 = DTbcdtodec(tpp->index.dhi, tpp->index.dlo)) < 100) {
            if (i1 != local_idx) {
                local_idx = i1;
                printf("\nindex %d   \n", i1);
            }
        }

        ptime_valid = 0;
        if (DTtcvalid(tcp)) {
            if (!ptime_synced) {
                exp_ptime = *tcp;
                ptime_synced = 1;
            } else if (memcmp(tcp, &exp_ptime, sizeof(struct dttimecode)) != 0) {
                DTtimetoa(s1, &exp_ptime);
                DTtimetoa(s2, tcp);
                printf("unexpected ptime: wanted %s got %s\n", s1, s2);
                exp_ptime = *tcp; /* Re-sync expected time */
            }
            ptime_valid = 1;
        } else {
            printf("invalid ptime\n");
            ptime_synced = 0;
            ptime_valid = 0;
        }

        /* Absolute Time (atime) */
        tpp = (struct dttimepack *)&frame.subcode.packs[DTP_ATIME-1];
        tcp = &(tpp->tc);
        if ((i2 = DTbcdtodec(tpp->index.dhi, tpp->index.dlo)) < 100) {
            local_idx = i2;
        }

        atime_valid = 0;
        if (DTtcvalid(tcp)) {
            if (!atime_synced) {
                exp_atime = *tcp;
                atime_synced = 1;
            } else if (memcmp(tcp, &exp_atime, sizeof(struct dttimecode)) != 0) {
                DTtimetoa(s1, &exp_atime);
                DTtimetoa(s2, tcp);
                printf("unexpected atime: wanted %s got %s\n", s1, s2);
                exp_atime = *tcp; /* Re-sync expected time */
            }
            atime_valid = 1;
        } else {
            printf("invalid atime\n");
            atime_synced = 0;
            atime_valid = 0;
        }

        fflush(stdout);
        if (i1 != i2) {
            printf("index numbers mismatch between atime and ptime\n");
        }

        /* Increment via libdataudio standard */
        if (atime_valid) {
            DTinctime(&exp_atime);
        }
        
        if (local_idx != 0 && ptime_valid) {
            DTinctime(&exp_ptime);
        }
    }
    return 1;
}
int main(int argc, char **argv) {
    int maj, min;

    /* Check for help flag or invalid arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: %s [-h]\n", argv[0]);
            printf("Verifies subcode program time (ptime) and absolute time (atime) on a DAT tape.\n");
            exit(0);
        } else {
            fprintf(stderr, "Invalid option: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [-h]\n", argv[0]);
            exit(1);
        }
    }

    (void)mediad_get_exclusiveuse(datdev, argv[0]);

    dat = open(datdev, O_RDONLY);
    if (dat < 0) {
        perror("Could not open DAT drive:");
        exit(-1);
    }

    if (get_firmware_revision(dat, &maj, &min) < 0) {
        fprintf(stderr, "Couldn't get DAT firmware revision\n");
        exit(-1);
    }

    if (maj < 2 || (maj == 2 && min < 63)) {
        fprintf(stderr, "DAT firmware rev %d.%d is too old -- you must have 2.63 or greater\n", maj, min);
        exit(-1);
    }

    if (prepare_dat(dat) < 0) exit(-1);
    if (rewind_dat(dat) < 0) exit(-1);
    if (verify_dat(dat) < 0) exit(-1);

    return 0;
}
