#include <stdio.h>
#include <unistd.h>

static const int BUFFER_SIZE = (4 * 1024 * 1024); // 4 MiB
static const char USAGE[] = "usage: wc <filename>\n";
static const char OUTPUT[] = "  %ld  %ld %ld %s\n";

static short is_word_start(const char ch);

int main(int argc, char *argv[]) {
    ssize_t w_cnt = 0, l_cnt = 0, b_cnt = 0, b_read = 0;
    char buf[BUFFER_SIZE];

    if (argc < 2) {
        printf(USAGE);
        return 2;
    }

    FILE *fp = fopen(argv[1], "r");
    int fd = fileno(fp);

    do {
        b_read = read(fd, buf, BUFFER_SIZE);
        for (ssize_t i = 0; i++ < b_read;) {
            w_cnt += is_word_start(buf[i]);
            l_cnt += buf[i] == '\n';
        }
        b_cnt += b_read;
    } while (b_read > 0);

    fclose(fp);

    printf(OUTPUT, l_cnt, w_cnt, b_cnt, argv[1]);

    return 0;
}

static short is_word_start(const char ch) {
    static short first = 0;
    if (ch == ' ' || ch == '\n') {
        first = 1;
        return 0;
    } else if (first) {
        first = 0;
        return 1;
    } else {
        return 0;
    }
}
