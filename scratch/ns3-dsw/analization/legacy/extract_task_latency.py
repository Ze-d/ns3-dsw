#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
任务延迟提取脚本
功能：从 pro_sink_stats.xml 解析任务数据，生成 task_latency_trace.csv
"""

import xml.etree.ElementTree as ET
import csv
import os
import sys
import matplotlib.pyplot as plt
from matplotlib import font_manager
from pathlib import Path

# ================= 配置区域 =================

# 1. 动态解析数据目录 (优先级: 命令行参数 > 环境变量 > 默认相对路径)
if len(sys.argv) > 1:
    DATA_DIR = Path(sys.argv[1]).resolve()
elif os.environ.get("TARGET_ANALYSIS_DIR"):
    DATA_DIR = Path(os.environ.get("TARGET_ANALYSIS_DIR")).resolve()
elif os.environ.get("OUT_DIR"):
    DATA_DIR = Path(os.environ.get("OUT_DIR")).resolve()
else:
    # 默认回退: 脚本所在目录/../scratch/ns3-dsw/out
    DATA_DIR = Path(__file__).resolve().parent.parent / "scratch/ns3-dsw/out"

print(f"[配置] 数据分析目录: {DATA_DIR}")

# 2. 基于 DATA_DIR 设置文件路径 (使用 Path 对象)
XML_FILE_PATH = DATA_DIR / 'pro_sink_stats.xml'
CSV_OUTPUT_PATH = DATA_DIR / 'task_latency_trace.csv'
VIS_OUTPUT_DIR = DATA_DIR / 'visualization'

# 字体设置 - 使用 CLAUDE.md 规范
FONT_NAME = 'Maple Mono Normal NF CN'
FONT_FILE = 'MapleMonoNormal-NF-CN-Regular.ttf'
# 尝试适配你的绝对路径，或者使用相对路径
FONT_PATH_SYSTEM = '/media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/.local/share/fonts/MapleMonoNormal-NF-CN-Regular.ttf'

# ===========================================

def configure_matplotlib_font():
    """
    配置 Matplotlib 使用指定的 Maple Mono 字体
    使用 CLAUDE.md 规范：300 DPI，Maple Mono Normal NF CN 字体
    """
    try:
        font_path = None

        # 优先从已知系统路径加载
        if os.path.exists(FONT_PATH_SYSTEM):
            font_path = FONT_PATH_SYSTEM

        # 尝试系统查找
        if not font_path:
            for f in font_manager.fontManager.ttflist:
                if FONT_NAME in f.name:
                    font_path = f.fname
                    break

        # 尝试本地查找 (当前目录)
        if not font_path and os.path.exists(FONT_FILE):
            font_path = FONT_FILE

        if font_path:
            font_manager.fontManager.addfont(font_path)
            plt.rcParams['font.family'] = FONT_NAME
            print(f"[配置] 成功加载字体: {FONT_NAME}")
        else:
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'DejaVu Sans']
            print(f"[警告] 未找到 {FONT_NAME}，已回退默认字体。")

        # 配置中文字体支持和负号显示
        plt.rcParams['axes.unicode_minus'] = False

        # 设置 DPI 为 300 (CLAUDE.md 规范要求)
        plt.rcParams['figure.dpi'] = 300
        plt.rcParams['savefig.dpi'] = 300

    except Exception as e:
        print(f"[错误] 字体配置失败: {e}")

def parse_xml_to_csv():
    """解析 XML 并生成 CSV"""
    print(f"[信息] 开始解析: {XML_FILE_PATH}")

    if not XML_FILE_PATH.exists():
        print(f"[错误] 找不到输入文件: {XML_FILE_PATH}")
        return False

    pending_tasks = {}
    completed_tasks = []
    
    stats = {'generated': 0, 'completed': 0, 'unmatched': 0}

    try:
        # 使用 str() 转换 Path 对象，以防旧版 ElementTree 不支持 Path
        context = ET.iterparse(str(XML_FILE_PATH), events=('end',))
        for event, elem in context:
            if elem.tag == 'Event':
                evt_type = elem.get('type')
                
                if evt_type == 'EdgeSend':
                    stats['generated'] += 1
                    edge_id = elem.get('Edge-Id')
                    task_id = elem.get('Task-Id')
                    time_str = elem.get('Time')
                    
                    key = (edge_id, task_id)
                    pending_tasks[key] = {
                        'start_time': float(time_str),
                        'producer': edge_id
                    }

                elif evt_type == 'CoreComp':
                    edge_id = elem.get('Edge-Id')
                    task_id = elem.get('Task-Id')
                    core_id = elem.get('Core-Id')
                    end_time = float(elem.get('Time'))
                    
                    key = (edge_id, task_id)
                    
                    if key in pending_tasks:
                        start_data = pending_tasks[key]
                        start_time = start_data['start_time']
                        latency = end_time - start_time
                        
                        record = {
                            'Task_Unique_ID': task_id, 
                            'Producer_Node': start_data['producer'],
                            'Consumer_Node': core_id,
                            'Start_Time_s': f"{start_time:.6f}",
                            'End_Time_s': f"{end_time:.6f}",
                            'Total_Latency_s': f"{latency:.6f}"
                        }
                        completed_tasks.append(record)
                        stats['completed'] += 1
                        del pending_tasks[key]
                    else:
                        stats['unmatched'] += 1
                
                elem.clear()

    except ET.ParseError as e:
        print(f"[Error] XML 解析错误: {e}")
        return False
    except Exception as e:
        print(f"[Error] 未知错误: {e}")
        return False

    if completed_tasks:
        headers = ['Task_Unique_ID', 'Producer_Node', 'Consumer_Node', 'Start_Time_s', 'End_Time_s', 'Total_Latency_s']
        
        # 确保父目录存在 (理论上 DATA_DIR 肯定存在，但是个好习惯)
        CSV_OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)

        with open(CSV_OUTPUT_PATH, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=headers)
            writer.writeheader()
            writer.writerows(completed_tasks)
        
        print(f"[成功] CSV 生成完毕: {CSV_OUTPUT_PATH}")
        if stats['generated'] > 0:
            print(f"    - 匹配成功率: {stats['completed']}/{stats['generated']} ({stats['completed']/stats['generated']:.1%})")
        return True
    else:
        print("[警告] 无数据生成。")
        return False

def generate_visualizations():
    """生成直方图和箱线图"""
    if not CSV_OUTPUT_PATH.exists():
        return

    # 确保输出目录存在
    if not VIS_OUTPUT_DIR.exists():
        VIS_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        print(f"[信息] 创建目录: {VIS_OUTPUT_DIR}")

    latencies = []
    with open(CSV_OUTPUT_PATH, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                latencies.append(float(row['Total_Latency_s']))
            except ValueError:
                pass
    
    if not latencies:
        return

    configure_matplotlib_font()
    
    # --- 图表 1: 直方图 (分布概览) ---
    plt.figure(figsize=(10, 6), dpi=100)
    n, bins, patches = plt.hist(latencies, bins=40, color='#4c72b0', edgecolor='white', alpha=0.8)
    plt.title('任务端到端时延分布 (Latency Distribution)', fontsize=14)
    plt.xlabel('时延 Latency (s)', fontsize=12)
    plt.ylabel('频次 Frequency', fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.3)
    
    # 添加平均值竖线
    avg_lat = sum(latencies) / len(latencies)
    plt.axvline(avg_lat, color='#c44e52', linestyle='--', linewidth=1.5, label=f'平均值: {avg_lat:.4f}s')
    plt.legend()
    
    hist_path = VIS_OUTPUT_DIR / 'latency_histogram.png'
    plt.savefig(hist_path)
    plt.close()
    print(f"[信息] 生成直方图: {hist_path}")

    # --- 图表 2: 箱线图 (离群点分析) ---
    plt.figure(figsize=(10, 4), dpi=100)
    box = plt.boxplot(latencies, vert=False, patch_artist=True, widths=0.5,
                      flierprops=dict(marker='o', markerfacecolor='#c44e52', markersize=4, linestyle='none'))
    
    for patch in box['boxes']:
        patch.set_facecolor('#55a868')
        patch.set_alpha(0.6)
    for median in box['medians']:
        median.set(color='black', linewidth=1.5)

    plt.title('任务时延箱线图 (Latency Box Plot)', fontsize=14)
    plt.xlabel('时延 Latency (s)', fontsize=12)
    plt.yticks([])
    plt.grid(True, axis='x', linestyle='--', alpha=0.3)
    
    sorted_lat = sorted(latencies)
    min_val = sorted_lat[0]
    max_val = sorted_lat[-1]
    median_val = sorted_lat[len(sorted_lat)//2]
    
    info_text = (f"最大值: {max_val:.4f}s\n"
                 f"中位数: {median_val:.4f}s\n"
                 f"最小值: {min_val:.4f}s")
    
    plt.text(0.95, 0.85, info_text, transform=plt.gca().transAxes, 
             fontsize=10, verticalalignment='top', horizontalalignment='right',
             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    box_path = VIS_OUTPUT_DIR / 'latency_boxplot.png'
    plt.savefig(box_path)
    plt.close()
    print(f"[信息] 生成箱线图: {box_path}")

if __name__ == "__main__":
    if parse_xml_to_csv():
        generate_visualizations()