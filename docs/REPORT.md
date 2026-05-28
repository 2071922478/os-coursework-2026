# 操作系统课程实验报告

**华南师范大学 计算机学院**

| 项目 | 信息 |
|------|------|
| 实验项目 | 操作系统课程设计 |
| 姓名 | XXX |
| 学号 | XXX |
| 班级 | 计算机科学与技术 1 班 |
| 指导教师 | 杨灿 |
| 提交日期 | 2026 年 6 月 |

---

## 一、实验目的及要求

### 实验目的

通过本次操作系统课程设计，深入理解操作系统的核心概念与实现机制，包括：
1. 处理机调度算法的原理与性能差异
2. 内存管理中的分区分配与页面置换策略
3. 进程同步与并发控制的经典问题
4. 文件系统的内部结构与实现方法
5. Linux 内核模块编程与 /proc 接口
6. 实时调度与性能分析

### 实验要求

1. 用 C 语言实现各模块的模拟器或程序，代码风格统一，可编译运行
2. 输出结果清晰直观（甘特图、统计表格、内存状态图等）
3. 对实验结果进行分析和对比
4. 撰写符合规范的实验报告

### 实验环境

| 项目 | 配置 |
|------|------|
| 宿主机 | Windows 11 Home |
| 虚拟机 | VMware Workstation Pro 17 |
| 操作系统 | Ubuntu 20.04.6 LTS (amd64) |
| 内核版本 | Linux 5.4.x |
| 编译器 | GCC 9.x |
| 工具 | GNU Make, GDB, pthread, sysbench, perf, Python 3 + matplotlib |

---

## 二、实验内容

本课程设计包含 **基础必做**（4 题，占 70%）和 **自由扩展**（2 个方向，占 30%）两部分：

| 编号 | 题目 | 关键内容 |
|------|------|----------|
| 基础(1) | 处理机调度 | FCFS, SJF, SRTF, RR, Priority |
| 基础(2) | 内存管理 | 动态分区(FF/BF/WF) + 页面置换(FIFO/LRU/OPT) |
| 基础(3) | 进程同步 | 生产者-消费者、读者-写者、哲学家就餐 |
| 基础(4) | 文件系统 | 用户态模拟磁盘、i 节点、位图、shell 交互 |
| 扩展(1) | Linux 内核 | 字符设备驱动 + /proc 接口 |
| 扩展(2) | 调度与性能 | MLFQ、CFS、实时调度 demo、perf 实测出图 |

---

## 三、实验过程

### 3.1 基础(1) — 处理机调度算法

#### 设计思路

采用事件驱动的模拟框架，统一定义 `Process` 结构体（进程名、到达时间、服务时间、优先级），通过函数指针 `Picker` 参数化调度策略。

- **非抢占算法**（FCFS、SJF、Priority）共用 `run_nonpreemptive()` 框架
- **抢占算法**（SRTF、RR）各自独立实现
- 输出 ASCII 甘特图直观展示执行序列 + 统计表（等待时间、周转时间、带权周转时间）

#### 核心数据结构

```c
typedef struct {
    char  name[16];
    int   arrival, burst, priority;
    int   remaining, start, finish;
    bool  done;
} Process;

typedef int (*Picker)(Process *ps, int n, int t);  // 选择函数指针
```

#### 算法说明

| 算法 | 抢占 | 策略 |
|------|------|------|
| FCFS | 否 | 按到达时间先来先服务 |
| SJF | 否 | 选服务时间最短的进程 |
| SRTF | 是 | 每个时间单位选剩余时间最短的 |
| RR | 是 | 固定时间片轮转，缺省 q=2 |
| Priority | 否 | 选优先级最高（数值最小）的进程 |

#### 运行结果

<!-- 截图: 01-case1-all.png -->
![基础(1) 用例1全部算法对比](screenshots/01-scheduler/01-case1-all.png)

<!-- 截图: 02-case2-all.png -->
![基础(1) 用例2全部算法对比](screenshots/01-scheduler/02-case2-all.png)

#### 结果分析

