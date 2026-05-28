# 实验环境搭建手册 — VMware + Ubuntu 20.04 + SSH

> 主机：Windows 11 · 项目目录：`D:\work\04` · 虚拟机存放：`D:\Development\Environment\VMs\`

本手册按"装 VMware → 装 Ubuntu → 配 SSH → 配开发工具"四步走，每步都附踩坑提示。

---

## 0. 前置准备

| 项 | 要求 |
|----|------|
| BIOS | 已开启 **Intel VT-x / AMD-V**（看 任务管理器→性能→CPU 右下"虚拟化"应为"已启用"）|
| Windows 功能 | **关闭** "Hyper-V"、"Windows 沙盒"、"虚拟机平台"；**关闭** Docker Desktop 的 WSL2 集成（与 VMware 冲突）|
| 磁盘 | C 盘可用 ≥ 5 GB（安装包），D 盘可用 ≥ 60 GB（虚拟机映像）|
| 下载 | VMware Workstation Pro 17.x、Ubuntu 20.04.6 LTS Desktop ISO |

**踩坑**：如果开机后 VMware 提示 "VMware Workstation and Device/Credential Guard are not compatible"，多半是 Hyper-V 还在跑。以管理员 PowerShell 执行：
```powershell
bcdedit /set hypervisorlaunchtype off
DISM /Online /Disable-Feature:Microsoft-Hyper-V-All /NoRestart
```
然后重启。

### ISO 下载（国内镜像加速）
- 清华源：`https://mirrors.tuna.tsinghua.edu.cn/ubuntu-releases/20.04/ubuntu-20.04.6-desktop-amd64.iso`
- 校验 SHA256 后再用（防止下载损坏）。

---

## 1. 安装 VMware Workstation

1. 双击安装包，选择**自定义安装路径**：`D:\Development\Environment\VMware\`
2. 取消勾选 "Check for product updates on startup"（避免每次开启检查更新弹窗）
3. 安装完毕，启动 → 输入许可证（教育版/试用版均可）

---

## 2. 创建虚拟机

1. 主菜单 → 新建虚拟机 → **自定义（高级）**
2. 兼容性：保持默认（Workstation 17.x）
3. **稍后安装操作系统**（不要让 VMware 简易安装，简易安装会跳过分区控制）
4. 客户机操作系统：Linux → Ubuntu 64 位
5. 虚拟机名称：`Ubuntu20.04-OSLab`；位置：`D:\Development\Environment\VMs\Ubuntu20.04-OSLab\`
6. 处理器：2 处理器 × 2 核心 = **4 vCPU**
7. 内存：**4096 MB**（4 GB）
8. 网络类型：**NAT**（默认）
9. SCSI 控制器：LSI Logic（默认）
10. 磁盘类型：SCSI / **拆分成多个文件**（便于备份/移动）
11. 磁盘容量：**40 GB**
12. 完成前选 "自定义硬件" → CD/DVD → 使用 ISO 映像 → 浏览到刚下载的 Ubuntu ISO
13. （可选）移除：打印机、声卡 — 实验用不到，关掉省事
14. 完成

**踩坑**：默认 USB 控制器是 USB 3.x，老主板可能识别异常；不行就改成 USB 2.0。

---

## 3. 安装 Ubuntu 20.04 LTS

启动虚拟机进入 ISO 安装界面：

1. 语言：**English**（实验中用英文系统出错信息可搜性更好）
2. 键盘：English (US)
3. 更新与软件：**Normal installation**；勾选 "Download updates while installing"（联网时可加快）；**不**勾选第三方软件（避免引入图形驱动问题）
4. 安装类型：**Erase disk and install Ubuntu**（在虚拟机中安全）
5. 时区：Shanghai
6. 用户信息（**记牢密码**）：
   - Your name：osuser
   - Computer name：oslab
   - Username：osuser
   - 密码：自定（实验场景可简单些）
   - 选 **Log in automatically** 省事
7. 等待安装完成 → Restart Now → 按提示拔 ISO（VMware 通常会自动断开 CD）

首次进入桌面后：
- 跳过 Ubuntu One/Livepatch 等向导
- **关闭自动更新弹窗**：Settings → About → Software Updates → 自动检查 改为 Never（避免实验中弹窗）

### 国内换源（重要，否则 apt 慢到怀疑人生）
```bash
sudo sed -i 's|archive.ubuntu.com|mirrors.tuna.tsinghua.edu.cn|g; s|security.ubuntu.com|mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list
sudo apt update
```

### 安装 open-vm-tools（实现剪贴板/拖拽/共享文件夹）
```bash
sudo apt install -y open-vm-tools open-vm-tools-desktop
sudo reboot
```

---

## 4. 安装开发工具链

```bash
sudo apt update && sudo apt -y upgrade
sudo apt -y install \
    build-essential gdb make cmake git vim \
    openssh-server net-tools curl wget \
    linux-headers-$(uname -r) \
    manpages-dev kmod \
    htop sysbench stress-ng linux-tools-common linux-tools-generic \
    python3-pip python3-matplotlib
