# 运行指南与截图清单

> 所有操作在 Ubuntu 20.04 虚拟机中执行，项目路径 `~/os-coursework-2026/`

---

## 基础(1) 处理机调度 — `01-scheduler/`

### 运行方式

```bash
cd ~/os-coursework-2026/01-scheduler
make
./scheduler -f cases/case1.txt -a all       # 批量运行全部算法
./scheduler -f cases/case2.txt -a all -q 4  # 用例2, RR时间片=4
./scheduler                                  # 交互菜单模式
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 1-1 | `./scheduler -f cases/case1.txt -a all` | 完整输出：5 个算法的甘特图 + 统计表 + 平均等待/周转 |
| 1-2 | `./scheduler -f cases/case2.txt -a all -q 4` | 用例2（进程数更多）的全部算法对比 |
| 1-3 | `./scheduler` 交互菜单选 6 (Compare ALL) | 交互模式下的对比输出（可选） |

---

## 基础(2) 内存管理 — `02-memory/`

### 2a. 动态分区分配 `02-memory/partition/`

```bash
cd ~/os-coursework-2026/02-memory/partition
make
./partition -a ff -f cases/case1.txt        # 首次适应
./partition -a bf -f cases/case1.txt        # 最佳适应
./partition -a wf -f cases/case1.txt        # 最坏适应
./partition -a ff -f cases/case2.txt        # 含紧凑(compact)的用例
./partition                                  # 交互模式
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 2-1 | `./partition -a ff -f cases/case1.txt` | FF 算法每一步分配/释放后的内存状态图 |
| 2-2 | `./partition -a bf -f cases/case1.txt` | BF 算法输出，对比与 FF 的差异 |
| 2-3 | `./partition -a wf -f cases/case1.txt` | WF 算法输出 |
| 2-4 | `./partition -a ff -f cases/case2.txt` | 包含 compact（紧凑）操作前后的内存状态对比 |

### 2b. 页面置换 `02-memory/paging/`

```bash
cd ~/os-coursework-2026/02-memory/paging
make
./paging -f cases/case1.txt -n 3             # 帧数=3, 三种算法对比
./paging -f cases/case1.txt -n 4             # 帧数=4, 观察 Belady 异常
./paging -r "1 2 3 4 1 2 5 1 2 3 4 5" -n 3  # 自定义引用串
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 2-5 | `./paging -f cases/case1.txt -n 3` | 帧数=3 时 FIFO/LRU/OPT 三步逐帧状态 + 缺页率 |
| 2-6 | `./paging -f cases/case1.txt -n 4` | 帧数=4 的输出，对比帧数=3 的结果，观察 Belady 异常 |
| 2-7 | 自定义引用串运行 | 可选：展示交互模式或自定义参数 |

---

## 基础(3) 进程同步 — `03-sync/`

```bash
cd ~/os-coursework-2026/03-sync
make
# 生产者-消费者: 4 生产者, 3 消费者, 缓冲=10, 每线程 15 个
./producer_consumer 4 3 10 15

# 读者-写者 (读者优先): 3 reader, 2 writer, 各运行 5 轮
./reader_writer rp 3 2 5

# 读者-写者 (写者优先)
./reader_writer wp 3 2 5

# 哲学家就餐: 5 个哲学家, 各吃 3 轮
./dining_philosophers 5 3
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 3-1 | `./producer_consumer 4 3 10 15` | 多生产者-多消费者并发输出, 最后的 total produced/consumed 验证 |
| 3-2 | `./reader_writer rp 3 2 5` | 读者优先的输出, 能看到多个读者同时读 |
| 3-3 | `./reader_writer wp 3 2 5` | 写者优先的输出, 对比读者优先的差异 |
| 3-4 | `./dining_philosophers 5 3` | 哲学家就餐全流程 + Meal Summary (无死锁) |

---

## 基础(4) 文件系统 — `04-filesystem/`

```bash
cd ~/os-coursework-2026/04-filesystem
make
./filesystem -f cases/test_script.txt        # 批量运行测试脚本
./filesystem                                  # 交互 shell 模式
```

### 交互模式建议操作序列
```
mkdir /home
mkdir /usr
create /home/test.txt
write /home/test.txt Hello_World_2026
cat /home/test.txt
ls /
ls /home
df
rm /home/test.txt
df
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 4-1 | `./filesystem -f cases/test_script.txt` | 批量脚本完整输出: mkdir/ls/create/write/cat/rm/df |
| 4-2 | 交互模式下 `mkdir, create, write, cat, ls, df` | 展示交互 shell 使用（截图前先执行几条命令） |
| 4-3 | 交互模式下 `rm` + `df` 对比 | 删除文件前后 df 的 free inode/block 变化 |

---

## 扩展(1) Linux 内核 — `ext1-kernel/`

> **重要**: 操作前务必在 VMware 中拍摄快照 `pre-kernel-dev`！

```bash
cd ~/os-coursework-2026/ext1-kernel
make all
sudo ./load.sh                                # 等价于 make load
```

### 测试命令
```bash
# 写入数据
echo "Hello from OS Lab 2026!" | sudo tee /dev/oslab_chrdev
echo "ABCDEFGHIJ" | sudo tee /dev/oslab_chrdev

