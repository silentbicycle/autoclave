#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>

void crash(int arg) {
    (void)arg;
    pid_t pid = getpid();
    int v = rand() % 100;
    /* Print this, so there's some output in the logs */
    printf("%llu: %d\n", (unsigned long long)pid, v);
    if (v == 1) {
        abort();
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int i = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    for (i = 0; i < 10; i++) {
        crash(i);
    }
    return 0;
}
