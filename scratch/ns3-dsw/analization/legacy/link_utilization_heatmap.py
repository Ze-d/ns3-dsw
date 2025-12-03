#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
链路占用率热力图生成器
从 link_util.xml 文件中提取所有链路的利用率数据，
生成热力图显示 40 个链路方向的占用率变化。
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

def parse_link_util_xml(file_path):
    """解析链路利用率XML文件并提取数据"""
    tree = ET.parse(file_path)
    root = tree.getroot()

    # 收集所有时间点和链路信息
    time_points = []
    link_data = []  # 每个元素是 (time, link_id, direction, utilPct)

    for sample in root.findall('Sample'):
        time = float(sample.get('time'))
        time_points.append(time)

        for link in sample.findall('Link'):
            link_id = link.get('id')
            node_a = link.get('nodeA')
            node_b = link.get('nodeB')

            # 提取 AtoB 方向
            atob = link.find('AtoB')
            if atob is not None:
                util_pct = float(atob.get('utilPct'))
                link_data.append({
                    'time': time,
                    'link_id': link_id,
                    'direction': f'L{link_id}:{node_a}→{node_b}',
                    'utilPct': util_pct
                })

            # 提取 BtoA 方向
            btoa = link.find('BtoA')
            if btoa is not None:
                util_pct = float(btoa.get('utilPct'))
                link_data.append({
                    'time': time,
                    'link_id': link_id,
                    'direction': f'L{link_id}:{node_b}→{node_a}',
                    'utilPct': util_pct
                })

    return pd.DataFrame(link_data), time_points

def create_heatmap_data(df):
    """将数据转换为热力图所需的格式"""
    # 获取所有唯一的链路方向
    # 修复排序问题：按链路ID数字排序，然后按方向排序
    def sort_key(direction):
        # 提取链路ID和方向
        parts = direction.split(':')
        link_id = int(parts[0][1:])  # 去掉'L'前缀，转为数字
        direction_part = parts[1]
        # AtoB 应该在 BtoA 之前
        dir_order = 0 if '→' in direction_part else 1
        return (link_id, dir_order)

    directions = sorted(df['direction'].unique(), key=sort_key)
    times = sorted(df['time'].unique())

    # 创建数据矩阵
    heatmap_data = np.zeros((len(directions), len(times)))

    for i, direction in enumerate(directions):
        for j, time in enumerate(times):
            util = df[(df['direction'] == direction) & (df['time'] == time)]
            if not util.empty:
                heatmap_data[i, j] = util['utilPct'].iloc[0]

    return heatmap_data, directions, times

def plot_link_utilization_heatmap(heatmap_data, directions, times, output_path):
    """绘制链路利用率热力图"""
    plt.figure(figsize=(16, 10))

    # 使用 viridis 颜色映射
    im = plt.imshow(
        heatmap_data,
        cmap='viridis',
        aspect='auto',
        interpolation='nearest'
    )

    # 设置坐标轴
    time_indices = np.linspace(0, len(times) - 1, min(10, len(times)), dtype=int)
    time_labels = [f'{times[i]:.1f}' for i in time_indices]
    plt.xticks(time_indices, time_labels)
    plt.xlabel('Time (s)', fontsize=12)

    # Y轴标签
    y_ticks = np.arange(0, len(directions), max(1, len(directions) // 20))
    plt.yticks(y_ticks, [directions[i] for i in y_ticks])
    plt.ylabel('Link Direction', fontsize=12)

    # 设置标题和颜色条
    plt.title('Link Utilization Heatmap', fontsize=14, fontweight='bold')
    cbar = plt.colorbar(im, shrink=0.8)
    cbar.set_label('Utilization (%)', fontsize=11)

    plt.tight_layout()

    # 保存图片
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"链路利用率热力图已保存至: {output_path}")

    # 也保存为SVG格式（矢量图）
    svg_path = output_path.with_suffix('.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"链路利用率热力图（SVG）已保存至: {svg_path}")

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
    input_file = data_dir / 'link_util.xml'
    output_dir = data_dir / 'visualization'
    output_dir.mkdir(exist_ok=True)
    output_file = output_dir / 'link_utilization_heatmap.png'

    print("正在解析 link_util.xml...")
    df, time_points = parse_link_util_xml(input_file)
    print(f"共解析 {len(df)} 条记录，{len(time_points)} 个时间点，{len(df['direction'].unique())} 个链路方向")

    print("正在转换数据格式...")
    heatmap_data, directions, times = create_heatmap_data(df)

    print("正在生成热力图...")
    plot_link_utilization_heatmap(heatmap_data, directions, times, output_file)

    print("\n=== 链路利用率分析 ===")
    print(f"链路数量: {len(directions)} 个方向")

    # 计算每个链路的平均利用率
    link_avg_util = df.groupby('direction')['utilPct'].mean().sort_values(ascending=False)
    print(f"\n平均利用率最高的5个链路方向:")
    for i, (link, util) in enumerate(link_avg_util.head(5).items()):
        print(f"  {i+1}. {link}: {util:.2f}%")

    print(f"\n平均利用率最低的5个链路方向:")
    for i, (link, util) in enumerate(link_avg_util.tail(5).items()):
        print(f"  {i+1}. {link}: {util:.2f}%")

    # 计算总体的利用率统计
    overall_avg = df['utilPct'].mean()
    overall_max = df['utilPct'].max()
    overall_min = df['utilPct'].min()
    print(f"\n=== 整体统计 ===")
    print(f"平均利用率: {overall_avg:.2f}%")
    print(f"最大利用率: {overall_max:.2f}%")
    print(f"最小利用率: {overall_min:.2f}%")

if __name__ == '__main__':
    main()
