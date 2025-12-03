#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化配置模块
设置matplotlib的字体支持和全局配置。
遵循 CLAUDE.md 规范：使用 Maple Mono Normal NF CN 字体，300 DPI
"""

import matplotlib.pyplot as plt
import matplotlib as mpl
import os
from matplotlib import font_manager

# ================= 配置区域 =================
# 字体设置 - 使用 CLAUDE.md 规范
FONT_NAME = 'Maple Mono Normal NF CN'
FONT_FILE = 'MapleMonoNormal-NF-CN-Regular.ttf'
FONT_PATH = '/media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/.local/share/fonts/MapleMonoNormal-NF-CN-Regular.ttf'

def configure_matplotlib():
    """
    配置 Matplotlib 使用指定的 Maple Mono 字体
    使用 CLAUDE.md 规范：300 DPI，Maple Mono Normal NF CN 字体
    """
    try:
        font_path = None

        # 优先从已知路径加载（CLAUDE.md 规范）
        if os.path.exists(FONT_PATH):
            font_path = FONT_PATH

        # 尝试系统查找
        if not font_path:
            for f in font_manager.fontManager.ttflist:
                if FONT_NAME in f.name:
                    font_path = f.fname
                    break

        # 尝试本地查找
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

        # 设置全局图表样式
        plt.rcParams['figure.facecolor'] = 'white'
        plt.rcParams['axes.facecolor'] = 'white'
        plt.rcParams['savefig.facecolor'] = 'white'

        # 设置默认字体大小
        plt.rcParams['font.size'] = 10
        plt.rcParams['axes.labelsize'] = 11
        plt.rcParams['axes.titlesize'] = 12
        plt.rcParams['legend.fontsize'] = 10

    except Exception as e:
        print(f"[错误] 字体配置失败: {e}")

# 立即应用配置
configure_matplotlib()

# ===========================================
