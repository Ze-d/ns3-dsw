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
    'Node2': '#6B8FB7',
    'Node6': '#D58C96',
    'Node9': '#8FBC8F',
}

# IP 到 Node 名称的映射表
IP_TO_NODE_MAP = {
    '10.0.8.1': 'Node 1',
    '10.0.9.2': 'Node 2',
    '10.0.12.2': 'Node 3',
    '10.0.11.2': 'Node 4',
    '10.0.8.2': 'Node 5',
    '10.0.2.2': 'Node 6',
    '10.0.16.2': 'Node 7',
    '10.0.17.2': 'Node 8',
    '10.0.6.2': 'Node 9',
    '10.0.5.2': 'Node 10',
    '10.0.4.2': 'Node 11',
    '10.0.0.1': 'Node 12',
    '10.0.0.2': 'Node 13',
    '10.0.1.2': 'Node 14',
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
    """绘制堆积面积图 (带平滑，IP已映射为Node名称)"""
    if pivot_df.empty: return

    # 1. 重命名列：将 IP 映射为 Node 名称
    new_columns = []
    for col in pivot_df.columns:
        # 使用 IP_TO_NODE_MAP 进行转换，如果找不到则保留原样
        new_name = IP_TO_NODE_MAP.get(col, col)
        new_columns.append(new_name)
    
    # 更新 DataFrame 的列名
    pivot_df.columns = new_columns

    # 2. 平滑处理
    smoothed = pivot_df.ewm(span=3, min_periods=1).mean()
    # 归一化回100%
    totals = smoothed.sum(axis=1)
    normalized = smoothed.div(totals, axis=0) * 100

    plt.figure(figsize=(12, 6))
    
    # 3. 动态生成颜色列表
    column_colors = []
    for col in normalized.columns:
        color = NODE_COLORS.get(col)
        if not color:
            color = NODE_COLORS.get(col.replace(" ", ""))
        if not color:
            color = '#999999' # 默认灰色
        column_colors.append(color)
    
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
    print(f"[绘图] 延迟分布图已保存: {output_path.name}")

def plot_link_utilization_heatmap(df, output_path):
    """ 绘制链路利用率热力图"""
    if df.empty: return

    # 透视表: Index=LinkDir, Columns=Time, Values=UtilPct
    pivot = df.pivot(index='LinkDir', columns='Time', values='UtilPct').fillna(0)
    
    # 按链路ID自然排序
    try:
        temp_df = pivot.reset_index()
        
        temp_df['sort_key_num'] = temp_df['LinkDir'].str.extract(r'L(\d+):', expand=False).astype(int, errors='ignore')

        temp_df = temp_df.sort_values(by=['sort_key_num', 'LinkDir'])
        
        pivot = temp_df.drop(columns='sort_key_num').set_index('LinkDir')
        
    except Exception as e:
        pass

    plt.figure(figsize=(14, 8))
    
    # 绘制热力图
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

def plot_traffic_generation_impulse(burst_data, output_path):
    """
    绘制流量生成脉冲图

    使用stem plots显示原始流量生成模式。
    高突发节点显示稀疏、高耸的尖峰（泊松簇过程）。
    低突发节点显示密集、矮小的尖峰（近泊松过程）。

    Args:
        burst_data: dict[int, pd.DataFrame] - load_burst_events()的返回值
                   每个DataFrame包含字段: time, burst_size, total_generated等
        output_path: 输出文件路径
    """
    if not burst_data: return

    # 节点分类
    high_burst_nodes = [1, 7, 11, 13]  # 高突发节点
    low_burst_nodes = [3, 4, 5, 8, 10, 12, 14]  # 低突发节点

    plt.style.use('default')
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.labelsize'] = 13
    plt.rcParams['axes.titlesize'] = 15
    plt.rcParams['xtick.labelsize'] = 11
    plt.rcParams['ytick.labelsize'] = 11
    plt.rcParams['legend.fontsize'] = 11
    plt.rcParams['figure.titlesize'] = 16

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10), constrained_layout=True)

    # 高突发节点
    colors_high = ['#d62728', '#ff7f0e', '#2ca02c', '#9467bd']
    for i, node_id in enumerate(high_burst_nodes):
        if node_id in burst_data:
            node_df = burst_data[node_id].copy()
            if not node_df.empty:
                node_df = node_df.sort_values('time')

                ax1.stem(node_df['time'], node_df['burst_size'],
                        linefmt=colors_high[i], markerfmt='o', basefmt=' ',
                        label=f'Edge-{node_id}')

    ax1.set_xlabel('Time (seconds)', fontsize=13)
    ax1.set_ylabel('Tasks Generated per Burst', fontsize=13)
    ax1.set_title('High-Burst Nodes',
                  fontsize=14, fontweight='bold', pad=15)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper right', ncol=4)
    ax1.set_xlim(0, 24)

    # 低突发节点
    colors_low = ['#1f77b4', '#8c564b', '#e377c2', '#7f7f7f',
                  '#bcbd22', '#17becf', '#aec7e8']
    for i, node_id in enumerate(low_burst_nodes):
        if node_id in burst_data:
            node_df = burst_data[node_id].copy()
            if not node_df.empty:
                node_df = node_df.sort_values('time')

                ax2.stem(node_df['time'], node_df['burst_size'],
                        linefmt=colors_low[i % len(colors_low)], markerfmt='o', basefmt=' ',
                        label=f'Edge-{node_id}')

    ax2.set_xlabel('Time (seconds)', fontsize=13)
    ax2.set_ylabel('Tasks Generated per Burst', fontsize=13)
    ax2.set_title('Low-Burst Nodes',
                  fontsize=14, fontweight='bold', pad=15)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='upper right', ncol=4, fontsize=10)
    ax2.set_xlim(0, 24)

    plt.suptitle('Traffic Generation Impulse',
                 fontsize=16, fontweight='bold', y=0.98)
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 流量生成脉冲图已保存: {output_path.name}")

