# 基础(1) 处理机调度

实现并对比四种经典处理机调度算法：

| 算法 | 简述 | 抢占 |
|------|------|------|
| FCFS | 先来先服务 | 否 |
| SJF  | 短作业优先（非抢占） | 否 |
| SRTF | 最短剩余时间优先（SJF 抢占式） | 是 |
| RR   | 时间片轮转 | 是 |
| PRIO | 优先级调度（非抢占 & 抢占两种） | 可选 |

## 构建运行
```bash
make            # 编译产出 ./scheduler
./scheduler     # 进入交互菜单
./scheduler -f cases/case1.txt -a rr -q 2   # 批量用例，RR 算法，时间片=2
```

## 输入文件格式
每行一条进程，字段以空格分隔：
```
# pid  arrival  burst  priority
P1     0        7      3
P2     2        4      1
P3     4        1      4
P4     5        4      2
```
- `arrival` = 到达时间
- `burst` = 总服务时间（CPU 时间）
- `priority`：数值越小优先级越高（与教材通用约定一致）

## 输出
- 进程执行顺序（甘特图，ASCII）
- 每个进程：开始时间 / 完成时间 / 等待时间 / 周转时间 / 带权周转时间
- 平均等待时间、平均周转时间
- 在批处理模式下可一次跑全部算法用于对比

## 设计要点
- 单文件多算法：`src/scheduler.c` 中按 `--algorithm` 路由
- 通用结构 `Process { pid, arrival, burst, priority, remaining, start, finish }`
- 事件驱动而非时钟轮询：找下一个就绪进程，时间直接跳到事件点
