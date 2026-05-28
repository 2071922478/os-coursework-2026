# 扩展(2) 调度与性能优化

延伸基础(1) 与扩展(1)，从"模拟"提升到"实测+分析"：

1. **算法改进** — 在模拟器中实现 MLFQ（多级反馈队列）与 CFS 的 vruntime 简化版
2. **实时调度 demo** — `sched_setscheduler` 使用 `SCHED_FIFO` / `SCHED_RR`，对比不同优先级线程的响应
3. **性能实测** — `perf` / `sysbench` / `stress-ng`，对 `nice` 值、调度策略下的吞吐与延迟出图（Python matplotlib）

> 进度：占位。
