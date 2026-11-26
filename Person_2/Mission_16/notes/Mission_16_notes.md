# Mission 16 — Build Your First Container Sandbox
Person 2 (Srujana) · Ubuntu 26.04.1 LTS VM, x86_64, 2 vCPUs · Docker 29.10.1 (official docker-ce repository)

## Question
When an application runs in a container, what is actually isolated from the host, what is shared, and how can its resource use be limited?

## Prediction
The container should see its own processes, hostname, network and filesystem (namespaces), but share the host's kernel, because a container is not a VM. cgroups should be able to cap its memory, CPU and number of processes, and the kernel should stop it when it goes over a limit.

## Experiment
- Checked Docker was not already in the VM (`docker --version` → not found), took snapshot SECURELAB-PRE-DOCKER, installed Docker Engine from the official repository (docs.docker.com/engine/install/ubuntu), verified with `sudo docker run --rm hello-world`.
- Handout commands: `sudo docker run --rm ubuntu:26.04 uname -a`, `cat /etc/os-release`, `ps`, compared with the host → `evidence/02_container_basics.txt`
- Namespace IDs (`readlink /proc/self/ns/*`), hostname, network interfaces, and one process seen from both sides → `evidence/03_namespaces.txt`
- cgroup limits `--memory 64m --cpus 0.5 --pids-limit 20`, read back and tested → `evidence/04_cgroups.txt`
- Root and capabilities inside the container → `evidence/05_container_root_capabilities.txt`

## Observation
| | Host (VM) | Container |
|---|---|---|
| Kernel (`uname -r`) | 7.0.0-44-generic | **the same 7.0.0-44-generic** |
| OS files | Ubuntu 26.04.1 | Ubuntu 26.04.1 (from the image) |
| Processes visible | 232 | only itself (PID 1) |
| Hostname | SECURELAB-Ubuntu | 012afa4a38df (container ID) |
| Network | docker0, enp0s3, lo | eth0, lo (`--network none`: only lo) |
| `sleep 60` process | PID 10700, user root | PID 1 |
| Namespaces pid/net/mnt/uts/ipc/cgroup | e.g. pid:[4026531836] | different, e.g. pid:[4026532705] |
| User namespace | user:[4026531837] | **the same** user:[4026531837] |

cgroups (cgroup v2):
- Inside the container: `memory.max = 67108864`, `cpu.max = 50000 100000`, `pids.max = 20`.
- Starting 40 processes under `--pids-limit 20`: `sh: 0: Cannot fork`. The kernel refused new processes at the limit.
- Using 200 MB under `--memory 64m`: `Killed`, exit code 137 (OOM killer). The same command with no limit: exit code 0.

Capabilities:
- Container user is `uid=0(root)`, but its CapEff is `00000000a80425fb` (14 capabilities: chown, dac_override, fowner, fsetid, kill, setgid, setuid, setpcap, net_bind_service, net_raw, sys_chroot, mknod, audit_write, setfcap). Real root on the host has `000001ffffffffff` (all).
- `hostname hacked` inside the container was refused (exit 1); it needs CAP_SYS_ADMIN, which Docker removes.

## Architecture: host vs container

    +---------------------------------------------------------------+
    | Windows host + VirtualBox (hypervisor)                        |
    |  +---------------------------------------------------------+  |
    |  | SECURELAB-Ubuntu VM: Ubuntu 26.04 guest                 |  |
    |  |                                                         |  |
    |  |   host processes     +------------------------------+   |  |
    |  |   (232 visible:      | Container: ubuntu:26.04      |   |  |
    |  |    gnome, sshd,      |  own PID ns   (sleep = PID 1)|   |  |
    |  |    dockerd, ...)     |  own NET ns   (eth0, lo)     |   |  |
    |  |                      |  own MNT ns   (image files)  |   |  |
    |  |                      |  own UTS ns   (012afa4a38df) |   |  |
    |  |                      |  cgroup: 64 MB, 0.5 CPU,     |   |  |
    |  |                      |          20 pids             |   |  |
    |  |                      |  root, 14 capabilities       |   |  |
    |  |                      +------------------------------+   |  |
    |  |   dockerd / containerd / runc (create namespaces+cgroups)|  |
    |  | ======================================================= |  |
    |  |   ONE shared Linux kernel 7.0.0-44-generic              |  |
    |  |   (namespaces, cgroups, capabilities, seccomp)          |  |
    |  +---------------------------------------------------------+  |
    |   virtual hardware: 2 vCPU, RAM, disk, NAT network (enp0s3)   |
    +---------------------------------------------------------------+

