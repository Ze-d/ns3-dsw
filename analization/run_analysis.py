#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
分析入口脚本 - 生成 JSON 报告
功能：
1. 初始化 DataLoader
2. 初始化 MetricsCalculator
3. 调用所有计算方法获取数据
4. 将结果汇总为字典结构
5. 控制台：使用 json.dumps 漂亮地打印摘要
6. 文件：保存为 out/analysis_report.json (机器可读)
"""

import json
import sys
from pathlib import Path
from lib.loader import DataLoader
from lib.metrics import MetricsCalculator


def main():
    """主函数"""
    print("\n" + "="*60)
    print(" ns3-dsw 仿真结果分析报告生成器")
    print("="*60)
    print()

    # 1. 确定数据目录
    if len(sys.argv) > 1:
        data_dir = sys.argv[1]
    elif 'OUT_DIR' in __import__('os').environ:
        data_dir = __import__('os').environ['OUT_DIR']
    else:
        # 默认路径：从 analization 目录到 out 目录
        script_dir = Path(__file__).parent
        data_dir = str(script_dir / '../out')

    print(f"[配置] 数据分析目录: {data_dir}")

    # 2. 验证数据目录
    data_path = Path(data_dir)
    if not data_path.exists():
        print(f"[错误] 数据目录不存在: {data_path}")
        sys.exit(1)

    try:
        # 3. 初始化 DataLoader
        print("\n[步骤 1/3] 初始化数据加载器...")
        loader = DataLoader(data_dir)

        # 4. 初始化 MetricsCalculator
        print("[步骤 2/3] 初始化指标计算器...")
        metrics_calc = MetricsCalculator(loader)

        # 5. 计算所有指标
        print("[步骤 3/3] 计算指标...")
        print()

        # 5.1 全局汇总
        global_summary = metrics_calc.get_global_summary()

        # 5.2 延迟分布统计
        latency_dist = metrics_calc.get_latency_distributions()

        # 5.3 核心利用率统计
        core_util_stats = metrics_calc.get_core_utilization_stats()

        # 5.4 链路利用率统计
        link_util_stats = metrics_calc.get_link_utilization_stats()

        # 5.5 电价统计
        power_cost_stats = metrics_calc.get_power_cost_stats()

        # 5.6 任务完成统计
        task_completion_stats = metrics_calc.get_task_completion_stats()

        # 6. 汇总所有结果
        report = {
            'metadata': {
                'data_directory': str(data_path),
                'analysis_script': 'run_analysis.py',
                'generated_by': 'ns3-dsw MVC Analysis Framework'
            },
            'global_summary': global_summary,
            'latency_distributions': latency_dist,
            'core_utilization': core_util_stats,
            'link_utilization': link_util_stats,
            'power_costs': power_cost_stats,
            'task_completion': task_completion_stats
        }

        # 7. 保存为 JSON 文件
        output_file = data_path / 'analysis_report.json'
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)

        print(f"✅ JSON 报告已保存: {output_file}")
        print()

        # 8. 控制台输出摘要
        print("\n" + "="*60)
        print(" 仿真结果分析摘要")
        print("="*60)
        print()
        print(f"1. 总电价:                    {global_summary['total_power_cost']:.6f}")
        print(f"2. 整体平均算力利用率:         {global_summary['average_core_utilization']:.6f} "
              f"({global_summary['average_core_utilization']*100:.2f}%)")
        print(f"3. 整体平均延迟:              {global_summary['average_delay_ms']:.6f} ms")
        print(f"4. 整体链路平均带宽:           {global_summary['average_bandwidth_mbps']:.6f} Mbps")
        print(f"5. 整体链路平均利用率:         {global_summary['average_link_utilization']:.6f} "
              f"({global_summary['average_link_utilization']:.2f}%)")
        print(f"6. 完成总任务数:              {global_summary['total_completed_tasks']}")
        print(f"7. 平均每任务电价:             {global_summary['average_cost_per_task']:.6f}")
        print()
        print("-" * 60)
        print("延迟分布统计:")
        print(f"  最小值:  {latency_dist['min_latency']:.6f}s")
        print(f"  最大值:  {latency_dist['max_latency']:.6f}s")
        print(f"  平均值:  {latency_dist['avg_latency']:.6f}s")
        print(f"  P95:     {latency_dist['p95_latency']:.6f}s")
        print(f"  P99:     {latency_dist['p99_latency']:.6f}s")
        print(f"  标准差:  {latency_dist['stddev_latency']:.6f}s")
        print(f"  任务数:  {latency_dist['task_count']}")
        print("="*60)

        print("\n✅ 分析完成！")
        print(f"   报告文件: {output_file}")
        print(f"   摘要已显示在上方")

    except Exception as e:
        print(f"\n[错误] 分析过程中发生错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
