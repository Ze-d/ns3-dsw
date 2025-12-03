# ns3-dsw 分析脚本架构重构完成报告

## 🏗️ 架构结构

```
analization/
├── lib/                          # 核心模块目录
│   ├── __init__.py               # 包初始化文件
│   ├── config.py                 # 全局配置（字体、DPI、配色）
│   ├── loader.py                 # 数据加载层 (Model)
│   ├── metrics.py                # 指标计算层 (Model)
│   └── plots.py                  # 绘图层 (View)
├── run_analysis.py               # 分析入口脚本 - 生成JSON报告
├── run_visualization.py          # 可视化入口脚本 - 生成图表
└── legacy/                       # 原始脚本备份
    ├── calculate_kpi.py
    ├── calculate_task_latency_kpi.py
    ├── config.py
    ├── consumer_task_stacked_area.py
    ├── core_utilization_line_chart.py
    ├── extract_task_latency.py
    ├── generate_all_visualizations.py
    ├── link_utilization_heatmap.py
    ├── power_cost_line_chart.py
    ├── scheduler_events.py
    └── task_latency_timeseries.py
```

## 🎯 主要改进

### 1. **消除散乱输出**
- ❌ 原来：各个脚本都有 `print` 输出，格式不统一
- ✅ 现在：统一输出为结构化的 JSON 报告 (`analysis_report.json`)

### 2. **分离关注点**
- **数据加载层** (`lib/loader.py`)：仅负责读取 XML/CSV，转换为 DataFrame
- **指标计算层** (`lib/metrics.py`)：仅负责 KPI 计算，无打印或绘图
- **绘图层** (`lib/plots.py`)：仅负责图表生成，接收数据后绘图

### 3. **统一 Matplotlib 配置**
- ✅ 统一字体：Maple Mono Normal NF CN
- ✅ 统一 DPI：300
- ✅ 统一配色方案：COLOR_SCHEME
- ✅ 统一图表尺寸：CHART_CONFIG

### 4. **提供两个入口脚本**
- **`run_analysis.py`**：生成纯 JSON 报告（机器可读）
- **`run_visualization.py`**：生成所有图表（PNG + SVG）

## 🚀 使用方法

### 生成分析报告

```bash
cd scratch/ns3-dsw/analization
python3 run_analysis.py [数据目录路径]
```

**输出：**
- 控制台：整洁的分析摘要
- 文件：`out/xxxx/analysis_report.json`（完整报告）

### 生成可视化图表

```bash
cd scratch/ns3-dsw/analization
python3 run_visualization.py [数据目录路径]
```

**输出：**
- 控制台：生成进度日志
- 目录：`out/xxxx/visualization/`（包含所有图表）

**生成的图表：**
- `task_latency_timeseries.png` + `.svg` - 延迟时间序列（双轴图）
- `core_utilization.png` + `.svg` - 核心利用率折线图
- `power_cost.png` + `.svg` - 电价折线图
- `latency_histogram.png` - 延迟直方图
- `link_utilization_heatmap.png` - 链路利用率热力图
- `scheduler_events.png` - 调度事件时间线图

