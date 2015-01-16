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

void crash(int arg) {
    (void)arg;
    int v = rand() % 100;
    //printf("v %d\n", v);
    if (v == 1) {
        abort();
    }
}

int main(int argc, char **argv) {
    int i = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    for (i = 0; i < 10; i++) {
        crash(i);
    }
    return 0;
}