def plot_producer_backlog_evolution(events_df, output_path):
    """
    绘制生产者积压演化图

    使用面积图显示队列积压随时间的变化。
    高突发节点显示锯齿模式（突发引起的积压）。
    低突发节点显示稳定模式（快速排空）。

    Args:
        events_df: pd.DataFrame - load_pro_sink_events()的返回值
                  包含字段: time, node_id, pending_tasks (EdgeSend事件)
        output_path: 输出文件路径
    """
    if events_df.empty: return

    high_burst_nodes = [1, 7, 11, 13]
    low_burst_nodes = [3, 4, 5, 8, 10, 12, 14]

    plt.style.use('default')
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.labelsize'] = 13
    plt.rcParams['axes.titlesize'] = 15
    plt.rcParams['xtick.labelsize'] = 11
    plt.rcParams['ytick.labelsize'] = 11
    plt.rcParams['legend.fontsize'] = 11
    plt.rcParams['figure.titlesize'] = 16

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10), constrained_layout=True)

    # === 子图 1: 高突发节点 (Training) ===
    colors_high = ['#d62728', '#ff7f0e', '#2ca02c', '#9467bd']

    for i, node_id in enumerate(high_burst_nodes):
        node_events = events_df[events_df['node_id'] == node_id].copy()
        if not node_events.empty:
            node_events = node_events.sort_values('time')

            ax1.fill_between(node_events['time'], 0, node_events['pending_tasks'],
                            step='post',
                            alpha=0.6, color=colors_high[i], label=f'Edge-{node_id}')

            ax1.plot(node_events['time'], node_events['pending_tasks'],
                    drawstyle='steps-post',
                    color=colors_high[i], linewidth=2)

    ax1.set_xlabel('Time (seconds)', fontsize=13)
    ax1.set_ylabel('Pending Tasks in Queue', fontsize=13)
    ax1.set_title('High-Burst Nodes',
                  fontsize=14, fontweight='bold', pad=15)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper right', ncol=4)
    ax1.set_xlim(0, 24)
    ax1.set_ylim(0, None)

    # === 子图 2: 低突发节点 (Inference) ===
    colors_low = ['#1f77b4', '#8c564b', '#e377c2', '#7f7f7f',
                  '#bcbd22', '#17becf', '#aec7e8']

    for i, node_id in enumerate(low_burst_nodes):
        node_events = events_df[events_df['node_id'] == node_id].copy()
        if not node_events.empty:
            node_events = node_events.sort_values('time')

            ax2.fill_between(node_events['time'], 0, node_events['pending_tasks'],
                            # step='post',
                            alpha=0.6, color=colors_low[i % len(colors_low)],
                            label=f'Edge-{node_id}')

            ax2.plot(node_events['time'], node_events['pending_tasks'],
                    # drawstyle='steps-post',
                    color=colors_low[i % len(colors_low)], linewidth=2)

    ax2.set_xlabel('Time (seconds)', fontsize=13)
    ax2.set_ylabel('Pending Tasks in Queue', fontsize=13)
    ax2.set_title('Low-Burst Nodes',
                  fontsize=14, fontweight='bold', pad=15)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='upper right', ncol=4, fontsize=10)
    ax2.set_xlim(0, 24)
    ax2.set_ylim(0, None)

    # 全局标题与保存
    plt.suptitle('Producer Backlog Evolution',
                 fontsize=16, fontweight='bold', y=0.98)

    # 保存为 PNG 和 SVG
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    if hasattr(output_path, 'with_suffix'):
        plt.savefig(output_path.with_suffix('.svg'), bbox_inches='tight')

    plt.close()
    print(f"[绘图] 生产者积压演化图已保存: {output_path.name}")

