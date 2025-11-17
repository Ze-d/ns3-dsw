#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
节点总电费折线图生成器
从 power_cost_node*.xml 文件中提取节点的累计电费数据，并生成折线图。
"""

import xml.etree.ElementTree as ET
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

# 导入配置
import config

def parse_power_cost_xml(file_path):
    """解析电费XML文件并提取数据"""
    tree = ET.parse(file_path)
    root = tree.getroot()

    data = []
    for sample in root.findall('Sample'):
        time = float(sample.get('time'))
        total_cost = float(sample.get('total_cost'))
        data.append({
            'Time': time,
            'total_cost': total_cost
        })

    return pd.DataFrame(data)

def plot_power_cost(node_data_dict, output_path):
    """绘制节点电费折线图"""
    plt.figure(figsize=(12, 6))

    colors = ['#1f77b4', '#ff7f0e', '#2ca02c']
    node_ids = ['Node-2', 'Node-6', 'Node-9']

    for i, (node_id, df) in enumerate(node_data_dict.items()):
        plt.plot(
            df['Time'].values,
            df['total_cost'].values,
            label=node_id,
            color=colors[i],
            linewidth=2,
            alpha=0.8
        )

    plt.xlabel('Time (s)', fontsize=12)
    plt.ylabel('Cumulative Cost', fontsize=12)
    plt.title('Node Cumulative Power Cost Over Time', fontsize=14, fontweight='bold')
    plt.legend(fontsize=11)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    # 保存图片
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"节点电费折线图已保存至: {output_path}")

    # 也保存为SVG格式（矢量图）
    svg_path = output_path.with_suffix('.svg')
    plt.savefig(svg_path, format='svg', bbox_inches='tight')
    print(f"节点电费折线图（SVG）已保存至: {svg_path}")

    plt.close()

def main():
    # 获取当前脚本所在目录
    script_dir = Path(__file__).parent

    # 文件路径（使用相对路径）
    input_dir = script_dir / '../out'
    node_files = {
        'Node-2': input_dir / 'power_cost_node2.xml',
        'Node-6': input_dir / 'power_cost_node6.xml',
        'Node-9': input_dir / 'power_cost_node9.xml'
    }

    output_dir = script_dir / '../out/visualization'
    output_dir.mkdir(exist_ok=True)
    output_file = output_dir / 'power_cost.png'

    print("正在解析电费XML文件...")
    node_data_dict = {}
    for node_id, file_path in node_files.items():
        df = parse_power_cost_xml(file_path)
        node_data_dict[node_id] = df
        print(f"  {node_id}: 解析了 {len(df)} 条记录")

    print("正在生成折线图...")
    plot_power_cost(node_data_dict, output_file)

    print("\n=== 节点电费分析 ===")
    for node_id, df in node_data_dict.items():
        final_cost = df['total_cost'].iloc[-1]
        avg_cost_rate = df['total_cost'].diff().mean()  # 平均成本增长速度
        print(f"{node_id}: 最终累计电费={final_cost:.6f}, 平均成本增长速度={avg_cost_rate:.6f}/秒")

    print("\n=== 电费对比 ===")
    final_costs = {node_id: df['total_cost'].iloc[-1] for node_id, df in node_data_dict.items()}
    max_cost_node = max(final_costs, key=final_costs.get)
    min_cost_node = min(final_costs, key=final_costs.get)
    print(f"电费最高节点: {max_cost_node} ({final_costs[max_cost_node]:.6f})")
    print(f"电费最低节点: {min_cost_node} ({final_costs[min_cost_node]:.6f})")
    print(f"电费差异: {final_costs[max_cost_node] - final_costs[min_cost_node]:.6f}")

if __name__ == '__main__':
    main()
