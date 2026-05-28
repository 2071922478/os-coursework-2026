# 基础(2) 内存管理

两个子模块（独立编译运行）：

| 子模块 | 内容 |
|--------|------|
| `partition/` | 动态分区分配：首次适应 FF + 最佳适应 BF + 最坏适应 WF + compact |
| `paging/`    | 页面置换：FIFO + LRU + OPT（最优置换基线） |

## 运行

```bash
# 动态分区分配
cd partition && make
./partition -a ff -f cases/case1.txt
./partition -a bf -f cases/case1.txt
./partition -a wf -f cases/case1.txt

# 页面置换
cd paging && make
./paging -f cases/case1.txt -n 3
./paging -f cases/case1.txt -n 4
```

## 实现要点

- 分区分配：数组模拟块链表，分配时分裂，释放时合并相邻空闲块
- 页面置换：逐帧显示每次访问后的帧状态，缺页标记 `F`
- Belady 异常：case2.txt 演示 FIFO 在 n=3→n=4 时缺页率反而上升
