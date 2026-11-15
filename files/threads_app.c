/*
 * threads_app.c - two threads running two different functions.
 * Thread 2 blocks (pthread_join) until thread 1 has terminated.
 *
 * Cross-compile:  arm-linux-gnueabi-gcc -Wall -Wextra -O2 -pthread -o threads_app threads_app.c
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Thread 2 needs thread 1's id to wait for it. It is written by pthread_create()
 * before thread 2 is even created, so there is no race on this variable. */
static pthread_t thread1;

/* Function run by thread 1 */
static void *task1(void *arg)
{
    (void)arg;
    for (int i = 1; i <= 5; i++) {
        printf("[thread 1] working... step %d/5\n", i);
        sleep(1);
    }
    printf("[thread 1] finished -> terminating\n");
    return NULL;
}

/* Function run by thread 2 (a different function) */
static void *task2(void *arg)
{
    (void)arg;
    printf("[thread 2] started, BLOCKED until thread 1 terminates...\n");

    int err = pthread_join(thread1, NULL);          /* <-- the blocking point */
    if (err != 0) {
        fprintf(stderr, "[thread 2] pthread_join: %s\n", strerror(err));
        return NULL;
    }

    printf("[thread 2] thread 1 has terminated -> thread 2 unblocked\n");
    for (int i = 1; i <= 3; i++) {
        printf("[thread 2] working... step %d/3\n", i);
        sleep(1);
    }
    printf("[thread 2] finished -> terminating\n");
    return NULL;
}

int main(void)
{
    /* Unbuffered output so every line appears immediately on the serial console */
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== threads_app: 2-thread demo (pid %d) ===\n", (int)getpid());

    int err = pthread_create(&thread1, NULL, task1, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_create(thread1): %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    pthread_t thread2;
    err = pthread_create(&thread2, NULL, task2, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_create(thread2): %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    /* main waits only for thread 2. Thread 1 is joined by thread 2
     * (a thread may be joined exactly once). */
    pthread_join(thread2, NULL);

    printf("=== threads_app: all threads finished ===\n");
    return EXIT_SUCCESS;
}
