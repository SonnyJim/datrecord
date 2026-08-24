#include "datrecord.h"

static int is_wav_file(const char *filename) {
    size_t len = strlen(filename);
    return (len >= 4 && strcasecmp(filename + len - 4, ".wav") == 0);
}

static int open_tape_audio_device(const char *tape_path) {
    int tape_fd;
    struct mtop op;

    tape_fd = open(tape_path, O_RDWR);
    if (tape_fd < 0) {
        perror("Error opening tape device");
        return -1;
    }

    memset(&op, 0, sizeof(op));
    op.mt_op = MTAUD;
    op.mt_count = 1;

    if (ioctl(tape_fd, MTIOCTOP, &op) < 0) {
        perror("ioctl(MTIOCTOP, MTAUD) failed");
        close(tape_fd);
        return -1;
    }

    return tape_fd;
}

static int process_single_wav(const char *wav_path, const char *tape_path, int pno) {
    int tape_fd;
    int running_frame_count = 0;
    int status;

    tape_fd = open_tape_audio_device(tape_path);
    if (tape_fd < 0) return 1;

    status = record_wav_to_dat(wav_path, tape_fd, pno, &running_frame_count);

    close(tape_fd);
    if (status == 0) {
        printf("\nSingle file recorded successfully. Total audio frames written: %d\n", running_frame_count);
    }
    return status;
}

static int process_playlist(const char *playlist_path, const char *tape_path, int start_pno) {
    FILE *list_fp;
    int tape_fd;
    char line[1024];
    int current_pno = start_pno;
    int running_frame_count = 0;
    int success_count = 0;

    list_fp = fopen(playlist_path, "r");
    if (!list_fp) {
        perror("Error opening playlist file");
        return 1;
    }

    tape_fd = open_tape_audio_device(tape_path);
    if (tape_fd < 0) {
        fclose(list_fp);
        return 1;
    }

    while (fgets(line, sizeof(line), list_fp)) {
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';

        char *path = line;
        while (*path == ' ' || *path == '\t') path++;

        if (*path == '\0' || *path == '#') continue;

        if (record_wav_to_dat(path, tape_fd, current_pno, &running_frame_count) == 0) {
            current_pno++;
            success_count++;
        } else {
            fprintf(stderr, "Recording interrupted on file: %s\n", path);
            break;
        }
    }

    fclose(list_fp);
    close(tape_fd);

    printf("\nBatch process finished. Total tracks recorded: %d (%d frames)\n", 
           success_count, running_frame_count);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *input_path = NULL;
    const char *tape_path = TAPE_DEV_DEFAULT;
    int start_pno = 1;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.wav | playlist.txt> [tape_device] [start_program_number]\n", argv[0]);
        return 1;
    }

    input_path = argv[1];
    if (argc >= 3) tape_path = argv[2];
    if (argc >= 4) start_pno = atoi(argv[3]);

    if (is_wav_file(input_path)) {
        return process_single_wav(input_path, tape_path, start_pno);
    } else {
        return process_playlist(input_path, tape_path, start_pno);
    }
}
