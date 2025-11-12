#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
调度器事件可视化脚本 (scheduler_events.xml)
用于分析调度器抖动(Thrashing)现象。

此脚本假定以下目录结构:
scratch/ns3-dsw/
├── out/
│   └── scheduler_events.xml     (输入)
└── visualization/
    ├── (此脚本.py)             (脚本位置)
    └── figure/
        ├── scheduler_decision_plot.png  (输出)
        └── scheduler_decision_plot.svg  (输出)
"""

import xml.etree.ElementTree as ET
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path # 导入 pathlib 用于处理路径

def parse_scheduler_events(file_path):
    """
    解析 scheduler_events.xml 文件并提取决策。
    """
    print(f"正在从 {file_path} 解析调度器事件...")
    tree = ET.parse(file_path)
    root = tree.getroot()
    data = []

    for event in root.findall('Event'):
        time = float(event.get('time'))
        producer = event.get('producerNode')
        decision = event.get('decision')

        current_target = event.find('CurrentTarget')
        switch_target = event.find('SwitchTarget')

        # 健壮性检查,以防 <Event> 标签不完整
        if current_target is None or switch_target is None:
            continue

        current_node = current_target.get('nodeId')
        switch_node = switch_target.get('nodeId')

        final_target_node = 'N/A'
        if decision == 'STAY':
            final_target_node = current_node
        elif decision == 'SWITCH':
            final_target_node = switch_node
            
        data.append({
            'Time': time,
            'Producer': f"Producer {producer}", # 格式化为字符串,以便 Y 轴排序
            'Decision': decision,
            'FinalTargetNode': f"Node {final_target_node}" # 格式化以便图例显示
        })
    
    print(f"解析完成, 共 {len(data)} 条调度事件。")
    return pd.DataFrame(data)

def create_scheduler_plot(df):
    """
    使用 matplotlib 创建散点图。
    """
    print("正在生成调度决策散点图...")

    # 筛选出我们关心的三个消费者节点
    consumers_to_plot = ['Node 2', 'Node 6', 'Node 9']
    df_filtered = df[df['FinalTargetNode'].isin(consumers_to_plot)]

    # 创建图形
    plt.figure(figsize=(12, 8))

    # 为每个消费者节点定义颜色
    colors = {'Node 2': 'red', 'Node 6': 'blue', 'Node 9': 'green'}

    # 获取唯一的生产者节点
    producers = sorted(df_filtered['Producer'].unique())

    # 为每个生产者创建 y 轴位置
    y_positions = {producer: i for i, producer in enumerate(producers)}

    # 绘制散点
    for target_node in consumers_to_plot:
        data_subset = df_filtered[df_filtered['FinalTargetNode'] == target_node]
        if not data_subset.empty:
            x_vals = data_subset['Time']
            y_vals = data_subset['Producer'].map(y_positions)
            plt.scatter(x_vals, y_vals, c=colors[target_node], label=target_node,
                       alpha=0.7, s=60)

    # 设置 y 轴标签
    plt.yticks(range(len(producers)), producers)

    # 设置标题和标签
    plt.title('Scheduler Decision Scatter Plot', fontsize=14, fontweight='bold')
    plt.xlabel('Time (seconds)', fontsize=12)
    plt.ylabel('Producer Nodes', fontsize=12)

    # 添加图例
    plt.legend(title='Target Consumer Node', bbox_to_anchor=(1.05, 1), loc='upper left')

    # 调整布局
    plt.tight_layout()

    return plt.gcf()

def main():
    # --- 路径定义 ---
    
    # 1. 获取脚本所在的目录 (即: .../scratch/ns3-dsw/visualization)
    script_dir = Path(__file__).resolve().parent

    # 2. 定义相对于脚本目录的输入路径
    # (../out/scheduler_events.xml)
    input_file = script_dir / "../out/scheduler_events.xml"
    
    # --- 路径修改 ---
    # 3. 定义输出目录为脚本同级的 "figure" 文件夹
    # (./figure/)
    output_dir = script_dir / "../out/visualization/"
    
    # 4. 确保输出目录存在
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # 5. 定义完整的输出文件路径
    output_file = output_dir / "scheduler_decision_plot.json"

    # --- 执行 ---
    try:
        # 解析数据
        df = parse_scheduler_events(input_file)

        # 创建图表
        fig = create_scheduler_plot(df)

        # 保存图表
        # 保存为 PNG 格式
        fig.savefig(str(output_file).replace('.json', '.png'), dpi=150, bbox_inches='tight')
        # 同时保存为 SVG 格式（矢量图）
        fig.savefig(str(output_file).replace('.json', '.svg'), bbox_inches='tight')

        # 关闭图形以释放内存
        plt.close(fig)

        print(f"\n成功！调度决策散点图已保存至:")
        print(f"  - {str(output_file).replace('.json', '.png')}")
        print(f"  - {str(output_file).replace('.json', '.svg')}")

    except FileNotFoundError:
        print(f"错误: 找不到输入文件。")
        print(f"脚本尝试在以下位置查找: {input_file}")
        print("请确保文件存在,并且脚本的目录结构正确。")
    except Exception as e:
        print(f"生成图表时发生错误: {e}")

if __name__ == '__main__':
    main()