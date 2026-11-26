# Mission 5 — The Race
Person 2 (Srujana) · Ubuntu 26.04.1 LTS VM, x86_64, 2 vCPUs · gcc 15.2.0

## Question
If two threads update the same variable at the same time without coordination, is the result still correct, and why does that matter for security?

## Prediction
Each thread adds 1,000,000, so the correct total is 2,000,000. Without a mutex I expected results below 2,000,000 that change from run to run, because counter++ is really read, add, write, and the threads can interleave. With a mutex I expected exactly 2,000,000 every time, but slower.

## Experiment
- `gcc -Wall -Wextra -O0 -pthread src/race_demo.c -o bin/race_demo`, then 10 runs → `evidence/01_unsync_runs.txt`
- `objdump -d bin/race_demo` to view the machine code of counter++ → `evidence/03_counter_machine_code.txt`
- Implemented the TODOs in `src/race_demo_mutex.c`: `pthread_mutex_lock` → `counter++` → `pthread_mutex_unlock`, then 10 runs → `evidence/04_mutex_runs.txt`
- `time` on both versions → `evidence/05_timing.txt`

## Observation
| Version | Correct runs | Results |
|---|---|---|
| Unsynchronized | 3 / 10 | 1,019,511 to 1,780,207 (up to 980,489 increments lost) |
| Mutex | 10 / 10 | 2,000,000 every run |

- The wrong results were always lower than expected, never higher, and different on every run.
- 3 unsynchronized runs happened to be correct: the threads did not overlap in those runs. The bug depends on timing.
- objdump shows counter++ compiles to three instructions: load counter into a register, add 1, store it back.
- Timing: real time 0.065 s vs 0.083 s; user CPU time 0.003 s vs 0.022 s. The lock and unlock on every increment adds overhead.

## Concept
Race condition on shared state. Threads in one process share one address space, so both see the same `counter`. The read-modify-write of `counter` is the critical section. If thread B reads counter between thread A's read and A's write, both write back the same value and one increment is lost. The output depends on thread scheduling, which the program does not control.

## Defense
Mutual exclusion with a POSIX mutex (`pthread_mutex_lock`/`unlock`): only one thread at a time can be inside the critical section; the other waits. Alternatives: atomic operations (`atomic_fetch_add`), or giving each thread its own counter and adding them up at the end.

## Verification
Same program, same input, same machine. The only change was adding lock/unlock around the marked critical section. Correct runs went from 3/10 to 10/10.

## Reflection: what if the control were missing?
Shared data can be silently corrupted: counters, balances, reference counts, permission flags. Because the bug only shows up under certain timing (3 of my 10 runs looked fine), it can pass testing and fail later. An attacker who can influence timing can trigger the bad case on purpose.

## Think about it
**13. Why can the unsynchronized result be lower than expected?**
counter++ is three steps (load, add, store). When both threads load the same old value, both store old+1, and one increment is overwritten. Lost updates only remove increments, so the result can only be lower.

**14. Which region is the critical section?**
The read-modify-write of the shared variable: the line `counter++` inside the loop in `worker()`. It is marked between CRITICAL SECTION START and END in `race_demo_mutex.c`. The loop variable `i` is local to each thread's own stack, so it does not need protection.

**15. Why can a race condition become a security vulnerability?**
Security checks assume state does not change between the check and the use. Examples: TOCTOU (a program checks a file's permissions, then an attacker swaps it for a symlink to a secret file before it is opened); reference-count races in the kernel that lead to use-after-free and privilege escalation (Dirty COW, CVE-2016-5195, was a race); two simultaneous withdrawals both passing a balance check. Races damage integrity, can bypass authorization, and can crash systems (availability).
