#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化配置模块
设置matplotlib的字体支持和全局配置。
"""

import matplotlib.pyplot as plt
from matplotlib import font_manager
import os

# ================= 配置区域 =================
# 字体设置 - 遵循 CLAUDE.md 规范
FONT_NAME = 'Maple Mono Normal NF CN'
FONT_FILE = 'MapleMonoNormal-NF-CN-Regular.ttf'
# 尝试适配你的绝对路径
FONT_PATH_SYSTEM = '/media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/.local/share/fonts/MapleMonoNormal-NF-CN-Regular.ttf'

def configure_matplotlib():
    """
    配置 Matplotlib 使用指定的 Maple Mono 字体
    使用 CLAUDE.md 规范：300 DPI
    """
    try:
        font_path = None

        # 1. 优先从已知系统路径加载
        if os.path.exists(FONT_PATH_SYSTEM):
            font_path = FONT_PATH_SYSTEM

        # 2. 尝试系统查找
        if not font_path:
            for f in font_manager.fontManager.ttflist:
                if FONT_NAME in f.name:
                    font_path = f.fname
                    break

        # 3. 尝试本地查找 (当前目录)
        if not font_path and os.path.exists(FONT_FILE):
            font_path = FONT_FILE

        if font_path:
            font_manager.fontManager.addfont(font_path)
            plt.rcParams['font.family'] = FONT_NAME
            # print(f"[配置] 成功加载字体: {FONT_NAME}")
        else:
            plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'DejaVu Sans']
            print(f"[警告] 未找到 {FONT_NAME}，已回退默认字体。")

        # 配置中文字体支持和负号显示
        plt.rcParams['axes.unicode_minus'] = False

        # 设置 DPI 和 样式
        plt.rcParams['figure.dpi'] = 300
        plt.rcParams['savefig.dpi'] = 300
        plt.rcParams['figure.facecolor'] = 'white'
        plt.rcParams['axes.facecolor'] = 'white'
        plt.rcParams['font.size'] = 10

    except Exception as e:
        print(f"[错误] 字体配置失败: {e}")

# 模块导入时自动运行配置
configure_matplotlib()