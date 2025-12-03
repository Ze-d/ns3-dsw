#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
任务延迟KPI统计脚本
计算并显示任务延迟相关的关键性能指标（KPI）
使用方法: python3 analization/calculate_task_latency_kpi.py [数据目录]
"""

import csv
import os
import sys
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
    # 默认回退: 脚本所在目录/../scratch/ns3-dsw/out
    DATA_DIR = Path(__file__).resolve().parent.parent / "scratch/ns3-dsw/out"

print(f"[配置] 数据分析目录: {DATA_DIR}")

# 2. 基于 DATA_DIR 设置文件路径
CSV_INPUT_PATH = DATA_DIR / 'task_latency_trace.csv'

# ===========================================

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

def calculate_kpi_basic_metrics(tasks):
    """
    计算基础KPI指标
    返回: dict: KPI指标字典
    """
    if not tasks:
        return {}

    print("\n正在计算基础KPI指标...")

    # 提取所有延迟值
    latencies = [task['Total_Latency_s'] for task in tasks]

    # 基础统计
    total_tasks = len(latencies)
    min_latency = min(latencies)
    max_latency = max(latencies)
    mean_latency = np.mean(latencies)
    median_latency = np.median(latencies)
    std_latency = np.std(latencies)

    # 百分位数
    p95_latency = np.percentile(latencies, 95)
    p99_latency = np.percentile(latencies, 99)

    # 时间范围
    start_times = [task['Start_Time_s'] for task in tasks]
    end_times = [task['End_Time_s'] for task in tasks]
    sim_start = min(start_times)
    sim_end = max(end_times)
    sim_duration = sim_end - sim_start

    # 吞吐量（任务/秒）
    throughput = total_tasks / sim_duration if sim_duration > 0 else 0

    # 按生产者节点分组统计
    producer_stats = {}
    for task in tasks:
        producer = task['Producer_Node']
        if producer not in producer_stats:
            producer_stats[producer] = []
        producer_stats[producer].append(task['Total_Latency_s'])

    # 按消费者节点分组统计
    consumer_stats = {}
    for task in tasks:
        consumer = task['Consumer_Node']
        if consumer not in consumer_stats:
            consumer_stats[consumer] = []
        consumer_stats[consumer].append(task['Total_Latency_s'])

    # 计算各节点的平均延迟
    producer_avg_latency = {node: np.mean(lat_list) for node, lat_list in producer_stats.items()}
    consumer_avg_latency = {node: np.mean(lat_list) for node, lat_list in consumer_stats.items()}

    # 计算各节点的任务数
    producer_task_count = {node: len(lat_list) for node, lat_list in producer_stats.items()}
    consumer_task_count = {node: len(lat_list) for node, lat_list in consumer_stats.items()}

    return {
        'total_tasks': total_tasks,
        'min_latency': min_latency,
        'max_latency': max_latency,
        'mean_latency': mean_latency,
        'median_latency': median_latency,
        'std_latency': std_latency,
        'p95_latency': p95_latency,
        'p99_latency': p99_latency,
        'sim_start': sim_start,
        'sim_end': sim_end,
        'sim_duration': sim_duration,
        'throughput': throughput,
        'producer_avg_latency': producer_avg_latency,
        'consumer_avg_latency': consumer_avg_latency,
        'producer_task_count': producer_task_count,
        'consumer_task_count': consumer_task_count
    }

def print_kpi_report(kpi):
    """打印KPI报告"""
    if not kpi:
        return

    print("\n" + "="*70)
    print(" 任务延迟KPI统计报告 (Task Latency KPI)")
    print("="*70)
    print()

    print("【仿真信息】")
    print(f"  仿真开始时间:     {kpi['sim_start']:.6f} s")
    print(f"  仿真结束时间:     {kpi['sim_end']:.6f} s")
    print(f"  仿真持续时间:     {kpi['sim_duration']:.6f} s")
    print()

    print("【整体性能指标】")
    print(f"  总完成任务数:     {kpi['total_tasks']}")
    print(f"  系统吞吐量:       {kpi['throughput']:.2f} 任务/秒")
    print()

    print("【延迟统计 (秒)】")
    print(f"  最小延迟:         {kpi['min_latency']:.6f} s")
    print(f"  平均延迟:         {kpi['mean_latency']:.6f} s")
    print(f"  中位数延迟:       {kpi['median_latency']:.6f} s")
    print(f"  最大延迟:         {kpi['max_latency']:.6f} s")
    print(f"  标准差:           {kpi['std_latency']:.6f} s")
    print()
    print(f"  95百分位延迟:     {kpi['p95_latency']:.6f} s")
    print(f"  99百分位延迟:     {kpi['p99_latency']:.6f} s")
    print()

    print("【按生产者节点统计】")
    for node in sorted(kpi['producer_avg_latency'].keys()):
        avg_lat = kpi['producer_avg_latency'][node]
        task_count = kpi['producer_task_count'][node]
        print(f"  {node}: 平均延迟={avg_lat:.6f}s, 任务数={task_count}")
    print()

    print("【按消费者节点统计】")
    for node in sorted(kpi['consumer_avg_latency'].keys()):
        avg_lat = kpi['consumer_avg_latency'][node]
        task_count = kpi['consumer_task_count'][node]
        print(f"  {node}: 平均延迟={avg_lat:.6f}s, 任务数={task_count}")
    print()

    print("="*70)

def main():
    """主函数"""
    print("="*70)
    print(" ns3-dsw 任务延迟KPI统计")
    print("="*70)
    print()

    # 检查输入文件
    if not CSV_INPUT_PATH.exists():
        print(f"[错误] 找不到输入文件: {CSV_INPUT_PATH}")
        print(f"       当前分析目录: {DATA_DIR}")
        print("请先运行 extract_task_latency.py 生成数据！")
        sys.exit(1)

    # 1. 加载数据
    tasks = load_task_latency_data(CSV_INPUT_PATH)
    if not tasks:
        print("[错误] 无数据可处理")
        sys.exit(1)

    # 2. 计算KPI
    kpi = calculate_kpi_basic_metrics(tasks)

    # 3. 打印报告
    print_kpi_report(kpi)

    print("\n✅ KPI统计完成！")

if __name__ == "__main__":
    main()