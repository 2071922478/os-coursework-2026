# 扩展(2) 调度与性能优化

延伸基础(1)，从"模拟"提升到"实测+分析"：

1. **算法改进** — MLFQ（多级反馈队列）与 CFS vruntime 简化版
2. **实时调度 demo** — `sched_setscheduler` 使用 `SCHED_FIFO` / `SCHED_RR`
3. **性能实测** — sysbench + nice/chrt + Python matplotlib 出图

## 运行

```bash
make all

# MLFQ / CFS 模拟器
./mlfq_cfs -f cases/mlfq_case.txt -a all

# 实时调度 (需要 sudo)
sudo ./realtime_demo fifo 4

# 性能基准 + 出图
python3 scripts/perf_measure.py
```
