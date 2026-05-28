# 扩展(1) Linux 内核与系统编程

三件套（不需重编内核）：

1. **字符设备驱动** — `/dev/oslab_chrdev`，支持 open/read/write/release，内部 4KB 环形缓冲
2. **/proc 接口** — `/proc/oslab_info` 输出驱动运行统计
3. **加载/卸载脚本** — `load.sh` / `unload.sh`

> **操作前务必给虚拟机拍快照**（`pre-kernel-dev`）！

## 运行

```bash
make
sudo ./load.sh
echo "hello" | sudo tee /dev/oslab_chrdev
sudo cat /dev/oslab_chrdev
cat /proc/oslab_info
sudo ./unload.sh
```