The container is an ordinary host process (PID 10700) that the kernel shows a restricted view of the system. The VM is a separate guest kernel running on virtual hardware.

## Concept
Containers = OS-level virtualisation. **Namespaces** give a process its own view of PIDs, network, mounts, hostname, IPC and cgroups. **cgroups** limit how much CPU, memory and how many processes it can use. **Capabilities** split root's power into pieces so container root gets only a few. All containers share the host kernel.

## Defense
- Namespaces isolate what the container can see.
- cgroups (`--memory`, `--cpus`, `--pids-limit`) stop one container from exhausting the host (memory hog → OOM-killed; fork bomb → Cannot fork).
- Docker drops most capabilities by default; more hardening: `--cap-drop ALL`, `--read-only`, `--network none`, `--security-opt no-new-privileges`, run as a non-root user (`--user`), user namespaces (rootless Docker), AppArmor/seccomp profiles.
- Docker access is root-equivalent: only trusted users should be allowed to run it (the handout's safety note).

## Verification
- Isolation: different namespace IDs, PID 1 inside vs PID 10700 on the host, separate hostname and network.
- Limits: 64 MB limit → exit 137, no limit → exit 0; pids limit → Cannot fork; limit values read back from /sys/fs/cgroup.
- Reduced root: 14 vs all capabilities; hostname change refused.

## Reflection: what if these controls were missing?
Without namespaces, an app would see and could signal every host process and use the host network directly. Without cgroups, one buggy or malicious container could use all memory, CPU or process slots and take down the host and every other container (availability). Without capability dropping, container root would be full host root. And because the kernel is shared, a kernel bug can break out of every container at once; a VM has a stronger boundary.

## VM vs container
| | Virtual machine (my SECURELAB VM) | Container (Docker) |
|---|---|---|
| Kernel | its own guest kernel | shares the host kernel (same 7.0.0-44) |
| Isolation by | hypervisor + virtual hardware | namespaces + cgroups + capabilities |
| Boundary strength | strong (a separate OS) | weaker: one kernel bug affects all containers |
| Start time | minutes (full OS boot) | under a second |
| Size | GBs (full OS) | MBs (ubuntu image ≈ 30 MB compressed) |
| Overhead | high (6 GB RAM reserved) | low (just a process) |
| Visible from host as | the VirtualBox process | ordinary processes (sleep = PID 10700) |
| Run a different OS | yes (Linux on Windows) | only Linux on a Linux kernel |
| Best for | strong isolation, different OS, labs | packaging and isolating apps on one host |

## Think about it
**47. Why does the container have its own process view?**
Docker starts it in a new PID namespace. The kernel numbers processes separately inside it, so the first process is PID 1 and processes outside are invisible. The same `sleep` was PID 1 inside and PID 10700 on the host: one process, two views.

**48. Is a container the same thing as a virtual machine?**
No. A VM runs its own kernel on virtual hardware provided by a hypervisor. A container is a group of host processes with a restricted view, sharing the host kernel. My container showed the same kernel (7.0.0-44-generic) as the VM. Containers are lighter and faster; VMs have a stronger isolation boundary.

**49. Which resources can cgroups control?**
Memory (memory.max: my 64 MB limit caused an OOM kill), CPU time (cpu.max: 50000/100000 = half a CPU), number of processes/threads (pids.max: fork refused at 20), and also block I/O bandwidth (io.max), CPU and memory-node pinning (cpuset), and hugepages.
