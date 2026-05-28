# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project context

This is a university OS course-design submission (2026 春《操作系统》课设, 计算机科学与技术 1 班, 教师杨灿). Scope:
- **Basic (required, 70%)** — all 4 items: scheduler, memory, sync, filesystem
- **Extensions (30%)** — directions (1) Linux kernel/system programming and (2) scheduling & performance
- **Deadline**: 2026-06-11. Submission = PDF/Word report + GitHub URL written into the report + classroom-wide CD burn.

The task assignment lives at `操作系统课程设计内容与要求1.pdf` in the repo root — treat it as the source of truth for grading criteria. The full plan and decisions are in `README.md` and `docs/00-setup-guide.md`.

## Build & run

Each `NN-topic/` (and `extN-topic/`) is an **independent C project with its own Makefile**. There is no top-level build — work inside the subdirectory.

Currently implemented:
```bash
cd 01-scheduler
make                                              # build ./scheduler
./scheduler                                       # interactive menu
./scheduler -f cases/case1.txt -a all             # batch run ALL 5 algorithms on a case
./scheduler -f cases/case1.txt -a rr -q 2         # single algorithm with explicit quantum
make test                                         # runs each algorithm on case1.txt in sequence
make clean
```

Algorithm flags: `fcfs | sjf | srtf | rr | prio | all`. Test cases live in `01-scheduler/cases/*.txt`; format is `name arrival burst priority` per line, `#` starts a comment.

The other topic dirs (`02-memory`, `03-sync`, `04-filesystem`, `ext1-kernel`, `ext2-sched-perf`) are scaffolds — when implementing them, follow the same `src/ + cases/ + Makefile` layout as `01-scheduler`.

## Where work actually runs

The host is Windows 11 but **the code is meant to be compiled and run inside an Ubuntu 20.04 VM** (VMware Workstation). The typical loop:
1. Edit on Windows or via VS Code Remote-SSH into the VM (`ssh -p 2222 osuser@127.0.0.1`, NAT port-forward)
2. Build & run inside the VM (`gcc 9.x`, `pthread`, `linux-headers-$(uname -r)`)
3. Anything touching kernel modules (`ext1-kernel/`) requires a VMware snapshot first — `insmod` can hang the VM, and rollback is faster than rebuild

Setup details: `docs/00-setup-guide.md`. Do not assume the dev host can build kernel code or run pthread programs natively — Windows can only edit/commit.

## Shell on this host

Bash on Windows: use **Unix syntax** (forward slashes, `/dev/null`, not `NUL`). Paths in tool calls still use Windows drive letters (`D:/work/04/...`). This trips people up when copying commands between docs and the live shell.

## Architecture conventions

- **Language**: C11 for everything in the basic 4 + ext1 driver. Python (matplotlib) only allowed in `ext2-sched-perf/` for plotting; do not introduce other languages.
- **One topic = one self-contained subdir** with its own `src/`, `cases/`, `Makefile`, and `README.md`. Topics never import code from each other — even shared utilities should be duplicated rather than promoted to a top-level lib (course grader sees each topic standalone).
- **Scheduler simulator pattern** (`01-scheduler/src/scheduler.c`): single binary, algorithms dispatched by `-a` flag, non-preemptive algorithms share `run_nonpreemptive()` parameterized by a `Picker` function. When adding new algorithms (e.g. MLFQ in ext2), reuse this pattern rather than forking a new binary.
- **ext2 reuses ext1's simulator + adds real-Linux measurement** (`sched_setscheduler`, `perf`, `sysbench`). It should not duplicate the scheduler core.

## Submission constraint that affects code

Every commit/PR/README must keep the **GitHub URL discoverable from the report**. When making changes, keep the top-level `README.md` and per-topic READMEs accurate enough that a grader cloning the repo can `make && ./run-something` per topic without asking questions.

## Persistent memory

This project has long-term memory at `C:\Users\王振东\.claude\projects\D--work-04\memory\` (`MEMORY.md` is the index). Already recorded: project scope/deadline, environment paths, technology decisions (Ubuntu 20.04, kernel depth = module+chrdev+/proc, no syscall-table edits, no kernel rebuild). Read these before re-asking the user about choices that were already settled.
