#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
核心利用率折线图生成器
从 node_util.xml 提取 Core-2、Core-6 和 Core-9 的利用率数据，
应用滚动平均平滑，并生成折线图。
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

def parse_node_util_xml(file_path):
    """解析 node_util.xml 文件并提取核心利用率数据"""
    tree = ET.parse(file_path)
    root = tree.getroot()

    data = []
    for event in root.findall('Event'):
        if event.get('type') == 'CoreUtil':
            time = float(event.get('Time'))
            core_id = event.get('Core-Id')
            utilization = float(event.get('Utilization'))
            data.append({
                'Time': time,
                'Core-Id': core_id,
                'Utilization': utilization
            })

    return pd.DataFrame(data)

def apply_rolling_average(df, window_size=5):
    """对每个核心的利用率应用滚动平均"""
    result_dfs = []

    for core_id in ['Core-2', 'Core-6', 'Core-9']:
        core_data = df[df['Core-Id'] == core_id].copy()
        core_data = core_data.sort_values('Time')
        core_data['Utilization_Smooth'] = core_data['Utilization'].rolling(
            window=window_size, center=True, min_periods=1
        ).mean()
        result_dfs.append(core_data)

    return result_dfs

def plot_core_utilization(core_data_list, output_path):
    """绘制核心利用率折线图"""
    plt.figure(figsize=(12, 6))

    colors = ['#1f77b4', '#ff7f0e', '#2ca02c']
    core_ids = ['Core-2', 'Core-6', 'Core-9']

    for i, (core_data, core_id) in enumerate(zip(core_data_list, core_ids)):
        plt.plot(
            core_data['Time'].values,
            core_data['Utilization_Smooth'].values,
            label=core_id,
            color=colors[i],
            linewidth=2,
            alpha=0.8
        )

    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Utilization', fontsize=12)
    plt.title('Core Utilization Over Time (Rolling Average Smoothed)', fontsize=14, fontweight='bold')
    plt.legend(fontsize=11)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    # 保存图片
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"核心利用率折线图已保存至: {output_path}")

    # 也保存为SVG格式（矢量图）
    svg_path = output_path.with_suffix('.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"核心利用率折线图（SVG）已保存至: {svg_path}")

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
    input_file = data_dir / 'node_util.xml'
    output_dir = data_dir / 'visualization'
    output_dir.mkdir(exist_ok=True)
    output_file = output_dir / 'core_utilization.png'

    print("正在解析 node_util.xml...")
    df = parse_node_util_xml(input_file)
    print(f"共解析 {len(df)} 条记录")

    print("正在应用滚动平均平滑...")
    core_data_list = apply_rolling_average(df, window_size=10)

    print("正在生成折线图...")
    plot_core_utilization(core_data_list, output_file)

    print("\n=== 核心利用率分析 ===")
    for core_data, core_id in zip(core_data_list, ['Core-2', 'Core-6', 'Core-9']):
        avg_util = core_data['Utilization'].mean()
        max_util = core_data['Utilization'].max()
        min_util = core_data['Utilization'].min()
        print(f"{core_id}: 平均利用率={avg_util:.3f}, 最大值={max_util:.3f}, 最小值={min_util:.3f}")

if __name__ == '__main__':
    main()