```

验证：
```bash
gcc --version          # 应为 9.x
gdb --version
uname -r               # 内核版本
ls /lib/modules/$(uname -r)/build   # 内核头存在
```

---

## 5. 配置 SSH（这一步做完就能告别 VMware 黑窗口）

### 5.1 在虚拟机里启用 SSH
```bash
sudo systemctl enable --now ssh
sudo systemctl status ssh        # 应为 active (running)
ip addr | grep inet              # 记下 ens33 的 IP，例如 192.168.184.130
```

### 5.2 在 Windows 上配置 VMware NAT 端口转发
1. 关闭虚拟机或先记下 VM 的 IP
2. VMware → 编辑 → 虚拟网络编辑器 → 点 "更改设置" 提权
3. 选 VMnet8（NAT 模式）→ "NAT 设置..."
4. 添加端口转发：
   - 名称：`ssh-to-oslab`
   - 主机端口：`2222`
   - 类型：TCP
   - 虚拟机 IP：刚才记下的 IP（如 192.168.184.130）192.168.232.131
   - 虚拟机端口：22
5. 确定 → 应用

### 5.3 Windows 侧连接测试
打开 PowerShell（Windows 10/11 自带 OpenSSH 客户端）：
```powershell
ssh -p 2222 osuser@127.0.0.1
```
输入密码即可登录。

### 5.4 配置免密登录（推荐）
Windows 端（一次性）：
```powershell
ssh-keygen -t ed25519       # 一路回车，公钥默认在 C:\Users\<你>\.ssh\id_ed25519.pub
type $env:USERPROFILE\.ssh\id_ed25519.pub | ssh -p 2222 osuser@127.0.0.1 "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"
```
再次 `ssh -p 2222 osuser@127.0.0.1` 应免密直接进入。

### 5.5 VS Code Remote-SSH（开发体验质变）
1. Windows VS Code 安装扩展 **Remote - SSH**
2. F1 → "Remote-SSH: Open SSH Configuration File" → 选 `C:\Users\<你>\.ssh\config`，写入：
   ```
   Host oslab
       HostName 127.0.0.1
       Port 2222
       User osuser
       IdentityFile ~/.ssh/id_ed25519
   ```
3. F1 → "Remote-SSH: Connect to Host" → 选 `oslab` → 新窗口里就在虚拟机里编辑代码了
4. 打开文件夹：选 `/home/osuser/os-coursework-2026`

**踩坑**：
- 主机睡眠/挂起恢复后端口转发偶尔抽风，重启 VMware NAT 服务即可（任务管理器 → VMware NAT Service → 重新启动）。
- 如果 `ssh: connection refused`：VM 里跑 `sudo systemctl status ssh`；防火墙 `sudo ufw status`（默认 inactive）。

---

## 6. 把项目克隆进虚拟机

在虚拟机里：
```bash
cd ~
git clone https://github.com/<你的用户名>/os-coursework-2026.git
cd os-coursework-2026
```

（如果还没建 GitHub 仓库，先在主机上把 `D:\work\04` 整个目录推上去。）

### Git 全局配置（一次性）
```bash
git config --global user.name  "<你的名字>"
git config --global user.email "<你的邮箱>"
git config --global init.defaultBranch main
```

---

## 7. 建立第一个快照（救命用）

在 VMware 主菜单 → 虚拟机 → **快照 → 拍摄快照**，命名 `clean-with-tools`。

> 写内核模块之前 **再拍一次**（命名 `pre-kernel-dev`）。如果 `insmod` 把内核搞挂，直接回滚，比重装快得多。

---

## 8. 自检清单

| 检查项 | 命令 / 操作 | 期望 |
|--------|------------|------|
| 网络 | `ping -c 3 mirrors.tuna.tsinghua.edu.cn` | 通 |
| 工具链 | `gcc -v && make -v` | 输出版本 |
| 内核头 | `ls /lib/modules/$(uname -r)/build/Makefile` | 存在 |
| SSH | Windows `ssh -p 2222 osuser@127.0.0.1` | 进入 shell |
| VS Code Remote | 连 `oslab` 打开终端 | 在 VM 里 |
| 快照 | 快照管理器中有 `clean-with-tools` | 存在 |

到这里实验环境就绪。下一步：进入 `01-scheduler/` 编译验证调度器。

```bash
cd ~/os-coursework-2026/01-scheduler
make
./scheduler -f cases/case1.txt -a all
```
