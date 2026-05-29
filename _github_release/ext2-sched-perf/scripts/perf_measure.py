#!/usr/bin/env python3
"""
perf_measure.py — 调度性能实测与可视化

使用 perf / sysbench / stress-ng 对不同的 nice 值和调度策略进行基准测试，
通过 matplotlib 生成对比图表。

前提:
  - Ubuntu 20.04
  - 已安装: sysbench stress-ng linux-tools-common linux-tools-generic
  - pip3 install matplotlib

用法:
  python3 scripts/perf_measure.py          # 运行全部测试并出图
  python3 scripts/perf_measure.py --nice   # 仅测试 nice 值影响
  python3 scripts/perf_measure.py --rt     # 仅测试实时调度对比
"""

import subprocess
import sys
import os
import time
import json
import platform

# ===== 配置 =====

NICE_VALUES = [-10, -5, 0, 5, 10]
SCHED_POLICIES = [
    ("SCHED_OTHER", "nice 0"),
    ("SCHED_FIFO", "chrt -f 50"),
    ("SCHED_RR", "chrt -r 50"),
]
THREAD_COUNTS = [1, 2, 4]
OUTPUT_DIR = "results"
PLOT_FILE = os.path.join(OUTPUT_DIR, "sched_perf_comparison.png")
DATA_FILE = os.path.join(OUTPUT_DIR, "results.json")


def run_cmd(cmd, timeout=120):
    """运行命令并返回 stdout"""
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=timeout
        )
        return result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        return "TIMEOUT"


def parse_sysbench_cpu(output):
    """从 sysbench cpu 输出中提取 events/sec"""
    import re
    match = re.search(r"events per second:\s+([0-9.]+)", output)
    if match:
        return float(match.group(1))
    return None


def parse_sysbench_threads(output):
    """从 sysbench threads 输出中提取总耗时"""
    import re
    match = re.search(r"total time:\s+([0-9.]+)s", output)
    if match:
        return float(match.group(1))
    return None


def bench_nice():
    """测试不同 nice 值下的 CPU 吞吐"""
    print("=" * 60)
    print("Benchmark 1: nice values vs CPU throughput")
    print("=" * 60)

    results = {}
    for threads in THREAD_COUNTS:
        key = f"nice_t{threads}"
        results[key] = []
        for nice in NICE_VALUES:
            print(f"  nice={nice:>3}, threads={threads} ... ", end="", flush=True)
            output = run_cmd(
                f"nice -n {nice} sysbench cpu --threads={threads} --cpu-max-prime=20000 run"
            )
            eps = parse_sysbench_cpu(output)
            if eps:
                results[key].append({"nice": nice, "eps": eps})
                print(f"{eps:.1f} events/sec")
            else:
                results[key].append({"nice": nice, "eps": 0})
                print("FAILED")

    return results


def bench_rt():
    """测试不同调度策略下的延迟"""
    print("\n" + "=" * 60)
    print("Benchmark 2: scheduling policy vs latency")
    print("=" * 60)

    results = {"rt_latency": []}
    for policy_name, prefix in SCHED_POLICIES:
        print(f"  {policy_name} ({prefix}) ... ", end="", flush=True)
        output = run_cmd(
            f"{prefix} sysbench threads --threads=4 --thread-yields=1000 "
            f"--thread-locks=8 --time=10 run"
        )
        total_time = parse_sysbench_threads(output)
        if total_time:
            results["rt_latency"].append({"policy": policy_name, "time_s": total_time})
            print(f"{total_time:.3f}s total")
        else:
            results["rt_latency"].append({"policy": policy_name, "time_s": 0})
            print("FAILED")

    return results


def bench_context_switch():
    """测试上下文切换延迟"""
    print("\n" + "=" * 60)
    print("Benchmark 3: context switch latency")
    print("=" * 60)

    results = {"ctx_switch": []}
    # 使用 sysbench 的 mutex 测试近似衡量
    output = run_cmd(
        "sysbench mutex --threads=4 --mutex-num=256 --mutex-locks=50000 "
        "--mutex-loops=100 run"
    )
    import re
    match = re.search(r"total time:\s+([0-9.]+)s", output)
    if match:
        t = float(match.group(1))
        results["ctx_switch"].append({"test": "mutex", "total_time_s": t})
        print(f"  Mutex test: {t:.3f}s")
    else:
        results["ctx_switch"].append({"test": "mutex", "total_time_s": 0})
        print("  FAILED")

    return results


