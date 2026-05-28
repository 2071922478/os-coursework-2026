# 基础(3) 进程同步与并发控制

C + pthread + POSIX semaphore，三个独立可执行：

| 程序 | 描述 |
|------|------|
| `producer_consumer` | 有界缓冲，多生产者-多消费者 |
| `reader_writer`     | 读者优先 (rp) 与 写者优先 (wp) 两种策略 |
| `dining_philosophers` | 资源分级法避免死锁 |

## 运行

```bash
make all
./producer_consumer 4 3 10 15    # 4 生产者, 3 消费者, 缓冲=10, 每线程 15 项
./reader_writer rp 3 2 5         # 读者优先
./reader_writer wp 3 2 5         # 写者优先
./dining_philosophers 5 3        # 5 哲学家, 各吃 3 轮
```

## 实现要点

- 生产者-消费者：`empty`/`full`/`mutex` 三个信号量
- 读者优先：第一个读者锁 `rw_lock`，最后一个释放
- 写者优先：增加 `write_mutex` 阻止写者等待期间新读者进入
- 哲学家：编号最大的哲学家先拿右筷，打破环形等待