- **FCFS** 实现最为简单，但会导致"护航效应"（长作业先到时短作业等待时间长）
- **SJF** 可最小化平均等待时间，但需要预知服务时间
- **SRTF** 进一步降低了平均周转时间，但抢占引入了额外上下文切换开销
- **RR** 通过时间片轮转保证了公平性，时间片大小直接影响响应时间与吞吐量的权衡
- **Priority** 允许关键任务优先执行，但可能导致低优先级饥饿

---

### 3.2 基础(2) — 内存管理

#### 3.2.1 动态分区分配

##### 设计思路

用数组模拟内存块链表，每个块记录起始地址、大小和分配状态。支持三种适配算法：
- **首次适应 (FF)**：从低地址开始找第一个足够大的空闲块
- **最佳适应 (BF)**：找满足大小的最小空闲块，减少碎片产生
- **最坏适应 (WF)**：找最大的空闲块，留下更大的剩余空间

分配时若块大于需求则分裂，释放时与相邻空闲块自动合并。支持 `compact` 紧凑操作。

##### 运行结果

<!-- 截图: 01-partition-ff.png -->
![动态分区 FF 算法](screenshots/02-memory/01-partition-ff.png)

<!-- 截图: 02-partition-bf.png -->
![动态分区 BF 算法](screenshots/02-memory/02-partition-bf.png)

<!-- 截图: 03-partition-wf.png -->
![动态分区 WF 算法](screenshots/02-memory/03-partition-wf.png)

<!-- 截图: 04-partition-compact.png -->
![动态分区 compact 操作](screenshots/02-memory/04-partition-compact.png)

##### 结果分析

- **FF** 倾向于使用低地址空间，高地址保留大块空闲，查找速度较快
- **BF** 每次选择最接近需求大小的空闲块，产生最小的剩余碎片，但每次都要遍历全部空闲块
- **WF** 选择最大空闲块分配，使得剩余空闲块仍然较大，有利于后续大请求
- **Compact** 操作可以消除外部碎片，将分散的空闲空间合并为一个大的空闲块

#### 3.2.2 页面置换

##### 设计思路

模拟进程的页面引用序列，维护固定数量的内存帧。新页面到达时若无空闲帧则换出一页：
- **FIFO**：换出最早装入的页面
- **LRU**：换出最久未被访问的页面（基于最近访问时间戳）
- **OPT**：换出在将来最远时间才使用的页面（理论最优基线）

##### 运行结果

<!-- 截图: 05-paging-n3.png -->
![页面置换 帧数=3](screenshots/02-memory/05-paging-n3.png)

<!-- 截图: 06-paging-n4.png -->
![页面置换 帧数=4](screenshots/02-memory/06-paging-n4.png)

##### 结果分析

- **OPT** 提供理论最优的缺页率，作为其他算法的比较基准
- **LRU** 利用了程序的局部性原理，缺页率接近 OPT，但需要额外硬件支持（访问位）
- **FIFO** 实现简单，但可能出现 Belady 异常（增加帧数反而增加缺页次数）
- 在帧数=3 时，参考串 `1 2 3 4 1 2 5 1 2 3 4 5` 的缺页率为 FIFO=75%, LRU=75%, OPT=58%
- 在帧数=4 时，FIFO 缺页率 83%（Belady 异常！），LRU=67%, OPT=50%

---

### 3.3 基础(3) — 进程同步与并发控制

#### 3.3.1 生产者-消费者问题

##### 设计思路

使用 POSIX 信号量实现有界缓冲区：
- `empty` 信号量（初值为缓冲大小）：计数空闲槽位
- `full` 信号量（初值为 0）：计数已填充槽位
- `mutex` 信号量（初值为 1）：保证互斥访问

多生产者和多消费者各用独立的线程实现，随机 sleep 模拟生产和消费的不确定性。

##### 运行结果

<!-- 截图: 01-producer-consumer.png -->
![生产者-消费者](screenshots/03-sync/01-producer-consumer.png)

#### 3.3.2 读者-写者问题

##### 设计思路

实现了两种策略：
- **读者优先 (RP)**：只要至少有一个读者在读取，新读者就可进入。使用 `rw_lock` 互斥锁：第一个读者获取，最后一个读者释放
- **写者优先 (WP)**：增加 `write_mutex`，有写者等待时阻止新读者进入，防止写者饥饿

