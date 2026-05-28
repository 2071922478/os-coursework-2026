# 基础(3) 进程同步与并发控制

C + pthread + POSIX semaphore（`<pthread.h>` `<semaphore.h>`），三个独立可执行：

| 程序 | 描述 |
|------|------|
| `producer_consumer` | 有界缓冲，多生产者-多消费者 |
| `reader_writer`     | 读者优先 与 写者优先 两版 |
| `dining_philosophers` | 用资源分级法避免死锁 |

构建：
```bash
make all
./producer_consumer 4 4 20    # 4 个生产者，4 个消费者，缓冲=20
./reader_writer rp 5 3        # 读者优先，5 reader 3 writer
./dining_philosophers 5
```

> 进度：占位。