def plot_results(data, output_path):
    """生成 matplotlib 对比图"""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("matplotlib not available, skipping plot")
        return False

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle("OS Scheduling Performance Analysis", fontsize=16, fontweight="bold")

    # Subplot 1: nice 值 vs 吞吐
    ax1 = axes[0, 0]
    for threads in THREAD_COUNTS:
        key = f"nice_t{threads}"
        if key in data:
            xs = [d["nice"] for d in data[key]]
            ys = [d["eps"] for d in data[key]]
            ax1.plot(xs, ys, "o-", label=f"{threads} thread(s)")
    ax1.set_xlabel("Nice value")
    ax1.set_ylabel("Events/sec")
    ax1.set_title("CPU Throughput vs Nice Value")
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Subplot 2: nice=-10 时不同线程数的加速比
    ax2 = axes[0, 1]
    if f"nice_t{THREAD_COUNTS[0]}" in data:
        base = data[f"nice_t1"][2]["eps"]  # nice=0, 1 thread
        speeds = []
        labels = []
        for threads in THREAD_COUNTS:
            key = f"nice_t{threads}"
            eps = [d["eps"] for d in data[key] if d["nice"] == 0]
            if eps:
                speedup = eps[0] / base if base > 0 else 0
                speeds.append(speedup)
                labels.append(f"{threads} thread(s)")
        if speeds:
            bars = ax2.bar(labels, speeds, color=["steelblue", "seagreen", "coral"])
            ax2.set_ylabel("Speedup (vs 1 thread nice=0)")
            ax2.set_title("Multi-thread Speedup")
            for bar, s in zip(bars, speeds):
                ax2.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.02,
                         f"{s:.2f}x", ha="center", fontsize=10)
            ax2.grid(True, alpha=0.3, axis="y")

    # Subplot 3: 调度策略 vs 延迟
    ax3 = axes[1, 0]
    if "rt_latency" in data and data["rt_latency"]:
        policies = [d["policy"] for d in data["rt_latency"]]
        times = [d["time_s"] for d in data["rt_latency"]]
        colors = ["skyblue", "salmon", "lightgreen"]
        bars = ax3.bar(policies, times, color=colors[:len(policies)])
        ax3.set_ylabel("Total time (s)")
        ax3.set_title("Scheduling Policy Latency Comparison")
        for bar, t in zip(bars, times):
            ax3.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.01,
                     f"{t:.2f}s", ha="center", fontsize=10)
        ax3.grid(True, alpha=0.3, axis="y")

    # Subplot 4: 汇总摘要
    ax4 = axes[1, 1]
    ax4.axis("off")
    summary_lines = [
        "Performance Summary:",
        "",
        "nice value:",
    ]
    if f"nice_t1" in data:
        for d in data["nice_t1"]:
            summary_lines.append(f"  nice={d['nice']:>3}: {d['eps']:.0f} eps")
    summary_lines.append("")
    summary_lines.append("Scheduling policies (4 threads):")
    if "rt_latency" in data:
        for d in data["rt_latency"]:
            summary_lines.append(f"  {d['policy']}: {d['time_s']:.3f}s")

    ax4.text(0.05, 0.95, "\n".join(summary_lines),
             transform=ax4.transAxes, fontsize=10, fontfamily="monospace",
             verticalalignment="top")

    plt.tight_layout()
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"\nPlot saved to {output_path}")
    return True


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    run_all = True
    run_nice_only = False
    run_rt_only = False

    for arg in sys.argv[1:]:
        if arg == "--nice":
            run_nice_only = True
            run_all = False
        elif arg == "--rt":
            run_rt_only = True
            run_all = False

    data = {}

    if run_all or run_nice_only:
        data.update(bench_nice())

    if run_all or run_rt_only:
        data.update(bench_rt())
        data.update(bench_context_switch())

    # 保存数据
    with open(DATA_FILE, "w") as f:
        json.dump(data, f, indent=2)
    print(f"\nRaw data saved to {DATA_FILE}")

    # 出图
    plot_results(data, PLOT_FILE)


if __name__ == "__main__":
    main()
