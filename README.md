# 操作系统课程设计 2026

> 2026 春《操作系统》OS 课程设计 · 计算机科学与技术 1 班 · 授课教师：杨灿
> 截止：**2026-06-11**

## 任务范围
- **基础必做（70%）** — 全部 4 题
- **自由扩展（30%）** — 方向 (1) Linux 内核与系统编程 +（2）调度与性能优化

## 目录布局
| 目录 | 对应题目 | 关键产物 |
|------|---------|---------|
| `01-scheduler/` | 基础(1) 处理机调度 | FCFS / SJF / RR / 优先级 |
| `02-memory/`    | 基础(2) 内存管理 | 动态分区 FF/BF + 页面置换 FIFO/LRU |
| `03-sync/`      | 基础(3) 进程同步 | 生产者-消费者 / 读者-写者 / 哲学家 |
| `04-filesystem/`| 基础(4) 文件系统 | 用户态模拟磁盘 + i 节点 + 位图 |
| `ext1-kernel/`  | 扩展(1) Linux 内核 | 字符设备驱动 + /proc 接口 |
| `ext2-sched-perf/` | 扩展(2) 调度与性能 | MLFQ/CFS 简化版 + perf 实测 |
| `docs/`         | 实验报告与文档 | 安装手册、各题报告、整合报告 |
| `scripts/`      | 公用脚本 | 一键编译、测试、出图 |

## 运行环境
- 主机：Windows 11
- 虚拟机：VMware Workstation + Ubuntu 20.04.6 LTS（amd64）
- 工具链：gcc 9.x / make / gdb / pthread / linux-headers
- 详见 [docs/00-setup-guide.md](docs/00-setup-guide.md)

## 各题运行
```bash
# 基础(1) 处理机调度
cd 01-scheduler && make && ./scheduler

# 基础(3) 同步（举例）
cd 03-sync && make && ./producer_consumer 4 4 20
```

## 实验报告
- 安装手册：`docs/00-setup-guide.md`
- 各题报告：`docs/01-scheduler.md` ~ `docs/ext2-sched-perf.md`
- 最终报告：`docs/REPORT.md`（提交时导出为 PDF）

## 代码托管
> 提交报告时在此处补充 GitHub 仓库 URL：`https://github.com/<user>/os-coursework-2026`
