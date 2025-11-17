#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化配置模块
设置matplotlib的字体支持和全局配置。
"""

import matplotlib.pyplot as plt
import matplotlib as mpl

# 设置字体支持
# 使用英文标签以确保跨平台兼容性
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'sans-serif']
plt.rcParams['axes.unicode_minus'] = False

# 设置全局图表样式
plt.rcParams['figure.facecolor'] = 'white'
plt.rcParams['axes.facecolor'] = 'white'
plt.rcParams['savefig.facecolor'] = 'white'

# 设置默认字体大小
plt.rcParams['font.size'] = 10
plt.rcParams['axes.labelsize'] = 11
plt.rcParams['axes.titlesize'] = 12
plt.rcParams['legend.fontsize'] = 10
