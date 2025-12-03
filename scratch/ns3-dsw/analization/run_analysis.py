#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
KPI 分析入口脚本
替代原有的 calculate_kpi.py 和 calculate_task_latency_kpi.py。
生成 JSON 格式的分析报告。
"""

import sys
import os
import json
from pathlib import Path
from lib.loader import DataLoader
from lib.metrics import MetricsCalculator

def main():
    # 1. 确定数据目录
    script_dir = Path(__file__).parent
    if len(sys.argv) > 1:
        data_dir = Path(sys.argv[1])
    elif os.environ.get("TARGET_ANALYSIS_DIR"):
        data_dir = Path(os.environ.get("TARGET_ANALYSIS_DIR"))
    elif os.environ.get("OUT_DIR"):
        data_dir = Path(os.environ.get("OUT_DIR"))
    else:
        data_dir = script_dir.parent / 'out'
    
    print(f"=== NS3-DSW 仿真分析 ===")
    print(f"数据目录: {data_dir.resolve()}")
    
    # 2. 初始化核心组件
    try:
        loader = DataLoader(data_dir)
        metrics = MetricsCalculator(loader)
    except Exception as e:
        print(f"[Fatal] 初始化失败: {e}")
        sys.exit(1)

    # 3. 计算所有指标
    print("正在计算 KPI...")
    report = {
        "global_kpi": metrics.get_global_kpi(),
        "latency_stats": metrics.get_latency_stats()
    }
    
    # 4. 生成 CSV (兼容旧接口: task_latency_trace.csv)
    # 这一步是为了让那些依赖 CSV 的旧工具（如果有的话）还能工作
    csv_path = data_dir / 'task_latency_trace.csv'
    if not metrics.task_df.empty:
        metrics.task_df.to_csv(csv_path, index=False)
        print(f"[文件] 任务追踪 CSV 已生成: {csv_path.name}")

    # 5. 生成时序 CSV (兼容旧接口: task_latency_timeseries.csv)
    ts_df = metrics.get_latency_timeseries()
    if not ts_df.empty:
        ts_csv_path = data_dir / 'task_latency_timeseries.csv'
        ts_df.to_csv(ts_csv_path, index=False)
        print(f"[文件] 延迟时序 CSV 已生成: {ts_csv_path.name}")

    # 6. 控制台输出摘要
    kpi = report['global_kpi']
    lat = report['latency_stats']
    
    print("\n" + "="*50)
    print("KPI 汇总报告")
    print("="*50)
    print(f"1. 总电价:                    {kpi.get('total_power_cost', 0):.6f}")
    print(f"2. 整体平均算力利用率:         {kpi.get('avg_core_utilization', 0)*100:.2f}%")
    print(f"3. 整体平均延迟:              {kpi.get('avg_delay_ms', 0):.6f} ms")
    print(f"4. 整体链路平均带宽:           {kpi.get('avg_link_bandwidth_mbps', 0):.2f} Mbps")
    print(f"5. 整体链路平均利用率:         {kpi.get('avg_link_utilization_pct', 0):.2f}%")
    print(f"6. 完成总任务数:              {kpi.get('total_completed_tasks', 0)}")
    print(f"7. 平均每任务电价:             {kpi.get('avg_cost_per_task', 0):.6f}")
    
    if lat:
        print("-" * 50)
        print(f"任务延迟详情 (秒):")
        print(f"Min: {lat['min']:.6f} | Avg: {lat['mean']:.6f} | Max: {lat['max']:.6f}")
        print(f"P95: {lat['p95']:.6f} | P99: {lat['p99']:.6f}")
        print(f"吞吐量: {lat['throughput']:.2f} task/s")
    print("="*50)

    # 7. 保存完整 JSON 报告
    json_path = data_dir / 'analysis_report.json'
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=4, ensure_ascii=False)
    print(f"\n[完成] 完整报告已保存至: {json_path}")

if __name__ == "__main__":
    main()