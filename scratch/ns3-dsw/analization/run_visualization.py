#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化入口脚本
一键生成所有图表，替代原有的多个 plotting 脚本。
"""

import sys
import os
from pathlib import Path
from lib.loader import DataLoader
from lib.metrics import MetricsCalculator
import lib.plots as plots

def main():
    # 1. 确定目录
    script_dir = Path(__file__).parent
    if len(sys.argv) > 1:
        data_dir = Path(sys.argv[1])
    elif os.environ.get("TARGET_ANALYSIS_DIR"):
        data_dir = Path(os.environ.get("TARGET_ANALYSIS_DIR"))
    else:
        data_dir = script_dir.parent / 'out'
        
    vis_dir = data_dir / 'visualization'
    vis_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"=== 开始可视化生成 ===")
    print(f"输出目录: {vis_dir.resolve()}")

    try:
        loader = DataLoader(data_dir)
        metrics = MetricsCalculator(loader)
    except Exception as e:
        print(f"[Fatal] 初始化失败: {e}")
        sys.exit(1)

    # 2. 生成各类图表
    
    # (1) 任务延迟时序图
    print("1. 生成延迟时序图...")
    ts_df = metrics.get_latency_timeseries()
    plots.plot_latency_timeseries(ts_df, vis_dir / 'task_latency_timeseries.png')

    # (2) 核心利用率
    print("2. 生成核心利用率图...")
    util_df = loader.load_node_utilization()
    plots.plot_core_utilization(util_df, vis_dir / 'core_utilization.png')

    # (3) 电费增长
    print("3. 生成电费趋势图...")
    power_dict = loader.load_power_costs()
    plots.plot_power_cost(power_dict, vis_dir / 'power_cost.png')

    # (4) 消费者任务堆积图
    print("4. 生成任务分布堆积图...")
    dist_df = metrics.get_consumer_distribution_timeseries()
    plots.plot_stacked_area(dist_df, vis_dir / 'consumer_task_stacked_area.png')

    # (5) 调度决策图
    print("5. 生成调度决策散点图...")
    sched_df = loader.load_scheduler_events()
    plots.plot_scheduler_decisions(sched_df, vis_dir / 'scheduler_decision_plot.png')

    # (6) 延迟分布 (直方图/箱线图)
    print("6. 生成延迟分布统计图...")
    plots.plot_latency_dist(metrics.task_df, vis_dir / 'latency_placeholder.png') # 文件名由函数内部决定

    # (7) 链路利用率热力图
    print("7. 生成链路利用率热力图...")
    link_df = loader.load_link_utilization_heatmap_data()
    plots.plot_link_utilization_heatmap(link_df, vis_dir / 'link_utilization_heatmap.png')

    # (8) 单位电价曲线
    print("8. 生成单位电价曲线图...")
    plots.plot_price_per_MWh(power_dict, vis_dir / 'price_per_MWh.png')

    print("\n[完成] 所有图表已生成完毕。")

if __name__ == "__main__":
    main()