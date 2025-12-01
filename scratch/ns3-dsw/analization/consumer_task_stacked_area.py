#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
消费者任务数百分比堆积面积图生成器
从 pro_sink_stats.xml 文件中提取 EdgeSend 事件，
按时间窗口统计三个消费者的任务百分比分布，
生成 100% 堆积面积图。
遵循 CLAUDE.md 规范：使用 Maple Mono Normal NF CN 字体，300 DPI
"""

import xml.etree.ElementTree as ET
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
from pathlib import Path

# 导入配置
import config

def parse_pro_sink_stats_xml(file_path):
    """解析 pro_sink_stats.xml 文件并提取 EdgeSend 事件"""
    tree = ET.parse(file_path)
    root = tree.getroot()

    data = []
    for event in root.findall('Event'):
        if event.get('type') == 'EdgeSend':
            time = float(event.get('Time'))
            target_ip = event.get('TargetIp')
            data.append({
                'Time': time,
                'TargetIp': target_ip
            })

    return pd.DataFrame(data)

def aggregate_by_time_window(df, window_size=1.0):
    """按时间窗口聚合数据并计算百分比"""
    df = df.copy()
    df['TimeWindow'] = (df['Time'] // window_size) * window_size

    # 统计每个时间窗口内发送给每个消费者的任务数
    window_counts = df.groupby(['TimeWindow', 'TargetIp']).size().reset_index(name='Count')

    # 计算每个时间窗口的总任务数
    window_totals = window_counts.groupby('TimeWindow')['Count'].sum().reset_index(name='Total')

    # 合并数据
    window_data = window_counts.merge(window_totals, on='TimeWindow')

    # 计算百分比
    window_data['Percentage'] = (window_data['Count'] / window_data['Total']) * 100

    return window_data

def apply_smoothing(df, span=3):
    """
    对时间窗口数据应用【指数加权移动平均】平滑
    
    参数:
    df (pd.DataFrame): aggregate_by_time_window 返回的数据
    span (int): 定义衰减的"跨度", 类似于 SMA 的 window_size。
                 值越大, 曲线越平滑 (滞后也越高)。
    """
    pivot_data = create_pivot_data(df)
    
    smoothed_data = pivot_data.ewm(span=span, min_periods=1).mean()
    
    total_per_window = smoothed_data.sum(axis=1)
    total_per_window[total_per_window == 0] = 1.0
    normalized_data = smoothed_data.div(total_per_window, axis=0) * 100
    
    return normalized_data

def create_pivot_data(window_data):
    """将数据转换为堆积面积图所需的格式"""
    pivot_data = window_data.pivot(
        index='TimeWindow',
        columns='TargetIp',
        values='Percentage'
    ).fillna(0)

    return pivot_data

def plot_stacked_area_chart(pivot_data, output_path):
    """绘制 100% 堆积面积图"""
    plt.figure(figsize=(12, 6))

    # 颜色方案
# 颜色方案 (Vibrant)
    colors = ['#663399', '#00C49F', '#FF8042']
    consumer_ips = sorted(pivot_data.columns)

    # 创建堆积面积图
    bottom = np.zeros(len(pivot_data))

    for i, ip in enumerate(consumer_ips):
        plt.fill_between(
            pivot_data.index,
            bottom,
            bottom + pivot_data[ip],
            label=f'Consumer {ip}',
            color=colors[i % len(colors)],
            alpha=0.8
        )
        bottom += pivot_data[ip]

    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Task Percentage (%)', fontsize=12)
    plt.title('Consumer Task Distribution (1s Window)', fontsize=14, fontweight='bold')
    plt.legend(fontsize=11)
    plt.grid(True, alpha=0.3)
    plt.ylim(0, 100)
    plt.tight_layout()

    # 保存图片
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"消费者任务堆积面积图已保存至: {output_path}")

    # 也保存为SVG格式（矢量图）
    svg_path = output_path.with_suffix('.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"消费者任务堆积面积图（SVG）已保存至: {svg_path}")

    plt.close()

def main():
    import os

    # 获取当前脚本所在目录
    script_dir = Path(__file__).parent

    # ================= 参数解析 =================
    # 优先使用命令行传进来的参数
    if len(sys.argv) > 1:
        # 接收命令行参数 (数据目录)
        data_dir = Path(sys.argv[1])
    else:
        # 检查环境变量
        env_path = os.environ.get("TARGET_ANALYSIS_DIR")
        if env_path:
            data_dir = Path(env_path)
        else:
            # 回退到默认路径
            data_dir = script_dir / '../out'

    print(f"[信息] 数据目录: {data_dir}")

    # 文件路径（使用相对路径）
    input_file = data_dir / 'pro_sink_stats.xml'
    output_dir = data_dir / 'visualization'
    output_dir.mkdir(exist_ok=True)
    output_file = output_dir / 'consumer_task_stacked_area.png'

    print("正在解析 pro_sink_stats.xml...")
    df = parse_pro_sink_stats_xml(input_file)
    print(f"共解析 {len(df)} 条 EdgeSend 记录")

    print("正在按时间窗口聚合数据...")
    window_data = aggregate_by_time_window(df, window_size=1)
    print(f"聚合为 {len(window_data['TimeWindow'].unique())} 个时间窗口 (1s 窗口)")

    print("正在应用 2.5s 平滑...")
    # 2.5秒平滑需要2.5个1s窗口，使用3个窗口（3秒）近似
    pivot_data = apply_smoothing(window_data, span=3)

    print("正在生成堆积面积图...")
    plot_stacked_area_chart(pivot_data, output_file)

    print("\n=== Consumer Task Analysis ===")
    consumer_ips = sorted(df['TargetIp'].unique())
    print(f"Consumer nodes: {consumer_ips}")

    # 计算总体百分比
    total_counts = df['TargetIp'].value_counts()
    total_tasks = len(df)

    print(f"\nOverall task distribution:")
    for ip in consumer_ips:
        count = total_counts.get(ip, 0)
        percentage = (count / total_tasks) * 100
        print(f"  {ip}: {count} tasks ({percentage:.1f}%)")

    # 分析时间窗口内的分配变化
    print(f"\n=== Time Window Analysis ===")
    window_variance = pivot_data.var()
    print(f"Task allocation variance (measuring volatility):")
    for ip in consumer_ips:
        var = window_variance.get(ip, 0)
        print(f"  {ip}: {var:.2f}")

    # 找出分配最不均匀的时间窗口
    max_variance_window = (pivot_data.var(axis=1)).idxmax()
    min_variance_window = (pivot_data.var(axis=1)).idxmin()
    print(f"\nMost uneven allocation window: {max_variance_window:.1f}s")
    print(f"Most even allocation window: {min_variance_window:.1f}s")

if __name__ == '__main__':
    main()
