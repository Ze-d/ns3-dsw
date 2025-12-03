#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
视图层 (Plots Layer)
负责接收数据并绘制图表。
"""

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
# 导入配置以确保字体生效
from . import config

# ==========================================================
# ⚡️ 统一节点颜色配置 ⚡️
# 用于所有涉及特定节点/核心的图表
# ==========================================================
NODE_COLORS = {
    # Core ID / Node ID
    'Core-2': '#6B8FB7',  # 蓝色 (Blue)
    'Core-6': '#D58C96',  # 橙色 (Orange)
    'Core-9': '#8FBC8F',  # 绿色 (Green)
    # 兼容 Node ID
    'Node2': '#6B8FB7',
    'Node6': '#D58C96',
    'Node9': '#8FBC8F',
    
    '10.0.2.2': '#6B8FB7',
    '10.0.6.2': '#D58C96',
    '10.0.9.2': '#8FBC8F',
}
# ==========================================================


def plot_latency_timeseries(df, output_path):
    """绘制任务延迟时间序列 (双轴：延迟 + 任务数)"""
    if df.empty: return

    fig, ax1 = plt.subplots(figsize=(14, 6))

    color1 = '#1f77b4'
    ax1.set_xlabel('时间 Time (s)')
    ax1.set_ylabel('平均延迟 Average Latency (s)', color=color1)
    ax1.plot(df['time_second'], df['avg_latency'], color=color1, marker='o', markersize=3, label='Avg Latency')
    ax1.tick_params(axis='y', labelcolor=color1)
    ax1.grid(True, alpha=0.3)

    ax2 = ax1.twinx()
    color2 = '#ff7f0e'
    ax2.set_ylabel('完成任务数 Task Count', color=color2)
    ax2.bar(df['time_second'], df['task_count'], alpha=0.3, color=color2, width=0.8, label='Task Count')
    ax2.tick_params(axis='y', labelcolor=color2)

    plt.title('任务延迟时间序列统计 (Task Latency Time Series)')
    fig.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg')) # 同时保存SVG
    plt.close()
    print(f"[绘图] 延迟时序图已保存: {output_path.name}")

def plot_core_utilization(df, output_path, window=10):
    """绘制核心利用率折线图 (带平滑)"""
    if df.empty: return

    plt.figure(figsize=(12, 6))
    
    # 使用统一的 NODE_COLORS 字典
    for core_id in ['Core-2', 'Core-6', 'Core-9']:
        color = NODE_COLORS.get(core_id)
        if not color: continue # 如果没有定义颜色，跳过

        sub_df = df[df['Core-Id'] == core_id].sort_values('Time')
        if sub_df.empty: continue
        
        # 滚动平均
        sub_df['Smooth'] = sub_df['Utilization'].rolling(window=window, min_periods=1, center=True).mean()
        # 使用 color 变量
        plt.plot(sub_df['Time'], sub_df['Smooth'], label=core_id, color=color, alpha=0.8, linewidth=2.5)

    plt.xlabel('Time (s)')
    plt.ylabel('Utilization')
    plt.title('Core Utilization (Rolling Avg)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 核心利用率图已保存: {output_path.name}")

def plot_power_cost(data_dict, output_path):
    """绘制电费增长图"""
    if not data_dict: return

    plt.figure(figsize=(12, 6))
    # 无需使用固定的 colors 列表，直接从 data_dict 中获取 node_id 并查表
    
    for node_id, df in data_dict.items():
        if df.empty: continue
        # 使用统一的颜色，如果 node_id 不在字典中则使用默认灰色
        color = NODE_COLORS.get(node_id, '#333333')
        plt.plot(df['Time'], df['total_cost'], label=node_id, color=color, linewidth=2)

    plt.xlabel('Time (s)')
    plt.ylabel('Cumulative Cost')
    plt.title('Node Cumulative Power Cost')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 电费图已保存: {output_path.name}")

def plot_scheduler_decisions(df, output_path):
    """绘制调度器决策散点图"""
    if df.empty: return

    plt.figure(figsize=(12, 8))
    plt.grid(True, linestyle='--', alpha=0.6)
    
    # 直接使用 NODE_COLORS
    consumers = ['Core-2', 'Core-6', 'Core-9']
    
    # 确保生产者排序逻辑不变
    producers = sorted(df['Producer'].unique(), key=lambda x: int(x.split('-')[1]))
    y_map = {p: i for i, p in enumerate(producers)}

    for target in consumers:
        sub = df[df['FinalTargetNode'] == target]
        if not sub.empty:
            # 使用统一的 NODE_COLORS 字典，如果目标不在字典中则使用默认灰色
            color = NODE_COLORS.get(target, 'gray')
            plt.scatter(sub['Time'], sub['Producer'].map(y_map), 
                        c=color, label=target, alpha=0.7, s=60)

    plt.yticks(range(len(producers)), producers)
    plt.xlabel('Time (s)')
    plt.title('Scheduler Decision Scatter Plot')
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 调度决策图已保存: {output_path.name}")

def plot_stacked_area(pivot_df, output_path):
    """绘制堆积面积图 (带平滑)"""
    if pivot_df.empty: return

    # 平滑处理
    smoothed = pivot_df.ewm(span=3, min_periods=1).mean()
    # 归一化回100%
    totals = smoothed.sum(axis=1)
    normalized = smoothed.div(totals, axis=0) * 100

    plt.figure(figsize=(12, 6))
    
    # 动态生成颜色列表，按列名（即节点ID）查询 NODE_COLORS
    # 如果列名不在预设字典中，则使用默认颜色，并保持顺序
    column_colors = [NODE_COLORS.get(col, '#999999') for col in normalized.columns]
    
    plt.stackplot(normalized.index, normalized.T.values, 
                  labels=normalized.columns, colors=column_colors, alpha=0.8)

    plt.xlabel('Time (s)')
    plt.ylabel('Percentage (%)')
    plt.title('Consumer Task Distribution')
    plt.legend(loc='upper left')
    plt.margins(0, 0)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 任务分布堆积图已保存: {output_path.name}")

def plot_latency_dist(df, output_path):
    """绘制延迟直方图和箱线图"""
    if df.empty: return
    
    latencies = df['Total_Latency_s'].values
    
    # 直方图
    plt.figure(figsize=(10, 6))
    plt.hist(latencies, bins=40, color='#4c72b0', edgecolor='white', alpha=0.8)
    plt.axvline(latencies.mean(), color='red', linestyle='--', label=f'Avg: {latencies.mean():.4f}')
    plt.xlabel('Latency (s)')
    plt.title('End-to-End Latency Distribution')
    plt.legend()
    plt.savefig(output_path.parent / 'latency_histogram.png')
    plt.close()
    
    # 箱线图
    plt.figure(figsize=(10, 4))
    plt.boxplot(latencies, vert=False, patch_artist=True, 
                boxprops=dict(facecolor='#55a868', alpha=0.6))
    plt.xlabel('Latency (s)')
    plt.title('Latency Box Plot')
    plt.tight_layout()
    plt.savefig(output_path.parent / 'latency_boxplot.png')
    plt.close()
    print(f"[绘图] 延迟分布图已保存至目录")

def plot_link_utilization_heatmap(df, output_path):
    """[新增] 绘制链路利用率热力图"""
    if df.empty: return

    # 透视表: Index=LinkDir, Columns=Time, Values=UtilPct
    pivot = df.pivot(index='LinkDir', columns='Time', values='UtilPct').fillna(0)
    
    # 尝试按链路ID自然排序 (L2 在 L10 之前)，并保持方向的稳定顺序
    try:
        temp_df = pivot.reset_index()
        
        # 提取 'L' 后面的数字作为主要排序键，确保自然排序
        # 使用 expand=False 和 errors='ignore' 来提高兼容性
        temp_df['sort_key_num'] = temp_df['LinkDir'].str.extract(r'L(\d+):', expand=False).astype(int, errors='ignore')

        # 按 'sort_key_num' (数字) 和 'LinkDir' (字符串，作为次级排序键) 进行排序
        temp_df = temp_df.sort_values(by=['sort_key_num', 'LinkDir'])
        
        # 移除临时键并重新设置索引
        pivot = temp_df.drop(columns='sort_key_num').set_index('LinkDir')
        
    except Exception as e:
        # print(f"排序错误: {e}") # 调试用
        pass # 保持默认排序

    plt.figure(figsize=(14, 8))
    
    # 绘制热力图
    # aspect='auto' 确保在长方形画布上铺满
    # origin='lower' 确保列表的第一个元素在坐标轴底部 (或根据需求调整)
    plt.imshow(pivot.values, aspect='auto', cmap='plasma', vmin=0, vmax=100,
               extent=[pivot.columns.min(), pivot.columns.max(), 0, len(pivot.index)],
               origin='lower', interpolation='nearest')
    
    plt.colorbar(label='Utilization (%)')
    plt.xlabel('Time (s)')
    plt.ylabel('Link Direction')
    plt.title('Link Utilization Heatmap')
    
    # 设置 Y 轴刻度
    plt.yticks(np.arange(len(pivot.index)) + 0.5, pivot.index, fontsize=8)
    
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 链路热力图已保存: {output_path.name}")

def plot_price_per_MWh(data_dict, output_path):
    """绘制各节点单位电价曲线（展示相位差）"""
    if not data_dict: return

    plt.figure(figsize=(12, 6))
    # 无需定义局部的 colors 字典，直接使用全局的 NODE_COLORS

    for node_id, df in data_dict.items():
        if df.empty or 'price_per_MWh' not in df.columns: continue
        # 使用统一的颜色，如果 node_id 不在字典中则使用默认灰色
        color = NODE_COLORS.get(node_id, '#333333')
        plt.plot(df['Time'], df['price_per_MWh'], label=node_id, color=color, linewidth=1.5, alpha=0.8)

    plt.xlabel('Time (s)')
    plt.ylabel('Price (per MWh)')
    plt.title('Unit Electricity Price per Node (Phase Difference)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 单位电价曲线图已保存: {output_path.name}")