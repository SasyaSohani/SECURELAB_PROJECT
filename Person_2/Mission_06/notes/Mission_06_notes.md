# Mission 6 — Two Processes Need to Talk
Person 2 (Srujana) · Ubuntu 26.04.1 LTS VM, x86_64, 2 vCPUs · gcc 15.2.0

## Question
When two processes exchange data, how does the OS move it, and who else on the machine can reach the communication endpoint?

## Prediction
A pipe copies data through the kernel (write in, read out) and has no name, so only the parent and child can use it. Shared memory maps the same pages into both processes, so no read/write is needed, but the reader needs a semaphore to know when the data is ready. A named shared-memory object lives in /dev/shm with file permissions, so with 0666 another user should be able to read it and with 0600 they should be denied.

## Experiment
| Step | Command | Evidence |
|---|---|---|
| Pipe IPC | `./bin/pipe_ipc` | 01_pipe_output.txt |
| Pipe syscalls | `strace -f -e trace=pipe2,clone,write,read ./bin/pipe_ipc` | 02_pipe_strace.txt |
| Pipe endpoint | `sleep 3 \| sleep 3` + `ls -l /proc/<PID>/fd` | 03_pipe_fds.txt |
| Shared memory | `./bin/shm_ipc secure` | 04_shm_output.txt |
| SHM syscalls | `strace -f -e trace=openat,mmap,futex,unlink ./bin/shm_ipc secure` | 05_shm_strace.txt |
| No synchronisation | `./bin/shm_ipc nosync` x3 | 06_shm_nosync.txt |
| Untrusted user | `shm_ipc insecure/secure 4` + `sudo -u nobody shm_snoop` | 07_security_test.txt |

## Observation
- Pipe: `Child received: Hello from the parent`. strace showed `pipe2([3, 4])`, then the parent `write(4, "Hello from the parent\0", 22)` and the child `read(3, "Hello from the parent\0", 127)`: the message was copied into and out of the kernel.
- Pipe endpoint: writer PID had `1 -> pipe:[39334]`, reader PID had `0 -> pipe:[39334]`. Same pipe, no file path.
- Shared memory: `Child received: "Hello from the parent via shared memory [token=SL-7881]"`. strace showed `openat("/dev/shm/securelab_m6", O_RDWR|O_CREAT|O_EXCL…, 0600)`, `mmap(… MAP_SHARED …)`, then only `futex` calls (the semaphore). No read/write carried the message.
- Without the semaphore, the child printed an EMPTY message in 3 of 3 runs: it read before the parent wrote.
- Untrusted user `nobody` (uid 65534):
  - insecure, mode `-rw-rw-rw-` → `[snoop] LEAKED: "Hello from the parent via shared memory [token=SL-7963]"`
  - secure, mode `-rw-------` → `[snoop] DENIED: Permission denied`, while the real child still received its message.

## Concept
IPC deliberately crosses the isolation between processes, so it creates a trust boundary. The question is who else can reach it.
- Pipe (message passing): the kernel copies the data, keeps it in order, and blocks the reader until data arrives. Access comes only from inheriting the file descriptor, which works like a capability.
- Shared memory: the kernel only sets up the mapping; after that, processes use the memory directly. Access to a named object is controlled by DAC file permissions on /dev/shm/<name>. Ordering is the programmer's job.

## Defense
1. Owner-only permissions (0600) on the IPC endpoint.
2. O_CREAT|O_EXCL: never reuse an object someone else may have created first (name squatting).
3. A process-shared semaphore so readers never see half-written data.
4. shm_unlink() as soon as possible, or use anonymous shared memory / memfd so there is no global name at all.
5. For sockets: check the peer's identity (SO_PEERCRED).

**Proposed secure-IPC control:** create IPC endpoints owner-only (0600) and exclusively (O_EXCL), synchronise with a semaphore, and unlink the name as soon as both sides are attached.

## Verification
Same program, same message, same untrusted user. Changing 0666 → 0600 turned LEAKED into DENIED: Permission denied, and the legitimate child still worked. Adding the semaphore turned 3/3 empty reads into correct reads.

## Reflection: what if the control were missing?
With a world-readable endpoint, any local user or compromised service can read secrets passed between processes (tokens, passwords), breaking confidentiality. If world-writable, they can inject or change messages, breaking integrity. Without synchronisation, readers act on half-written data, which is a race condition like Mission 5.

## Message passing vs shared memory
| Property | Pipe | POSIX shared memory |
|---|---|---|
| How data moves | write() copies into kernel, read() copies out | both processes mmap the same pages |
| Speed | 2 copies + 2 syscalls per message | no copies after setup (fastest) |
| Synchronisation | built in (read blocks, FIFO order) | none: needs a semaphore/mutex |
| Direction | one-way | both can read and write |
| Endpoint | anonymous pipe:[inode], no path | named /dev/shm/<name>, visible system-wide |
| Who can connect | only processes that inherited the fd | anyone the file permissions allow |
| Main risk | fd leaked to the wrong process | weak permissions, name squatting, races |

## Think about it
**16. Why is a pipe different from shared memory?**
A pipe is a kernel-managed, one-way byte stream: data is copied in and out with system calls, and the kernel handles ordering and blocking. Shared memory is the same physical pages mapped into both address spaces; after setup the kernel is not involved, so it is faster, but the processes must coordinate themselves. A pipe is anonymous and reachable only through inherited fds; a named shared-memory object is reachable by anyone its permissions allow.

**17. Why does shared memory normally require synchronisation?**
Both processes access the same bytes and nothing tells the reader when the writer has finished. Without a semaphore the reader saw an empty message in 3/3 runs, and two writers could overwrite each other (the lost-update race from Mission 5).

**18. What must be protected in secure IPC: the data, the endpoint, or both?**
Both. Protecting the endpoint (permissions, O_EXCL, unlinking, checking the peer) decides who can connect; my test shows that alone decides whether the data leaks. Protecting the data (synchronisation, validating every message, encryption if it crosses machines) matters because even an allowed peer may be buggy or compromised.
