#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
可视化配置模块
负责 Matplotlib 的全局样式配置、字体加载及路径管理。
"""

import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
from pathlib import Path
import logging
import sys

# 配置日志输出
logging.basicConfig(
    level=logging.INFO, 
    format='[%(levelname)s] %(message)s',
    stream=sys.stdout
)
logger = logging.getLogger(__name__)

def configure_matplotlib():
    """
    配置 Matplotlib 环境：
    1. 动态定位并加载 Maple Mono 字体。
    2. 设置 Linux/Windows 兼容的字体回退列表。
    3. 应用科研绘图常用的样式标准 (300 DPI, Grid, Layout)。
    """
    try:
        # ================= 1. 智能路径定位 =================
        # 获取当前文件 (lib/config.py) 的绝对路径
        current_file_path = Path(__file__).resolve()
        # 项目根目录 (analization/)
        project_root = current_file_path.parent.parent
        # 字体文件的绝对路径
        font_path = project_root / "resource" / "MapleMonoNormal-NF-CN-Regular.ttf"

        custom_font_name = None
        is_custom_font_loaded = False

        # ================= 2. 加载自定义字体 =================
        if font_path.exists():
            try:
                # 将字体文件注册到 matplotlib
                fm.fontManager.addfont(str(font_path))
                
                # 关键步骤：动态获取该字体的内部注册名 (Family Name)
                # 避免硬编码字体名导致的匹配错误
                prop = fm.FontProperties(fname=str(font_path))
                custom_font_name = prop.get_name()
                
                is_custom_font_loaded = True
                logger.info(f"成功加载自定义字体: '{custom_font_name}' (from {font_path.name})")
            except Exception as e:
                logger.warning(f"自定义字体文件存在但在加载时出错: {e}")
        else:
            logger.warning(f"未找到自定义字体文件: {font_path}")

        # ================= 3. 构建字体优先级列表 =================
        # Linux 和 Windows 的常见中文字体备选
        fallback_fonts = [
            'WenQuanYi Micro Hei',   
            'WenQuanYi Zen Hei',     
            'Noto Sans CJK SC',      
            'SimHei',                
            'Microsoft YaHei',       
            'Arial Unicode MS',      
            'DejaVu Sans',           
            'sans-serif'
        ]

        # 如果加载成功，将 Maple Mono 放在首位
        if is_custom_font_loaded and custom_font_name:
            font_family = [custom_font_name] + fallback_fonts
        else:
            font_family = fallback_fonts
            logger.info("已回退到系统默认字体列表。")

        # ================= 4. 更新全局参数 (rcParams) =================
        config_dict = {
            # --- 字体设置 ---
            'font.family': 'sans-serif',    # 显式声明使用无衬线字体族
            'font.sans-serif': font_family, # 设置优先级列表
            'axes.unicode_minus': False,    # 解决负号显示为方块的问题

            # --- 图像质量与尺寸 ---
            'figure.dpi': 300,              # 屏幕显示分辨率
            'savefig.dpi': 300,             # 保存图片分辨率 (高发论文标准)
            'figure.figsize': (10, 6),      # 默认画布大小
            
            # --- 颜色与背景 ---
            'figure.facecolor': 'white',    # 画布背景色
            'axes.facecolor': 'white',      # 坐标轴背景色
            
            # --- 布局与网格 (优化阅读体验) ---
            'axes.grid': True,              # 默认开启网格
            'grid.alpha': 0.3,              # 网格透明度 (淡一点，不抢眼)
            'grid.linestyle': '--',         # 网格线型
            'figure.autolayout': True,      # 自动调整布局避免标签被截断
            
            # --- 文字大小 ---
            'font.size': 10,
            'axes.titlesize': 12,
            'axes.labelsize': 11,
            'legend.fontsize': 10,
        }
        
        plt.rcParams.update(config_dict)
        # logger.info("Matplotlib 全局配置已更新。")

    except Exception as e:
        logger.error(f"字体配置过程中发生未预期的错误: {e}")

# 模块被导入时自动执行
configure_matplotlib()