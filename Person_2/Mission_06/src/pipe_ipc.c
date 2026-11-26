#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];
    if (pipe(fd) == -1) {               /* kernel creates an anonymous one-way channel */
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();                 /* only parent + child inherit the pipe ends */
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {                     /* CHILD = reader */
        close(fd[1]);
        char buffer[128] = {0};
        ssize_t n = read(fd[0], buffer, sizeof(buffer) - 1);   /* waits until data arrives */
        if (n >= 0)
            printf("Child received: %s\n", buffer);
        close(fd[0]);
        return 0;
    }

    close(fd[0]);                       /* PARENT = writer */
    const char msg[] = "Hello from the parent";
    if (write(fd[1], msg, strlen(msg) + 1) == -1)   /* message is copied into the kernel */
        perror("write");
    close(fd[1]);
    waitpid(pid, NULL, 0);
    return 0;
}