##### 运行结果

<!-- 截图: 02-reader-writer-rp.png -->
![读者-写者 读者优先](screenshots/03-sync/02-reader-writer-rp.png)

<!-- 截图: 03-reader-writer-wp.png -->
![读者-写者 写者优先](screenshots/03-sync/03-reader-writer-wp.png)

##### 结果分析

- 读者优先可能导致写者饥饿（持续有新读者时写者永远等不到锁）
- 写者优先在保证写者不饥饿的同时，引入了额外的排队开销，读并发度有所降低

#### 3.3.3 哲学家就餐问题

##### 设计思路

采用**资源分级法**（Resource Hierarchy）避免死锁：
- 哲学家 0 到 N-2 先拿左筷再拿右筷
- 哲学家 N-1 先拿右筷再拿左筷
- 打破环形等待条件，保证无死锁

##### 运行结果

<!-- 截图: 04-dining-philosophers.png -->
![哲学家就餐](screenshots/03-sync/04-dining-philosophers.png)

##### 结果分析

- 所有哲学家都能完成规定次数的就餐，无死锁发生
- Meal Summary 显示 total meals = N × iters，验证了正确性
- 资源分级法相比使用信号量限制最多 N-1 人同时进餐的方法，并发度更高

---

### 3.4 基础(4) — 文件系统

#### 设计思路

在用户态用一个大文件（`disk.img`）模拟磁盘，实现完整的文件系统：

**磁盘布局**（512B/块，共 1024 块 ≈ 512KB）：

| 块范围 | 内容 |
|--------|------|
| Block 0 | 超级块（magic、块数、inode 数、空闲计数等） |
| Block 1 | inode 位图（64 个 inode） |
| Block 2 | 数据块位图（1024 个数据块） |
| Block 3-6 | inode 表（64 × 64B = 4 块） |
| Block 7-1023 | 数据块（1017 块） |

**inode 结构**：包含文件类型、大小、链接数、8 个直接块指针、1 个一级间接指针

**支持命令**：`mkdir`, `ls`, `create`, `write`, `cat`, `rm`, `df`, `format`

#### 运行结果

<!-- 截图: 01-batch-script.png -->
![文件系统批量测试](screenshots/04-filesystem/01-batch-script.png)

<!-- 截图: 02-interactive.png -->
![文件系统交互模式](screenshots/04-filesystem/02-interactive.png)

<!-- 截图: 03-rm-df.png -->
![文件系统删除前后对比](screenshots/04-filesystem/03-rm-df.png)

#### 结果分析

- 位图管理方式使得空闲块和 inode 的分配/释放为 O(n) 复杂度（n 为位图位数）
- 通过超级块持久化空闲计数，`df` 命令可以快速查询而不需要扫描全盘
- 目录实现为特殊类型的文件，其数据块存储 `DirEntry` 数组，支持最多 128 个条目/目录（8 个直接块 × 16 条目/块）
- 释放文件时会级联释放所有数据块和间接块，防止空间泄漏

---

### 3.5 扩展(1) — Linux 内核与系统编程

#### 设计思路

编写一个 Linux 内核模块（字符设备驱动），包含：

1. **字符设备 `/dev/oslab_chrdev`**：
   - 4KB 内核态环形缓冲区
   - 支持 `open`, `read`, `write`, `release` 操作
   - 使用 `mutex` 保护临界区（替代旧版的 `semaphore`）
   - 非阻塞语义：无数据可读时返回 0

2. **`/proc/oslab_info` 接口**：
   - 使用 `seq_file` API
   - 输出：设备名、缓冲使用量、open 计数、读写总次数/总字节数

#### 运行结果

<!-- 截图: 01-load.png -->
![内核模块加载](screenshots/ext1-kernel/01-load.png)

<!-- 截图: 02-rw.png -->
![设备读写测试](screenshots/ext1-kernel/02-rw.png)

<!-- 截图: 03-proc.png -->
![/proc/oslab_info 输出](screenshots/ext1-kernel/03-proc.png)

