#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
所有可视化图表生成器
调用四个可视化脚本，生成完整的仿真结果图表集合。
"""

import subprocess
import sys
from pathlib import Path
import time

def run_script(script_path):
    """运行Python脚本并捕获输出"""
    print(f"\n{'='*60}")
    print(f"正在运行: {script_path.name}")
    print(f"{'='*60}\n")

    start_time = time.time()

    try:
        result = subprocess.run(
            [sys.executable, script_path],
            capture_output=False,
            text=True,
            cwd=script_path.parent
        )

        end_time = time.time()
        duration = end_time - start_time

        if result.returncode == 0:
            print(f"\n✅ {script_path.name} 运行成功 (耗时: {duration:.2f} 秒)")
            return True
        else:
            print(f"\n❌ {script_path.name} 运行失败 (返回码: {result.returncode})")
            return False

    except Exception as e:
        print(f"\n❌ 运行 {script_path.name} 时发生错误: {e}")
        return False

def main():
    """主函数"""
    # 获取当前脚本所在目录
    script_dir = Path(__file__).parent

    # 脚本目录（使用相对路径）
    viz_dir = script_dir
    output_dir = script_dir / '../out/visualization'

    # 确保输出目录存在
    output_dir.mkdir(exist_ok=True)

    # 所有可视化脚本
    scripts = [
        'core_utilization_line_chart.py',
        'power_cost_line_chart.py',
        'link_utilization_heatmap.py',
        'consumer_task_stacked_area.py',
        'scheduler_events.py'
    ]

    print("\n" + "="*60)
    print(" ns3-dsw 仿真结果可视化生成器")
    print("="*60)
    print(f"\n输出目录: {output_dir}")
    print(f"将要生成 {len(scripts)} 种可视化图表:")
    for i, script in enumerate(scripts, 1):
        print(f"  {i}. {script.replace('_', ' ').replace('.py', '').title()}")

    # 检查数据文件是否存在
    print("\n正在检查数据文件...")
    required_files = [
        script_dir / '../out/node_util.xml',
        script_dir / '../out/power_cost_node2.xml',
        script_dir / '../out/power_cost_node6.xml',
        script_dir / '../out/power_cost_node9.xml',
        script_dir / '../out/link_util.xml',
        script_dir / '../out/pro_sink_stats.xml',
        script_dir / '../out/scheduler_events.xml'
    ]

    missing_files = [f for f in required_files if not Path(f).exists()]
    if missing_files:
        print("❌ 缺少以下数据文件:")
        for f in missing_files:
            print(f"  - {f}")
        print("\n请先运行仿真生成数据文件！")
        sys.exit(1)

    print("✅ 所有数据文件都存在")

    # 运行所有脚本
    print("\n开始生成可视化图表...")
    print()

    success_count = 0
    failed_scripts = []

    for script_name in scripts:
        script_path = viz_dir / script_name
        if script_path.exists():
            if run_script(script_path):
                success_count += 1
            else:
                failed_scripts.append(script_name)
        else:
            print(f"❌ 脚本不存在: {script_path}")
            failed_scripts.append(script_name)

    # 汇总结果
    print("\n" + "="*60)
    print(" 可视化生成完成")
    print("="*60)

    if success_count == len(scripts):
        print(f"\n✅ 全部 {len(scripts)} 个图表生成成功！")

        # 列出生成的文件
        print("\n生成的图表文件:")
        for png_file in sorted(output_dir.glob('*.png')):
            print(f"  📊 {png_file.name}")
        for svg_file in sorted(output_dir.glob('*.svg')):
            print(f"  📊 {svg_file.name} (矢量图)")

        print(f"\n输出目录: {output_dir}")
        print("\n你可以使用以下命令查看图片:")
        print(f"  ls -lh {output_dir}")

    else:
        print(f"\n⚠️  {success_count}/{len(scripts)} 个图表生成成功")
        if failed_scripts:
            print("\n失败的脚本:")
            for script in failed_scripts:
                print(f"  - {script}")

        sys.exit(1)

if __name__ == '__main__':
    main()
