/*
 * oslab_chrdev.c — 操作系统课设 字符设备驱动
 *
 * 提供:
 *   /dev/oslab_chrdev — 环形缓冲读写，支持 open/read/write/release
 *   /proc/oslab_info   — 驱动运行统计
 *
 * 编译: make
 * 加载: sudo ./load.sh
 * 卸载: sudo ./unload.sh
 *
 * 测试:
 *   echo "hello os" | sudo tee /dev/oslab_chrdev
 *   sudo cat /dev/oslab_chrdev
 *   cat /proc/oslab_info
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/string.h>

#define DEVICE_NAME  "oslab_chrdev"
#define CLASS_NAME   "oslab"
#define PROC_NAME    "oslab_info"
#define BUF_SIZE     (4 * 1024)   /* 4KB 环形缓冲 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS Course Design");
MODULE_DESCRIPTION("Character device driver with ring buffer and /proc interface");

/* ============ 环形缓冲 ============ */

typedef struct {
    char  data[BUF_SIZE];
    int   head;       /* 写位置 */
    int   tail;       /* 读位置 */
    int   count;      /* 当前数据量 */
} RingBuf;

static void rb_init(RingBuf *rb) {
    rb->head = rb->tail = rb->count = 0;
}

static int rb_write(RingBuf *rb, const char *src, int len) {
    int written = 0;
    while (written < len && rb->count < BUF_SIZE) {
        rb->data[rb->head] = src[written];
        rb->head = (rb->head + 1) % BUF_SIZE;
        rb->count++;
        written++;
    }
    return written;
}

static int rb_read(RingBuf *rb, char *dst, int len) {
    int read_bytes = 0;
    while (read_bytes < len && rb->count > 0) {
        dst[read_bytes] = rb->data[rb->tail];
        rb->tail = (rb->tail + 1) % BUF_SIZE;
        rb->count--;
        read_bytes++;
    }
    return read_bytes;
}

/* ============ 全局状态 ============ */

static dev_t          g_dev_num;
static struct cdev    g_cdev;
static struct class  *g_class = NULL;
static struct device *g_device = NULL;
static RingBuf        g_rbuf;
static struct mutex   g_mutex;

/* 统计 */
static unsigned long g_total_reads   = 0;
static unsigned long g_total_writes  = 0;
static unsigned long g_total_bytes_read  = 0;
static unsigned long g_total_bytes_written = 0;
static unsigned long g_open_count    = 0;

/* ============ 文件操作 ============ */

static int oslab_open(struct inode *inode, struct file *filp) {
    mutex_lock(&g_mutex);
    g_open_count++;
    mutex_unlock(&g_mutex);
    pr_info("oslab_chrdev: opened (count=%lu)\n", g_open_count);
    return 0;
}

static int oslab_release(struct inode *inode, struct file *filp) {
    pr_info("oslab_chrdev: released\n");
    return 0;
}

static ssize_t oslab_read(struct file *filp, char __user *buf,
                          size_t count, loff_t *f_pos) {
    char *tmp;
    int nread;

    if (count == 0) return 0;

    mutex_lock(&g_mutex);
    if (g_rbuf.count == 0) {
        mutex_unlock(&g_mutex);
        return 0;  /* 无数据返回 0 (非阻塞语义) */
    }

    tmp = kmalloc(count, GFP_KERNEL);
    if (!tmp) {
        mutex_unlock(&g_mutex);
        return -ENOMEM;
    }

    nread = rb_read(&g_rbuf, tmp, count);
    g_total_reads++;
    g_total_bytes_read += nread;
    mutex_unlock(&g_mutex);

    if (copy_to_user(buf, tmp, nread)) {
        kfree(tmp);
        return -EFAULT;
    }

    kfree(tmp);
    pr_info("oslab_chrdev: read %d bytes\n", nread);
    return nread;
}