<!-- 截图: 04-unload.png -->
![内核模块卸载](screenshots/ext1-kernel/04-unload.png)

<!-- 截图: 05-dmesg.png -->
![内核日志 dmesg](screenshots/ext1-kernel/05-dmesg.png)

#### 结果分析

- 内核模块编译使用 kbuild 系统（`/lib/modules/$(uname -r)/build`）
- 驱动通过 `alloc_chrdev_region` + `cdev_add` + `class_create` + `device_create` 自动创建设备节点
- `/proc/oslab_info` 实时反映了驱动累计运行统计，可用于监控和调试
- `dmesg` 中的 `pr_info` 输出记录了每次 open/read/write 操作
- 卸载模块后设备节点自动消失，/proc 条目也随之移除

---

### 3.6 扩展(2) — 调度与性能优化

#### 3.6.1 MLFQ 与 CFS 模拟器

##### 设计思路

**MLFQ（多级反馈队列）**：
- 3 级队列，时间片分别为 2 / 4 / 8
- 新进程从最高优先级队列进入
- 用完当前时间片未完成的进程降级（到下一级队列）
- 每 30 时间单位进行全局 boost（所有进程回归最高优先级），防止饥饿

**CFS（完全公平调度，简化版）**：
- 使用 `vruntime` 跟踪每个进程的"虚拟运行时间"
- `vruntime += 实际运行时间 × (nice0_weight / 进程权重)`
- 每次调度选择 vruntime 最小的进程
- nice 值越低（优先级越高），权重越大，vruntime 增长越慢，获得更多 CPU 时间

##### 运行结果

<!-- 截图: 01-mlfq-cfs.png -->
![MLFQ 与 CFS 模拟对比](screenshots/ext2-sched-perf/01-mlfq-cfs.png)

##### 结果分析

- MLFQ 自动优待短作业（I/O 密集型）：短作业在高层队列快速完成，长作业被逐渐降级
- 周期性 boost 确保被降级的进程不会永久饥饿
- CFS 通过 vruntime 公平分配 CPU：高权重（低 nice）进程获得更多时间片
- 两者都能在不预先知道作业长度的情况下，实现良好的响应时间与吞吐量平衡

#### 3.6.2 实时调度演示

##### 设计思路

使用 `sched_setscheduler` 系统调用设置线程的调度策略：
- `SCHED_FIFO`：静态优先级抢占调度，高优先级线程先运行
- `SCHED_RR`：在 SCHED_FIFO 基础上增加同优先级时间片轮转
- `SCHED_OTHER`：CFS 默认策略，作为对照组

##### 运行结果

<!-- 截图: 02-realtime-fifo.png -->
![SCHED_FIFO 实时调度](screenshots/ext2-sched-perf/02-realtime-fifo.png)

<!-- 截图: 03-realtime-rr.png -->
![SCHED_RR 实时调度](screenshots/ext2-sched-perf/03-realtime-rr.png)

##### 结果分析

- SCHED_FIFO 下线程严格按优先级顺序完成，高优先级线程获得连续的 CPU 时间
- SCHED_RR 在 FIFO 基础上增加同优先级时间片轮转，防止同一优先级的线程相互饥饿
- 实时调度线程的上下文切换开销略高于 SCHED_OTHER，但响应时间确定性更好

#### 3.6.3 性能实测

##### 设计思路

使用 `sysbench` 和 Linux 的 `nice` / `chrt` 工具，系统测量不同调度参数下的性能表现：

1. **nice 值影响**：测试 nice 从 -10 到 +10，线程数 1/2/4 时的 CPU 吞吐量
2. **调度策略对比**：测试 SCHED_OTHER / SCHED_FIFO / SCHED_RR 下的延迟
3. **使用 Python matplotlib 生成对比图表**

##### 运行结果

<!-- 截图: 04-perf-terminal.png -->
![性能测试终端输出](screenshots/ext2-sched-perf/04-perf-terminal.png)

<!-- 截图: 05-perf-plot.png -->
![性能对比图表](screenshots/ext2-sched-perf/05-perf-plot.png)

##### 结果分析

