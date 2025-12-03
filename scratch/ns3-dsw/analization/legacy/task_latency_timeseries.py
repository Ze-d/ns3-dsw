#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
任务延迟时间序列统计脚本
以1秒为颗粒度，统计每个时间段内的平均延迟
按任务的完成时间（End_Time_s）划分时间段
"""

import csv
import os
import sys
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# ================= 配置区域 =================

# 1. 动态解析数据目录 (优先级: 命令行参数 > 环境变量 > 默认相对路径)
if len(sys.argv) > 1:
    DATA_DIR = Path(sys.argv[1]).resolve()
elif os.environ.get("TARGET_ANALYSIS_DIR"):
    DATA_DIR = Path(os.environ.get("TARGET_ANALYSIS_DIR")).resolve()
elif os.environ.get("OUT_DIR"):
    # 兼容旧脚本的环境变量
    DATA_DIR = Path(os.environ.get("OUT_DIR")).resolve()
else:
    # 默认回退: 脚本所在目录/../out
    DATA_DIR = Path(__file__).resolve().parent.parent / "out"

print(f"[配置] 数据分析目录: {DATA_DIR}")

# 2. 基于 DATA_DIR 设置文件路径
CSV_INPUT_PATH = DATA_DIR / 'task_latency_trace.csv'
CSV_OUTPUT_PATH = DATA_DIR / 'task_latency_timeseries.csv'
VIS_OUTPUT_DIR = DATA_DIR / 'visualization'

# 字体设置 - 使用 CLAUDE.md 规范
FONT_NAME = 'Maple Mono Normal NF CN'
FONT_FILE = 'MapleMonoNormal-NF-CN-Regular.ttf'

# ===========================================

def configure_matplotlib_font():
    """配置 Matplotlib 使用指定的 Maple Mono 字体"""
    from matplotlib import font_manager

    try:
        font_path = None
        # 尝试系统查找
        for f in font_manager.fontManager.ttflist:
            if FONT_NAME in f.name:
                font_path = f.fname
                break

        # 尝试本地查找
        if not font_path and os.path.exists(FONT_FILE):
            font_path = FONT_FILE

        if font_path:
            font_manager.fontManager.addfont(font_path)
            plt.rcParams['font.family'] = FONT_NAME
            print(f"[配置] 成功加载字体: {FONT_NAME}")
        else:
            # 回退字体
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'DejaVu Sans']
            print(f"[警告] 未找到 {FONT_NAME}，已回退默认字体。")

        plt.rcParams['axes.unicode_minus'] = False

        # 设置 DPI 为 300 (CLAUDE.md 规范)
        plt.rcParams['figure.dpi'] = 300
        plt.rcParams['savefig.dpi'] = 300

    except Exception as e:
        print(f"[错误] 字体配置失败: {e}")

def load_task_latency_data(csv_path):
    """
    加载任务延迟数据
    返回: list of dict: 任务记录列表
    """
    print(f"[信息] 正在加载: {csv_path}")

    if not csv_path.exists():
        print(f"[错误] 找不到文件: {csv_path}")
        return []

    tasks = []
    try:
        with open(csv_path, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    task = {
                        'Task_Unique_ID': row['Task_Unique_ID'],
                        'Producer_Node': row['Producer_Node'],
                        'Consumer_Node': row['Consumer_Node'],
                        'Start_Time_s': float(row['Start_Time_s']),
                        'End_Time_s': float(row['End_Time_s']),
                        'Total_Latency_s': float(row['Total_Latency_s'])
                    }
                    tasks.append(task)
                except (ValueError, KeyError) as e:
                    # print(f"[警告] 跳过无效行: {e}")
                    continue

        print(f"[信息] 成功加载 {len(tasks)} 条任务记录")
        return tasks

    except Exception as e:
        print(f"[错误] 读取CSV失败: {e}")
        return []

def calculate_timeseries_latency(tasks):
    """
    按1秒为颗粒度计算时间序列平均延迟
    """
    if not tasks:
        return []

    print("[信息] 正在计算时间序列统计...")

    # 找出仿真时间范围
    end_times = [task['End_Time_s'] for task in tasks]
    # min_time = 0.0 
    max_time = max(end_times)

    # 按每秒分组
    time_buckets = {}
    for task in tasks:
        # 按完成时间划分到对应的秒
        time_bucket = int(task['End_Time_s'])
        if time_bucket not in time_buckets:
            time_buckets[time_bucket] = []
        time_buckets[time_bucket].append(task)

    # 计算每个时间段的平均延迟
    timeseries_data = []
    for time_sec in sorted(time_buckets.keys()):
        tasks_in_second = time_buckets[time_sec]
        avg_latency = sum(task['Total_Latency_s'] for task in tasks_in_second) / len(tasks_in_second)

        timeseries_data.append({
            'time': float(time_sec),
            'avg_latency': avg_latency,
            'task_count': len(tasks_in_second)
        })

    print(f"[信息] 生成了 {len(timeseries_data)} 个时间点的统计")
    return timeseries_data

def save_timeseries_csv(timeseries_data, output_path):
    """保存时间序列数据到CSV文件"""
    print(f"[信息] 正在保存到: {output_path}")

    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)

        with open(output_path, 'w', newline='', encoding='utf-8') as f:
            fieldnames = ['time_second', 'avg_latency_s', 'task_count']
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()

            for data in timeseries_data:
                writer.writerow({
                    'time_second': int(data['time']),
                    'avg_latency_s': f"{data['avg_latency']:.6f}",
                    'task_count': data['task_count']
                })

        print(f"[成功] 已保存 {len(timeseries_data)} 条时间序列记录")
        return True

    except Exception as e:
        print(f"[错误] 保存CSV失败: {e}")
        return False

def plot_timeseries(timeseries_data, output_path):
    """生成时间序列折线图"""
    if not timeseries_data:
        print("[警告] 无数据可绘制")
        return

    print("[信息] 正在生成时间序列折线图...")

    # 配置 matplotlib
    configure_matplotlib_font()

    # 提取数据
    times = [d['time'] for d in timeseries_data]
    avg_latencies = [d['avg_latency'] for d in timeseries_data]
    task_counts = [d['task_count'] for d in timeseries_data]

    # 创建图表 - 双Y轴显示
    fig, ax1 = plt.subplots(figsize=(14, 6), dpi=100)

    # 绘制平均延迟
    color1 = '#1f77b4'  # 蓝色
    ax1.set_xlabel('时间 Time (s)', fontsize=12)
    ax1.set_ylabel('平均延迟 Average Latency (s)', color=color1, fontsize=12)
    line1 = ax1.plot(times, avg_latencies, color=color1, linewidth=2,
                      marker='o', markersize=3, alpha=0.8, label='平均延迟')
    ax1.tick_params(axis='y', labelcolor=color1)
    ax1.grid(True, alpha=0.3)

    # 创建第二个Y轴显示任务数
    ax2 = ax1.twinx()
    color2 = '#ff7f0e'  # 橙色
    ax2.set_ylabel('完成任务数 Task Count', color=color2, fontsize=12)
    bars = ax2.bar(times, task_counts, alpha=0.3, color=color2, width=0.8, label='完成任务数')
    ax2.tick_params(axis='y', labelcolor=color2)

    # 图表标题
    plt.title('任务延迟时间序列统计 (Task Latency Time Series)', fontsize=14, fontweight='bold', pad=20)

    # 添加图例
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left', fontsize=10)

    # 保存图片
    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"[信息] 折线图已保存: {output_path}")

    # 也保存为SVG格式
    svg_path = output_path.with_suffix('.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"[信息] 折线图（SVG）已保存: {svg_path}")

    plt.close()

def print_statistics(timeseries_data):
    """打印统计信息"""
    if not timeseries_data:
        return

    print("\n" + "="*60)
    print("任务延迟时间序列统计摘要")
    print("="*60)

    # 计算总体统计
    all_latencies = [d['avg_latency'] for d in timeseries_data]
    all_task_counts = [d['task_count'] for d in timeseries_data]

    min_latency = min(all_latencies)
    max_latency = max(all_latencies)
    avg_latency = sum(all_latencies) / len(all_latencies)
    median_latency = sorted(all_latencies)[len(all_latencies)//2]

    total_tasks = sum(all_task_counts)
    peak_second = max(timeseries_data, key=lambda x: x['task_count'])

    print(f"时间范围: {min(timeseries_data, key=lambda x: x['time'])['time']:.0f}s - "
          f"{max(timeseries_data, key=lambda x: x['time'])['time']:.0f}s")
    print(f"总时间段数: {len(timeseries_data)}")
    print(f"")
    print(f"延迟统计 (秒):")
    print(f"  最小值:  {min_latency:.6f}s")
    print(f"  最大值:  {max_latency:.6f}s")
    print(f"  平均值:  {avg_latency:.6f}s")
    print(f"  中位数:  {median_latency:.6f}s")
    print(f"")
    print(f"任务统计:")
    print(f"  总完成任务数:  {total_tasks}")
    print(f"  最繁忙时段:  第{peak_second['time']:.0f}秒 ({peak_second['task_count']}个任务)")
    print("="*60)

def main():
    """主函数"""
    print("\n" + "="*60)
    print(" ns3-dsw 任务延迟时间序列统计")
    print("="*60)
    print()

    # 检查输入文件 (使用 pathlib 对象)
    if not CSV_INPUT_PATH.exists():
        print(f"[错误] 找不到输入文件: {CSV_INPUT_PATH}")
        print(f"当前分析目录: {DATA_DIR}")
        print("请先运行 extract_task_latency.py 生成数据！")
        sys.exit(1)

    # 确保输出目录存在
    if not VIS_OUTPUT_DIR.exists():
        VIS_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # 1. 加载数据
    tasks = load_task_latency_data(CSV_INPUT_PATH)
    if not tasks:
        print("[错误] 无数据可处理")
        sys.exit(1)

    # 2. 计算时间序列
    timeseries_data = calculate_timeseries_latency(tasks)
    if not timeseries_data:
        print("[错误] 时间序列计算失败")
        sys.exit(1)

    # 3. 保存CSV
    if not save_timeseries_csv(timeseries_data, CSV_OUTPUT_PATH):
        sys.exit(1)

    # 4. 生成图表
    output_image = VIS_OUTPUT_DIR / 'task_latency_timeseries.png'
    plot_timeseries(timeseries_data, output_image)

    # 5. 打印统计
    print_statistics(timeseries_data)

    print("\n✅ 时间序列统计完成！")
    print(f"   输出文件: {CSV_OUTPUT_PATH}")
    print(f"   图表文件: {output_image}")

if __name__ == "__main__":
    main()