#include <pthread.h>
#include <stdio.h>

long counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    (void)arg;

    for (long i = 0; i < 1000000; i++) {
        pthread_mutex_lock(&lock);      /* TODO 2: lock before touching shared data */

        /* >>>>>>>>>> CRITICAL SECTION START >>>>>>>>>> */
        counter++;                      /* TODO 3: only one thread can be here at a time */
        /* <<<<<<<<<< CRITICAL SECTION END <<<<<<<<<<<< */

        pthread_mutex_unlock(&lock);    /* TODO 4: let the other thread in */
    }

    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Expected: 2000000\n");
    printf("Actual : %ld\n", counter);

    pthread_mutex_destroy(&lock);
    return 0;
}
