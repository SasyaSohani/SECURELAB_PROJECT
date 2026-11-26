#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "shm_common.h"

int main(void) {
    printf("[snoop] running as uid=%d, trying to open %s\n", getuid(), SHM_NAME);
    int fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (fd == -1) { printf("[snoop] DENIED: %s\n", strerror(errno)); return 1; }
    struct shared_box *box = mmap(NULL, sizeof *box, PROT_READ, MAP_SHARED, fd, 0);
    if (box == MAP_FAILED) { printf("[snoop] DENIED (mmap): %s\n", strerror(errno)); return 1; }
    printf("[snoop] LEAKED: \"%.*s\"\n", MSG_LEN, box->msg);
    return 0;
}