# 读取数据
sudo cat /dev/oslab_chrdev

# 查看统计
cat /proc/oslab_info

# 卸载
sudo ./unload.sh                               # 等价于 make unload
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 5-1 | `make all && sudo ./load.sh` | 编译 + 加载成功, `ls -l /dev/oslab_chrdev` 设备节点存在 |
| 5-2 | `echo "..." \| sudo tee /dev/oslab_chrdev` + `sudo cat /dev/oslab_chrdev` | 写入和读回数据一致 |
| 5-3 | `cat /proc/oslab_info` | /proc 接口输出的统计信息 |
| 5-4 | `sudo ./unload.sh` + `ls /dev/oslab_chrdev` (不存在) | 卸载后设备节点消失 |
| 5-5 | `dmesg \| tail -20` | 内核日志, 显示模块加载/操作/卸载的 pr_info 输出 |

---

## 扩展(2) 调度与性能 — `ext2-sched-perf/`

### MLFQ / CFS 模拟器

```bash
cd ~/os-coursework-2026/ext2-sched-perf
make
./mlfq_cfs -f cases/mlfq_case.txt -a all     # MLFQ + CFS 对比
./mlfq_cfs -f cases/mlfq_case.txt -a mlfq    # 仅 MLFQ
```

### 实时调度 Demo (需要 root)

```bash
sudo ./realtime_demo fifo 4                    # SCHED_FIFO, 4 线程
sudo ./realtime_demo rr 3                      # SCHED_RR, 3 线程
sudo ./realtime_demo other 3                   # SCHED_OTHER 对照组
```

### 性能实测与出图

```bash
python3 scripts/perf_measure.py               # 全部基准测试
python3 scripts/perf_measure.py --nice        # 仅 nice 值影响
python3 scripts/perf_measure.py --rt          # 仅实时策略对比
```

### 截图清单

| 截图编号 | 运行内容 | 截图要点 |
|----------|----------|----------|
| 6-1 | `./mlfq_cfs -f cases/mlfq_case.txt -a all` | MLFQ 和 CFS 的甘特图 + 统计对比 |
| 6-2 | `sudo ./realtime_demo fifo 4` | 实时线程按优先级顺序完成, avg/max 耗时 |
| 6-3 | `sudo ./realtime_demo rr 3` | SCHED_RR 的输出 |
| 6-4 | `python3 scripts/perf_measure.py` | 终端输出 (events/sec 对比、策略延迟) |
| 6-5 | `results/sched_perf_comparison.png` | 生成的 4 张 matplotlib 子图 (打开图片截图) |

---

## 截图文件整理建议

在报告中使用以下命名规范，方便与报告对应：

```
screenshots/
├── 01-scheduler/
│   ├── 01-case1-all.png
│   ├── 02-case2-all.png
│   └── 03-interactive.png
├── 02-memory/
│   ├── 01-partition-ff.png
│   ├── 02-partition-bf.png
│   ├── 03-partition-wf.png
│   ├── 04-partition-compact.png
│   ├── 05-paging-n3.png
│   ├── 06-paging-n4.png
│   └── 07-paging-custom.png
├── 03-sync/
│   ├── 01-producer-consumer.png
│   ├── 02-reader-writer-rp.png
│   ├── 03-reader-writer-wp.png
│   └── 04-dining-philosophers.png
├── 04-filesystem/
│   ├── 01-batch-script.png
│   ├── 02-interactive.png
│   └── 03-rm-df.png
├── ext1-kernel/
│   ├── 01-load.png
│   ├── 02-rw.png
│   ├── 03-proc.png
│   ├── 04-unload.png
│   └── 05-dmesg.png
└── ext2-sched-perf/
    ├── 01-mlfq-cfs.png
    ├── 02-realtime-fifo.png
    ├── 03-realtime-rr.png
    ├── 04-perf-terminal.png
    └── 05-perf-plot.png
```

### 截图方法

1. **终端截图**: 使用 Ubuntu 自带的 `gnome-screenshot` 或 `PrtSc` 键截取终端窗口
2. **图片查看**: `eog results/sched_perf_comparison.png` 打开后截图
3. **复制到 Windows**: 使用 VMware 共享文件夹或 `scp`:
   ```powershell
   scp -P 2222 osuser@127.0.0.1:~/os-coursework-2026/screenshots/* ./screenshots/
   ```

---

## 一键运行所有测试

```bash
cd ~/os-coursework-2026

# 基础(1)
cd 01-scheduler && make test && cd ..

# 基础(2)
cd 02-memory/partition && make test && cd ../..
cd 02-memory/paging && make test && cd ../..

# 基础(3)
cd 03-sync && make test && cd ..

# 基础(4)
cd 04-filesystem && make test && cd ..

# 扩展(1) — 需要 root
cd ext1-kernel && make test && cd ..

# 扩展(2)
cd ext2-sched-perf && make test && cd ..
```
