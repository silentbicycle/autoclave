#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

bool deadlock_flag = false;

static void *mainloop(void *arg) {
    pthread_mutex_t *lock = (pthread_mutex_t *)arg;
    pthread_mutex_lock(lock);

    while (deadlock_flag) {
        sleep(1);
    }
    pthread_mutex_unlock(lock);
    return NULL;    
}

int main(int argc, char **argv) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    deadlock_flag = 9 == (rand() % 10);

    pthread_t threads[2];
    
    pthread_mutex_t lock;
    int res = pthread_mutex_init(&lock, NULL);
    assert(0 == res);

    res = pthread_create(&threads[0], NULL, mainloop, &lock);
    assert(0 == res);
    res = pthread_create(&threads[1], NULL, mainloop, &lock);
    assert(0 == res);

    for (int i = 0; i < 2; i++) {
        void *value = NULL;
        res = pthread_join(threads[i], &value);
        (void)value;
    }

    return 0;
}
