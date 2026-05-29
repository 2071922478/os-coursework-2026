#!/bin/bash
# unload.sh — 卸载 oslab_chrdev 内核模块
set -e

echo "Unloading oslab_chrdev..."
sudo rmmod oslab_chrdev
echo "Done. Module removed."