static ssize_t oslab_write(struct file *filp, const char __user *buf,
                           size_t count, loff_t *f_pos) {
    char *tmp;
    int nwritten;

    if (count == 0) return 0;

    tmp = kmalloc(count, GFP_KERNEL);
    if (!tmp) return -ENOMEM;

    if (copy_from_user(tmp, buf, count)) {
        kfree(tmp);
        return -EFAULT;
    }

    mutex_lock(&g_mutex);
    nwritten = rb_write(&g_rbuf, tmp, count);
    g_total_writes++;
    g_total_bytes_written += nwritten;
    mutex_unlock(&g_mutex);

    kfree(tmp);
    pr_info("oslab_chrdev: wrote %d bytes (accepted %d)\n", (int)count, nwritten);
    return nwritten;  /* 返回实际写入量 */
}

static struct file_operations g_fops = {
    .owner   = THIS_MODULE,
    .open    = oslab_open,
    .release = oslab_release,
    .read    = oslab_read,
    .write   = oslab_write,
};

/* ============ /proc 接口 ============ */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static struct proc_dir_entry *g_proc_entry = NULL;
#endif

static int oslab_proc_show(struct seq_file *m, void *v) {
    mutex_lock(&g_mutex);
    seq_printf(m, "===== oslab_chrdev Driver Statistics =====\n");
    seq_printf(m, "Device         : /dev/%s\n", DEVICE_NAME);
    seq_printf(m, "Buffer size    : %d bytes\n", BUF_SIZE);
    seq_printf(m, "Buffer used    : %d bytes\n", g_rbuf.count);
    seq_printf(m, "Buffer free    : %d bytes\n", BUF_SIZE - g_rbuf.count);
    seq_printf(m, "Open count     : %lu\n", g_open_count);
    seq_printf(m, "Total reads    : %lu\n", g_total_reads);
    seq_printf(m, "Total writes   : %lu\n", g_total_writes);
    seq_printf(m, "Bytes read     : %lu\n", g_total_bytes_read);
    seq_printf(m, "Bytes written  : %lu\n", g_total_bytes_written);
    seq_printf(m, "==========================================\n");
    mutex_unlock(&g_mutex);
    return 0;
}

static int oslab_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, oslab_proc_show, NULL);
}

static const struct proc_ops g_proc_ops = {
    .proc_open    = oslab_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ============ 模块加载/卸载 ============ */

static int __init oslab_init(void) {
    int ret;

    /* 分配设备号 */
    ret = alloc_chrdev_region(&g_dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("oslab_chrdev: alloc_chrdev_region failed (%d)\n", ret);
        return ret;
    }
    pr_info("oslab_chrdev: device number %d:%d\n", MAJOR(g_dev_num), MINOR(g_dev_num));

    /* 初始化 cdev */
    cdev_init(&g_cdev, &g_fops);
    g_cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_cdev, g_dev_num, 1);
    if (ret < 0) {
        pr_err("oslab_chrdev: cdev_add failed (%d)\n", ret);
        goto err_cdev;
    }

    /* 创建设备节点 /dev/oslab_chrdev */
    g_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(g_class)) {
        ret = PTR_ERR(g_class);
        pr_err("oslab_chrdev: class_create failed (%d)\n", ret);
        goto err_class;
    }

    g_device = device_create(g_class, NULL, g_dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(g_device)) {
        ret = PTR_ERR(g_device);
        pr_err("oslab_chrdev: device_create failed (%d)\n", ret);
        goto err_device;
    }

    /* 创建 /proc/oslab_info */
    proc_create(PROC_NAME, 0444, NULL, &g_proc_ops);
    pr_info("oslab_chrdev: /proc/%s created\n", PROC_NAME);

    /* 初始化环形缓冲 */
    rb_init(&g_rbuf);
    mutex_init(&g_mutex);

    pr_info("oslab_chrdev: module loaded successfully\n");
    return 0;

err_device:
    class_destroy(g_class);
err_class:
    cdev_del(&g_cdev);
err_cdev:
    unregister_chrdev_region(g_dev_num, 1);
    return ret;
}

static void __exit oslab_exit(void) {
    remove_proc_entry(PROC_NAME, NULL);
    device_destroy(g_class, g_dev_num);
    class_destroy(g_class);
    cdev_del(&g_cdev);
    unregister_chrdev_region(g_dev_num, 1);
    mutex_destroy(&g_mutex);
    pr_info("oslab_chrdev: module unloaded\n");
}

module_init(oslab_init);
module_exit(oslab_exit);
