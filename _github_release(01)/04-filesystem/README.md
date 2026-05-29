# 基础(4) 文件系统

用户态模拟磁盘镜像 `disk.img`，实现：

- 超级块 + i 节点表 + 数据块（512B/块，共 1024 块）
- **位图**管理空闲块/空闲 i 节点
- Shell 风格交互：`mkdir / ls / create / write / cat / rm / df / format`
- i 节点支持 8 个直接块指针 + 1 个一级间接块指针

## 运行

```bash
make
./filesystem                         # 交互 shell
./filesystem -f cases/test_script.txt  # 批量脚本
```

交互示例：
```
fs> mkdir /home
fs> create /home/test.txt
fs> write /home/test.txt Hello_World
fs> cat /home/test.txt
fs> ls /
fs> df
```
