/* Mission 15 — "StudyTimer": advertised as a simple focus timer (tick every second + session log).
 * It ALSO does 4 things its purpose does not need (harmless lab stand-ins):
 *   1. listens on TCP port 4444 on ALL interfaces   2. opens /etc/passwd and keeps it open
 *   3. checks whether ~/.ssh exists                 4. runs an extra idle "updater" thread
 * Each extra fails gracefully, so a sandbox can block it without breaking the timer.
 * Usage: suspicious_app <seconds> <log_directory> */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static volatile int running = 1;

static void *telemetry_thread(void *arg) {          /* NOT NEEDED: network listener */
    (void)arg;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) { printf("[telemetry] socket() blocked: %s\n", strerror(errno)); return NULL; }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(4444),
                             .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(s, (struct sockaddr *)&a, sizeof a) == -1 || listen(s, 4) == -1) {
        printf("[telemetry] bind/listen blocked: %s\n", strerror(errno)); close(s); return NULL;
    }
    printf("[telemetry] listening on 0.0.0.0:4444\n");
    while (running) sleep(1);
    close(s);
    return NULL;
}

static void *updater_thread(void *arg) {            /* NOT NEEDED: idle extra thread */
    (void)arg;
    while (running) sleep(1);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) { fprintf(stderr, "usage: %s <seconds> <log_directory>\n", argv[0]); return 2; }
    int seconds = atoi(argv[1]);
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("[StudyTimer] pid=%d uid=%d starting a %d s focus session\n", getpid(), getuid(), seconds);

    char logpath[512];                              /* NEEDED: session log */
    snprintf(logpath, sizeof logpath, "%s/studytimer.log", argv[2]);
    FILE *log = fopen(logpath, "a");
    if (!log) printf("[log] cannot open %s: %s (timer continues)\n", logpath, strerror(errno));

    int pw = open("/etc/passwd", O_RDONLY);         /* NOT NEEDED */
    if (pw == -1) printf("[extra] /etc/passwd blocked: %s\n", strerror(errno));
    else          printf("[extra] opened /etc/passwd (fd %d) and kept it open\n", pw);

    const char *home = getenv("HOME");              /* NOT NEEDED */
    if (home) {
        char ssh[512];
        snprintf(ssh, sizeof ssh, "%s/.ssh", home);
        printf("[extra] probe %s -> %s\n", ssh, access(ssh, F_OK) == 0 ? "exists" : strerror(errno));
    }

    pthread_t t1, t2;
    pthread_create(&t1, NULL, telemetry_thread, NULL);
    pthread_create(&t2, NULL, updater_thread, NULL);

    for (int i = 1; i <= seconds; i++) {            /* NEEDED: the actual timer */
        sleep(1);
        printf("[StudyTimer] tick %d/%d\n", i, seconds);
        if (log) { fprintf(log, "%ld tick %d\n", (long)time(NULL), i); fflush(log); }
    }
    running = 0;
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    if (log) fclose(log);
    if (pw != -1) close(pw);
    printf("[StudyTimer] session complete\n");
    return 0;
}
