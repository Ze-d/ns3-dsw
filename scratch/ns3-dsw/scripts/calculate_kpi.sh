#!/bin/bash

# KPI统计脚本 - 计算仿真结果的七个关键性能指标
# 使用方法: sh scratch/ns3-dsw/scripts/calculate_kpi.sh

# 设置输出目录
OUT_DIR="scratch/ns3-dsw/out"

# 检查输出目录是否存在
if [ ! -d "$OUT_DIR" ]; then
    echo "错误: 输出目录 $OUT_DIR 不存在"
    exit 1
fi

echo "========================================"
echo "KPI 统计报告"
echo "========================================"
echo ""

# 检查必要的工具
if ! command -v python3 &> /dev/null && [ ! -f /usr/bin/python3 ]; then
    echo "错误: 未找到 python3，请先安装 Python 3"
    exit 1
fi

# 创建Python脚本来计算KPI
python3 << 'PYTHON_SCRIPT'
import xml.etree.ElementTree as ET
import csv
import glob
import os
import sys

def calculate_total_power_cost(out_dir):
    """计算总电价 - 统计所有power_cost_node*.xml文件的最后一个total_cost"""
    print("1. 计算总电价...")

    total_cost = 0.0
    power_files = glob.glob(os.path.join(out_dir, "power_cost_node*.xml"))

    if not power_files:
        print("   警告: 未找到 power_cost_node*.xml 文件")
        return 0.0

    for file in power_files:
        try:
            tree = ET.parse(file)
            root = tree.getroot()

            # 获取所有Sample标签
            samples = root.findall('Sample')
            if samples:
                # 获取最后一个Sample的total_cost
                last_sample = samples[-1]
                total_cost_str = last_sample.get('total_cost', '0')
                cost = float(total_cost_str)
                total_cost += cost

        except Exception as e:
            print(f"   警告: 处理文件 {file} 时出错: {e}")

    print(f"   总电价: {total_cost:.6f}")
    return total_cost

def calculate_average_core_utilization(out_dir):
    """计算整体平均消费者算力利用率"""
    print("\n2. 计算整体平均消费者算力利用率...")

    node_util_file = os.path.join(out_dir, "node_util.xml")

    if not os.path.exists(node_util_file):
        print("   错误: 未找到 node_util.xml 文件")
        return 0.0

    try:
        tree = ET.parse(node_util_file)
        root = tree.getroot()

        total_utilization = 0.0
        event_count = 0

        # 遍历所有Event标签
        for event in root.findall('Event'):
            if event.get('type') == 'CoreUtil':
                util_str = event.get('Utilization', '0')
                util = float(util_str)
                total_utilization += util
                event_count += 1

        if event_count == 0:
            print("   警告: 未找到 CoreUtil 事件")
            return 0.0

        average_utilization = total_utilization / event_count
        print(f"   整体平均算力利用率: {average_utilization:.6f} ({average_utilization*100:.2f}%)")
        return average_utilization

    except Exception as e:
        print(f"   错误: 处理 node_util.xml 时出错: {e}")
        return 0.0

