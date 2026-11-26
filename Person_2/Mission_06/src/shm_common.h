#ifndef SHM_COMMON_H
#define SHM_COMMON_H
#include <semaphore.h>

#define SHM_NAME "/securelab_m6"     /* appears as /dev/shm/securelab_m6 */
#define MSG_LEN  128

struct shared_box {
    sem_t ready;                     /* semaphore: "message is ready" */
    char  msg[MSG_LEN];              /* data both processes see directly */
};
#endif
