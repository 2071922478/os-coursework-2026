#!/bin/bash
# load.sh — 加载 oslab_chrdev 内核模块
set -e

echo "Building and loading oslab_chrdev..."
make all
sudo insmod oslab_chrdev.ko
echo "Done. Device node:"
ls -l /dev/oslab_chrdev 2>/dev/null || echo "  (check dmesg for device info)"
echo ""
echo "You can now test with:"
echo "  echo 'hello' | sudo tee /dev/oslab_chrdev"
echo "  sudo cat /dev/oslab_chrdev"
echo "  cat /proc/oslab_info"