def calculate_average_delay(out_dir):
    """计算整体平均延迟"""
    print("\n3. 计算整体平均延迟...")

    flowstats_file = os.path.join(out_dir, "flowstats.csv")

    if not os.path.exists(flowstats_file):
        print("   错误: 未找到 flowstats.csv 文件")
        return 0.0

    try:
        total_delay = 0.0
        flow_count = 0

        with open(flowstats_file, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                delay_str = row.get('avgDelay_ms', '0')
                delay = float(delay_str)
                total_delay += delay
                flow_count += 1

        if flow_count == 0:
            print("   警告: 未找到流数据")
            return 0.0

        average_delay = total_delay / flow_count
        print(f"   整体平均延迟: {average_delay:.6f} ms")
        return average_delay

    except Exception as e:
        print(f"   错误: 处理 flowstats.csv 时出错: {e}")
        return 0.0

def calculate_average_bandwidth(out_dir):
    """计算整体链路平均带宽"""
    print("\n4. 计算整体链路平均带宽...")

    link_util_file = os.path.join(out_dir, "link_util.xml")

    if not os.path.exists(link_util_file):
        print("   错误: 未找到 link_util.xml 文件")
        return 0.0

    try:
        tree = ET.parse(link_util_file)
        root = tree.getroot()

        total_bandwidth = 0.0
        link_count = 0

        # 获取第一个Sample
        first_sample = root.find('Sample')
        if first_sample is not None:
            # 遍历第一个Sample内的所有Link标签
            for link in first_sample.findall('Link'):
                rate_str = link.get('rateMbps', '0')
                rate = float(rate_str)
                total_bandwidth += rate
                link_count += 1

        if link_count == 0:
            print("   警告: 未找到链路信息")
            return 0.0

        average_bandwidth = total_bandwidth / link_count
        print(f"   整体链路平均带宽: {average_bandwidth:.6f} Mbps")
        return average_bandwidth

    except Exception as e:
        print(f"   错误: 处理 link_util.xml 时出错: {e}")
        return 0.0

def calculate_average_link_utilization(out_dir):
    """计算整体链路平均利用率"""
    print("\n5. 计算整体链路平均利用率...")

    link_util_file = os.path.join(out_dir, "link_util.xml")

    if not os.path.exists(link_util_file):
        print("   错误: 未找到 link_util.xml 文件")
        return 0.0

    try:
        tree = ET.parse(link_util_file)
        root = tree.getroot()

        total_util_pct = 0.0
        sample_count = 0

        # 遍历所有Sample标签
        for sample in root.findall('Sample'):
            # 在每个Sample内遍历所有Link标签
            for link in sample.findall('Link'):
                # 处理AtoB方向
                atoB = link.find('AtoB')
                if atoB is not None:
                    util_str = atoB.get('utilPct', '0')
                    util = float(util_str)
                    total_util_pct += util
                    sample_count += 1

                # 处理BtoA方向
                btoA = link.find('BtoA')
                if btoA is not None:
                    util_str = btoA.get('utilPct', '0')
                    util = float(util_str)
                    total_util_pct += util
                    sample_count += 1

        if sample_count == 0:
            print("   警告: 未找到链路利用率数据")
            return 0.0

        average_utilization = total_util_pct / sample_count
        print(f"   整体链路平均利用率: {average_utilization:.6f} ({average_utilization:.2f}%)")
        return average_utilization

    except Exception as e:
        print(f"   错误: 处理 link_util.xml 时出错: {e}")
        return 0.0

def calculate_total_completed_tasks(out_dir):
    """计算完成总任务数 - 统计所有计算核心累计完成的任务总和"""
    print("\n6. 计算完成总任务数...")

    pro_sink_file = os.path.join(out_dir, "pro_sink_stats.xml")

    if not os.path.exists(pro_sink_file):
        print("   错误: 未找到 pro_sink_stats.xml 文件")
        return 0

    try:
        tree = ET.parse(pro_sink_file)
        root = tree.getroot()

        # 创建字典存储每个核心的最终完成数
        core_max_completed = {}

        # 遍历所有<Event type="CoreComp" .../>标签
        for event in root.findall('Event'):
            if event.get('type') == 'CoreComp':
                core_id = event.get('Core-Id', '')
                completed_str = event.get('TotalCompleted', '0')
                completed = int(completed_str)

                # 更新字典：core_max_completed[Core-Id] = max(...)
                if core_id in core_max_completed:
                    if completed > core_max_completed[core_id]:
                        core_max_completed[core_id] = completed
                else:
                    core_max_completed[core_id] = completed

        # 将所有核心的完成数相加
        total_completed = sum(core_max_completed.values())

        print(f"   完成总任务数: {total_completed}")
        return total_completed

    except Exception as e:
        print(f"   错误: 处理 pro_sink_stats.xml 时出错: {e}")
        return 0

def calculate_average_cost_per_task(total_power_cost, total_completed_tasks):
    """计算平均每任务电价"""
    print("\n7. 计算平均每任务电价...")

    if total_completed_tasks == 0:
        print("   警告: 完成总任务数为0，无法计算平均每任务电价")
        return 0.0

    average_cost = total_power_cost / total_completed_tasks
    print(f"   平均每任务电价: {average_cost:.6f}")
    return average_cost

def main():
    out_dir = "scratch/ns3-dsw/out"

    print("正在分析仿真结果...")
    print("输出目录:", out_dir)
    print()

    # 计算所有KPI
    kpi1 = calculate_total_power_cost(out_dir)
    kpi2 = calculate_average_core_utilization(out_dir)
    kpi3 = calculate_average_delay(out_dir)
    kpi4 = calculate_average_bandwidth(out_dir)
    kpi5 = calculate_average_link_utilization(out_dir)
    kpi6 = calculate_total_completed_tasks(out_dir)
    kpi7 = calculate_average_cost_per_task(kpi1, kpi6)

    print("\n" + "="*50)
    print("KPI 汇总报告")
    print("="*50)
    print(f"1. 总电价:                    {kpi1:.6f}")
    print(f"2. 整体平均算力利用率:         {kpi2:.6f} ({kpi2*100:.2f}%)")
    print(f"3. 整体平均延迟:              {kpi3:.6f} ms")
    print(f"4. 整体链路平均带宽:           {kpi4:.6f} Mbps")
    print(f"5. 整体链路平均利用率:         {kpi5:.6f} ({kpi5:.2f}%)")
    print(f"6. 完成总任务数:              {kpi6}")
    print(f"7. 平均每任务电价:             {kpi7:.6f}")
    print("="*50)

if __name__ == "__main__":
    main()
PYTHON_SCRIPT

echo ""
echo "KPI 计算完成！"