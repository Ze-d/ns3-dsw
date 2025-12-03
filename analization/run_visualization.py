#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化入口脚本 - 生成图片
功能：
1. 初始化 DataLoader
2. 调用 MetricsCalculator 获取预处理后的数据（如时间序列数据）
3. 调用 lib.plots 中的各个函数生成图片
4. 打印生成进度
"""

import sys
from pathlib import Path
from lib.loader import DataLoader
from lib.metrics import MetricsCalculator
from lib.plots import save_all_plots


def main():
    """主函数"""
    print("\n" + "="*60)
    print(" ns3-dsw 仿真结果可视化生成器")
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

    # 3. 确定可视化输出目录
    viz_dir = data_path / 'visualization'
    viz_dir.mkdir(parents=True, exist_ok=True)
    print(f"[配置] 可视化输出目录: {viz_dir}")
    print()

    try:
        # 4. 初始化 DataLoader
        print("[步骤 1/3] 初始化数据加载器...")
        loader = DataLoader(data_dir)

        # 5. 初始化 MetricsCalculator
        print("[步骤 2/3] 初始化指标计算器...")
        metrics_calc = MetricsCalculator(loader)

        # 6. 生成所有图表
        print("[步骤 3/3] 生成可视化图表...")
        print("-" * 60)

        save_all_plots(metrics_calc, viz_dir)

        print("-" * 60)
        print("\n✅ 可视化完成！")
        print(f"   所有图表已保存到: {viz_dir}")
        print(f"   包含以下文件:")
        print(f"     - task_latency_timeseries.png")
        print(f"     - core_utilization.png")
        print(f"     - power_cost.png")
        print(f"     - latency_histogram.png")
        print(f"     - link_utilization_heatmap.png")
        print(f"     - scheduler_events.png")
        print(f"     - 以及对应的 SVG 格式文件")

    except Exception as e:
        print(f"\n[错误] 可视化过程中发生错误: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