def plot_interarrival_time_cdf(interval_df, output_path):
    """
    绘制到达间隔时间CDF图

    显示任务到达间隔时间的经验CDF，与理论指数分布对比。

    Args:
        interval_df: pd.DataFrame - get_burst_intervals()的返回值
                    包含字段: node_id, time, interval
        output_path: 输出文件路径
    """
    if interval_df.empty: return

    high_burst_nodes = [1, 7, 11, 13]
    low_burst_nodes = [3, 4, 5, 8, 10, 12, 14]

    plt.style.use('default')
    plt.rcParams['font.size'] = 11
    plt.rcParams['axes.labelsize'] = 13
    plt.rcParams['axes.titlesize'] = 15
    plt.rcParams['xtick.labelsize'] = 11
    plt.rcParams['ytick.labelsize'] = 11
    plt.rcParams['legend.fontsize'] = 11
    plt.rcParams['figure.titlesize'] = 16

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7), constrained_layout=True)

    # 高突发节点间隔
    all_intervals_high = []
    for node_id in high_burst_nodes:
        node_intervals = interval_df[interval_df['node_id'] == node_id]['interval'].values
        if len(node_intervals) > 0:
            all_intervals_high.extend(node_intervals.tolist())

    if all_intervals_high:
        counts_high, bin_edges_high = np.histogram(all_intervals_high, bins=50, density=True)
        cdf_high = np.cumsum(counts_high) * (bin_edges_high[1] - bin_edges_high[0])

        ax1.step(bin_edges_high[1:], cdf_high, where='post',
                 color='#d62728', linewidth=3, label='High-Burst (Empirical)')

        theoretical_x = np.linspace(0.001, max(all_intervals_high), 1000)
        mean_interval = np.mean(all_intervals_high)
        theoretical_exp = 1 - np.exp(-theoretical_x / mean_interval)
        ax1.plot(theoretical_x, theoretical_exp, '--',
                 color='#ff7f0e', linewidth=2, alpha=0.7, label='Exponential Fit')

    ax1.set_xlabel('Inter-arrival Time (seconds, log scale)', fontsize=13)
    ax1.set_ylabel('Cumulative Probability', fontsize=13)
    ax1.set_title('High-Burst Nodes',
                  fontsize=14, fontweight='bold', pad=15)
    ax1.set_xscale('log')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim(0, 1)

    # 低突发节点间隔
    all_intervals_low = []
    for node_id in low_burst_nodes:
        node_intervals = interval_df[interval_df['node_id'] == node_id]['interval'].values
        if len(node_intervals) > 0:
            all_intervals_low.extend(node_intervals.tolist())

    if all_intervals_low:
        counts_low, bin_edges_low = np.histogram(all_intervals_low, bins=50, density=True)
        cdf_low = np.cumsum(counts_low) * (bin_edges_low[1] - bin_edges_low[0])

        ax2.step(bin_edges_low[1:], cdf_low, where='post',
                 color='#1f77b4', linewidth=3, label='Low-Burst (Empirical)')

        theoretical_x = np.linspace(0.001, max(all_intervals_low), 1000)
        mean_interval = np.mean(all_intervals_low)
        theoretical_exp = 1 - np.exp(-theoretical_x / mean_interval)
        ax2.plot(theoretical_x, theoretical_exp, '--',
                 color='#9467bd', linewidth=2, alpha=0.7, label='Exponential Fit')

    ax2.set_xlabel('Inter-arrival Time (seconds, log scale)', fontsize=13)
    ax2.set_ylabel('Cumulative Probability', fontsize=13)
    ax2.set_title('Low-Burst Nodes)',
                  fontsize=14, fontweight='bold', pad=15)
    ax2.set_xscale('log')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    ax2.set_ylim(0, 1)

    plt.suptitle('Inter-arrival Time CDF',
                 fontsize=16, fontweight='bold', y=0.98)
    plt.savefig(output_path, dpi=300, bbox_inches='tight', facecolor='white')
    plt.savefig(output_path.with_suffix('.svg'))
    plt.close()
    print(f"[绘图] 到达间隔时间CDF图已保存: {output_path.name}")