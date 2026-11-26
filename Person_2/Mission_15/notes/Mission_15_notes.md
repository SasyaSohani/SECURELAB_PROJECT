# Mission 15 — The Suspicious Application
Person 2 (Srujana) · Ubuntu 26.04.1 LTS VM, x86_64, 2 vCPUs · gcc 15.2.0

## Question
What resources can an application actually reach, which of them does it really need, and can the OS deny the rest without breaking it?

## Prediction
A "focus timer" should only need its own log file and a clock. If the program touches the network or system/user files, inspection with /proc, ss and strace should reveal it, and a sandbox should be able to block those extras while the timer keeps working.

## Experiment
Test app: `src/suspicious_app.c` ("StudyTimer"), written for this lab. Its real job is a tick every second plus a session log. It also contains 4 deliberate, harmless extras: a TCP listener on 0.0.0.0:4444, opening /etc/passwd, probing ~/.ssh, and an idle extra thread.
- Inspection while running (handout commands): `ps -p <PID> -o pid,ppid,uid,euid,nlwp,%cpu,%mem,cmd`, `ls -l /proc/<PID>/fd`, `grep -E 'Name|Uid|Gid|Threads' /proc/<PID>/status`, `ls /proc/<PID>/task`, `ss -lntup` → `evidence/02_inspection.txt`
- System-call trace: `strace -f -e trace=openat,access,socket,bind,listen` → `evidence/03_strace.txt`
- Sandbox test: the same app run with `systemd-run`, first without restrictions and then with the policy below; scored with `systemd-analyze security` → `evidence/04_sandbox_test.txt`
- ~/.ssh check inside the sandbox → `evidence/05_ssh_blocked.txt`

## Observation
- PID 8107 ran as uid/euid 1000 (not root) with NLWP 3 / Threads: 3 (tasks 8107, 8109, 8110).
- Open fds: 0,2 → /dev/pts/0; 1 → output file; **3 → /tmp/m15/studytimer.log**; **4 → /etc/passwd**; **5 → socket:[40470]**.
- `ss -lntup`: `LISTEN 0.0.0.0:4444 users:(("suspicious_app",pid=8107,fd=5))`, listening on every network interface.
- strace: `openat("/etc/passwd", O_RDONLY) = 4`, `access("/home/srujana/.ssh", F_OK) = 0`, `socket(AF_INET, SOCK_STREAM)`, `bind(… htons(4444) … "0.0.0.0")`, `listen(5, 4)`.

### Application resource table
| Resource | Observed | Needed for the timer? |
|---|---|---|
| Log file /tmp/m15/studytimer.log (write) | fd 3 | ✅ Yes |
| Terminal stdout/stderr | fd 0–2 | ✅ Yes |
| Clock / sleep | tick every second | ✅ Yes |
| /etc/passwd (read) | fd 4 | ❌ No |
| ~/.ssh (probe) | access() = 0 | ❌ No |
| TCP socket, LISTEN 0.0.0.0:4444 | fd 5, ss | ❌ No |
| Extra threads | 3 threads | ❌ Only 1 needed |
| Root privileges / capabilities | runs as uid 1000 | ❌ No |

### Sandbox result
| | Baseline (no policy) | Sandboxed |
|---|---|---|
| systemd-analyze exposure | **9.2 UNSAFE** | **4.9 OK** |
| /etc/passwd | opened (fd 4) | `blocked: Permission denied` |
| Network | listening on 0.0.0.0:4444 | `socket() blocked: Address family not supported by protocol`; no listener on 4444 |
| Capabilities | — | CapEff/CapBnd = 0000000000000000, NoNewPrivs = 1 |
| ~/.ssh | `exists`, listable | existence still visible, but `ls`: **Permission denied** |
| Timer | tick 8/8, complete | **tick 8/8, complete** (still works) |

## Concept
Sandboxing, least privilege and attack surface. By default a process gets everything its user can reach: all of the user's files, the network, many system files. The attack surface is everything a compromised app could use. Inspection (/proc, ss, strace) shows what an app really touches, so access can be cut down to what its purpose needs.

## Defense: proposed sandbox policy
Applied with systemd unit settings (they use Linux namespaces, seccomp, capabilities and cgroups):
| Restriction | Setting |
|---|---|
| 1. No network | `PrivateNetwork=yes`, `RestrictAddressFamilies=AF_UNIX` |
| 2. No access to the user database | `InaccessiblePaths=/etc/passwd` |
| 3. No access to SSH keys | `InaccessiblePaths=-~/.ssh` |
| 4. Read-only system and home | `ProtectSystem=strict`, `ProtectHome=read-only` |
| 5. Write only to the log folder | `ReadWritePaths=/tmp/m15` |
| 6. No privileges | `NoNewPrivileges=yes`, `CapabilityBoundingSet=` (empty) |
| 7. Resource limits | `TasksMax=8`, `MemoryMax=64M` |

## Verification
Same binary, same arguments. With the policy, all three unneeded resources were blocked (network, /etc/passwd, ~/.ssh contents), capabilities were 0 and NoNewPrivs was 1, the exposure score dropped from 9.2 to 4.9, and the timer still finished tick 8/8. The denied resources were never needed.
Limitation: InaccessiblePaths makes ~/.ssh unreadable but does not hide that it exists (access(F_OK) still returned 0). Its contents were protected (`Permission denied`).

## Reflection: what if this control were missing?
If the app were malicious or got compromised, it could use everything its user can: read SSH keys and personal files, read the user list, and open a network port that anyone on the network can connect to (a backdoor or a way to send data out). A sandbox limits the damage to what the app legitimately needs.

## Think about it
**44. Which resources does your chosen application actually need?**
Its log file (/tmp/m15/studytimer.log, write), the terminal for output, the clock/sleep, and one thread.

**45. Which resources could be denied without breaking its purpose?**
The network (TCP listener on 0.0.0.0:4444), /etc/passwd, ~/.ssh, write access to the rest of the system and home, all capabilities, and the extra threads. I denied the first three plus privileges, and the timer still completed tick 8/8.

**46. How does least privilege reduce attack surface?**
Every resource a process can reach is something an attacker can use once the process is compromised. Giving the app only what its job needs removes the rest from reach: here the open network port disappeared, secret files became unreadable, and the exposure score fell from 9.2 UNSAFE to 4.9 OK. A bug in the app can now do much less damage.