- **nice 值对吞吐的影响**：低 nice 值（高优先级）在竞争环境下获得明显更多的 CPU 时间，events/sec 更高
- **多线程加速比**：随着线程数增加，总吞吐量上升但单线程吞吐量下降，体现了 CPU 争用
- **实时策略的延迟**：SCHED_FIFO 和 SCHED_RR 相比 SCHED_OTHER 在锁竞争场景下延迟更低
- **上下文切换开销**：实时调度策略的上下文切换延迟更低，但需要在应用设计上更谨慎（避免死锁和优先级反转）

---

## 四、小结

通过本次操作系统课程设计，我完成了以下工作：

### 基础部分（4/4 完成）

1. **处理机调度**：实现了 5 种经典调度算法，通过甘特图和统计表格直观对比了各算法的特点和性能差异。理解了抢占式与非抢占式、优先级与公平性之间的权衡。

2. **内存管理**：实现了动态分区的 FF/BF/WF 三种适配算法和紧凑操作，以及 FIFO/LRU/OPT 三种页面置换算法。通过实验观察到了 Belady 异常现象（FIFO 在帧数从 3 增加到 4 时缺页率反而上升），加深了对虚拟内存管理的理解。

3. **进程同步**：使用 pthread 和 POSIX 信号量分别实现了生产者-消费者、读者-写者（两种策略）和哲学家就餐三个经典问题。掌握了信号量编程模式和死锁避免策略（资源分级法）。

4. **文件系统**：从零实现了一个用户态模拟文件系统，包含超级块、inode 表、位图管理、数据块分配/释放、目录操作和文件读写。将课堂所学的文件系统概念（inode、间接块、位图等）真正落地到了代码层面。

### 扩展部分（2/2 完成）

5. **Linux 内核编程**：编写了一个完整的字符设备驱动模块（环形缓冲 + /proc 接口），掌握了内核模块开发流程（kbuild、insmod/rmmod、设备节点创建）。

6. **调度性能分析**：实现了 MLFQ 和 CFS 的简化模拟器，并在真实 Linux 系统上使用 sysbench 和 real-time scheduling API 进行了性能测量，通过 matplotlib 生成了可视化对比图表。

### 收获与体会

- 从"知道操作系统原理"到"用代码实现操作系统机制"，理解深度有了质的提升
- 掌握了 C 语言在系统编程中的应用（信号量、pthread、内核模块、文件 I/O）
- 熟悉了 Linux 开发环境（GCC、Makefile、dmesg、perf、sysbench）的使用
- 学习了如何设计可测试的模拟器（统一的输入/输出格式、参数化算法选择、批量测试脚本）

---

## 五、指导教师评语及成绩

> （此处由教师填写）

**评语：**



**成绩：** ______________

**指导教师签名：** ______________

**日期：** ______________

---

## 附录：源码目录结构

```
os-coursework-2026/
├── 01-scheduler/       基础(1) 处理机调度
│   ├── src/scheduler.c
│   ├── cases/case1.txt, case2.txt
│   └── Makefile
├── 02-memory/          基础(2) 内存管理
│   ├── partition/      → 动态分区分配 (FF/BF/WF)
│   └── paging/         → 页面置换 (FIFO/LRU/OPT)
├── 03-sync/            基础(3) 进程同步
│   ├── src/producer_consumer.c
│   ├── src/reader_writer.c
│   ├── src/dining_philosophers.c
│   └── Makefile
├── 04-filesystem/      基础(4) 文件系统
│   ├── src/filesystem.c
│   ├── cases/test_script.txt
│   └── Makefile
├── ext1-kernel/        扩展(1) Linux 内核
│   ├── src/oslab_chrdev.c
│   ├── load.sh, unload.sh
│   └── Makefile
├── ext2-sched-perf/    扩展(2) 调度与性能
│   ├── src/mlfq_cfs.c
│   ├── src/realtime_demo.c
│   ├── scripts/perf_measure.py
│   └── Makefile
├── docs/
│   ├── 00-setup-guide.md
│   ├── RUN-GUIDE.md
│   └── REPORT.md
└── README.md
```

> **GitHub 仓库**：`https://github.com/<user>/os-coursework-2026`
