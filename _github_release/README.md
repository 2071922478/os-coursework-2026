# 操作系统课程设计 2026

> 2026 春《操作系统》课程设计 · 计算机科学与技术 1 班 · 授课教师：杨灿
> 截止：**2026-06-11**

## 任务完成情况

| 编号 | 题目 | 内容 | 状态 |
|------|------|------|:--:|
| 基础(1) | 处理机调度 | FCFS / SJF / SRTF / RR / Priority 五种算法模拟器 | ✅ |
| 基础(2) | 内存管理 | 动态分区分配 (FF/BF/WF) + 页面置换 (FIFO/LRU/OPT) | ✅ |
| 基础(3) | 进程同步 | 生产者-消费者 / 读者-写者 / 哲学家就餐 (pthread) | ✅ |
| 基础(4) | 文件系统 | 用户态模拟磁盘 (超级块+i节点+位图+shell交互) | ✅ |
| 扩展(1) | Linux 内核编程 | 字符设备驱动 (/dev/oslab_chrdev) + /proc 接口 | ✅ |
| 扩展(2) | 调度与性能 | MLFQ/CFS 模拟器 + 实时调度 demo + perf 实测出图 | ✅ |

## 目录结构

```
├── 01-scheduler/         基础(1) 处理机调度
│   ├── src/scheduler.c      五种算法: FCFS/SJF/SRTF/RR/Priority
│   ├── cases/case1.txt      测试用例1 (4进程)
│   ├── cases/case2.txt      测试用例2 (7进程)
│   ├── Makefile
│   └── README.md
│
├── 02-memory/            基础(2) 内存管理
│   ├── partition/           动态分区分配
│   │   ├── src/partition.c    FF/BF/WF 三种适配 + compact紧凑
│   │   ├── cases/case1.txt    测试用例1
│   │   ├── cases/case2.txt    测试用例2 (含compact)
│   │   └── Makefile
│   └── paging/              页面置换
│       ├── src/paging.c       FIFO/LRU/OPT, 逐帧显示状态
│       ├── cases/case1.txt    经典引用串
│       ├── cases/case2.txt    Belady异常演示
│       └── Makefile
│
├── 03-sync/              基础(3) 进程同步
│   ├── src/producer_consumer.c     多生产者-多消费者 (POSIX semaphore)
│   ├── src/reader_writer.c         读者-写者 (读者优先/写者优先)
│   ├── src/dining_philosophers.c   哲学家就餐 (资源分级法防死锁)
│   ├── Makefile
│   └── README.md
│
├── 04-filesystem/        基础(4) 文件系统
│   ├── src/filesystem.c       用户态模拟文件系统
│   │                           磁盘布局: 超级块 + inode表 + 位图 + 数据块
│   │                           Shell: mkdir/ls/create/write/cat/rm/df/format
│   ├── cases/test_script.txt  批量测试脚本
│   ├── Makefile
│   └── README.md
│
├── ext1-kernel/          扩展(1) Linux 内核编程
│   ├── src/oslab_chrdev.c     字符设备驱动 (4KB环形缓冲)
│   │                           /proc/oslab_info 统计接口
│   ├── load.sh                加载脚本 (需要root)
│   ├── unload.sh              卸载脚本
│   ├── Makefile
│   └── README.md
│
├── ext2-sched-perf/      扩展(2) 调度与性能优化
│   ├── src/mlfq_cfs.c          MLFQ (3级反馈队列) + CFS (vruntime)
│   ├── src/realtime_demo.c     实时调度 (SCHED_FIFO/SCHED_RR)
│   ├── scripts/perf_measure.py 性能实测 + matplotlib出图
│   ├── cases/mlfq_case.txt     MLFQ/CFS测试用例
│   ├── Makefile
│   └── README.md
│
├── docs/
│   ├── 00-setup-guide.md      VMware + Ubuntu 20.04 环境搭建手册
│   ├── RUN-GUIDE.md           运行指南与截图清单
│   └── REPORT.md              实验报告
│
└── README.md               ← 本文件
```

## 运行环境

- **宿主机**: Windows 11
- **虚拟机**: VMware Workstation + Ubuntu 20.04 LTS (amd64)
- **工具链**: GCC 9.x / GNU Make / GDB / pthread / POSIX semaphore
- **扩展(1)**: 需要 `linux-headers-$(uname -r)`，操作前拍VM快照
- **扩展(2)**: Python 3 + matplotlib + sysbench
- 详见 `docs/00-setup-guide.md`

## 快速运行

```bash
# 基础(1) — 处理机调度
cd 01-scheduler && make && ./scheduler -f cases/case1.txt -a all

# 基础(2) — 动态分区分配
cd 02-memory/partition && make && ./partition -a ff -f cases/case1.txt

# 基础(2) — 页面置换
cd 02-memory/paging && make && ./paging -f cases/case1.txt -n 3

# 基础(3) — 进程同步
cd 03-sync && make && ./producer_consumer 4 3 10 15

# 基础(4) — 文件系统
cd 04-filesystem && make && ./filesystem -f cases/test_script.txt

# 扩展(1) — Linux内核 (需root，先拍VM快照!)
cd ext1-kernel && make && sudo ./load.sh

# 扩展(2) — 调度性能
cd ext2-sched-perf && make && ./mlfq_cfs -f cases/mlfq_case.txt -a all
```

详细运行说明与截图清单见 `docs/RUN-GUIDE.md`。
