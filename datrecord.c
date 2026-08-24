#include "datrecord.h"

int main(int argc, char *argv[]) {
    const char *wav_path = NULL;
    const char *tape_path = TAPE_DEV_DEFAULT;
    int pno = 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wav> [tape_device] [program_number]\n", argv[0]);
        return 1;
    }

    wav_path = argv[1];
    if (argc >= 3) tape_path = argv[2];
    if (argc >= 4) pno = atoi(argv[3]);

    return record_wav_to_dat(wav_path, tape_path, pno);
}
