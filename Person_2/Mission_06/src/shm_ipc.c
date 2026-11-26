/* Usage: shm_ipc <secure|insecure|nosync> [hold_seconds]
 *  secure   : permissions 0600 (owner only) + O_EXCL + semaphore
 *  insecure : permissions 0666 (anyone can read/write)
 *  nosync   : secure permissions, but the child does NOT wait for the semaphore */
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "shm_common.h"

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: %s <secure|insecure|nosync> [hold]\n", argv[0]); return 2; }
    const char *mode = argv[1];
    int hold     = (argc > 2) ? atoi(argv[2]) : 0;
    int secure   = strcmp(mode, "insecure") != 0;
    int use_sync = strcmp(mode, "nosync") != 0;

    int fd;
    if (secure) {
        fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0600);   /* owner only, must be new */
    } else {
        umask(0);
        fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);            /* everyone can access */
    }
    if (fd == -1) { fprintf(stderr, "shm_open failed: %s\n", strerror(errno)); return 1; }

    ftruncate(fd, sizeof(struct shared_box));
    struct shared_box *box = mmap(NULL, sizeof *box, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (box == MAP_FAILED) { perror("mmap"); shm_unlink(SHM_NAME); return 1; }
    close(fd);
    memset(box, 0, sizeof *box);
    sem_init(&box->ready, 1, 0);                 /* 1 = shared between processes */

    struct stat st;
    if (stat("/dev/shm" SHM_NAME, &st) == 0)
        printf("[parent] created /dev/shm%s with mode %04o (mode=%s)\n", SHM_NAME, st.st_mode & 07777, mode);
    fflush(stdout);

    pid_t pid = fork();
    if (pid == 0) {                              /* CHILD = reader */
        if (use_sync) sem_wait(&box->ready);     /* wait for "ready" */
        printf("Child received: \"%s\"%s\n", box->msg,
               box->msg[0] ? "" : "   <-- EMPTY: read before the write (no synchronisation)");
        return 0;
    }

    usleep(100 * 1000);                          /* parent is "busy" for 0.1 s */
    snprintf(box->msg, MSG_LEN, "Hello from the parent via shared memory [token=SL-%d]", getpid());
    if (use_sync) sem_post(&box->ready);         /* tell the child: ready */
    waitpid(pid, NULL, 0);

    if (hold > 0) {
        printf("[parent] keeping it alive for %d s so another user can try to read it...\n", hold);
        fflush(stdout);
        sleep(hold);
    }
    sem_destroy(&box->ready);
    munmap(box, sizeof *box);
    shm_unlink(SHM_NAME);                        /* remove the name from /dev/shm */
    printf("[parent] unlinked %s\n", SHM_NAME);
    return 0;
}
