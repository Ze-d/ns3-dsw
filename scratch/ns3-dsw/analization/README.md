# ns3-dsw 仿真结果分析与可视化工具

本工具包提供了基于 ns3-dsw 仿真数据的可视化图表生成和KPI计算脚本，用于分析网络仿真结果。

## 📊 生成的图表类型

### 1. 核心利用率折线图 (core_utilization.png)
- **数据源**: `out/node_util.xml`
- **展示内容**: Core-2、Core-6、Core-9 的利用率变化趋势
- **特色**: 应用滚动平均（窗口大小=10）平滑处理，清晰展示宏观调度趋势
- **用途**: 分析核心负载分布和调度器的工作模式

### 2. 节点总电费折线图 (power_cost.png)
- **数据源**: `out/power_cost_node2.xml`, `out/power_cost_node6.xml`, `out/power_cost_node9.xml`
- **展示内容**: 三个节点的累计电费随时间变化
- **用途**: 比较不同节点的运营成本，分析成本优化策略

### 3. 链路占用率热力图 (link_utilization_heatmap.png)
- **数据源**: `out/link_util.xml`
- **展示内容**: 38个链路方向（双向）的利用率热力图
- **Y轴**: 链路方向 (格式: L{ID}:{源}→{目标})
- **X轴**: 时间 (秒)
- **用途**: 监控全网链路瓶颈，发现非对称流量模式

### 4. 消费者任务数百分比堆积面积图 (consumer_task_stacked_area.png)
- **数据源**: `out/pro_sink_stats.xml`
- **展示内容**: 三个消费者（10.0.2.2, 10.0.6.2, 10.0.9.2）的任务百分比分布
- **时间窗口**: 0.5秒
- **用途**: 分析调度器在不同时间段的分配策略变化

### 5. 调度器决策散点图 (scheduler_decision_plot.png)
- **数据源**: `out/scheduler_events.xml`
- **展示内容**: 生产者节点的调度决策分布
- **用途**: 分析调度器抖动(Thrashing)现象，监控连接切换模式

## 📊 KPI 计算

本工具还提供七个关键性能指标的计算：

1. **总电价** - 统计所有节点的累计电费
2. **整体平均算力利用率** - 消费者核心的平均利用率
3. **整体平均延迟** - 网络流的平均延迟
4. **整体链路平均带宽** - 所有链路的平均带宽
5. **整体链路平均利用率** - 所有链路的平均利用率
6. **完成总任务数** - 所有计算核心完成的任务总和
7. **平均每任务电价** - 总电价除以完成任务数

使用方法：
```bash
python3 calculate_kpi.py
```

## 🚀 快速开始

### 方法一：一键生成所有图表（推荐）

```bash
cd scratch/ns3-dsw/analization
python3 generate_all_visualizations.py
```

此命令将自动运行所有5个可视化脚本，生成完整的图表集合。

### 方法二：计算KPI统计量

```bash
# 计算七个关键性能指标
python3 calculate_kpi.py
```

### 方法三：单独运行某个脚本

```bash
# 生成核心利用率折线图
python3 core_utilization_line_chart.py

# 生成节点电费折线图
python3 power_cost_line_chart.py

# 生成链路利用率热力图
python3 link_utilization_heatmap.py

# 生成消费者任务堆积面积图
python3 consumer_task_stacked_area.py

# 生成调度器事件散点图
python3 scheduler_events.py
```

## 📂 文件结构

```
scratch/ns3-dsw/
├── analization/                           # 可视化分析脚本目录
│   ├── README.md                          # 本文档
│   ├── config.py                          # 公共配置（字体、样式等）
│   ├── generate_all_visualizations.py     # 一键生成脚本
│   ├── calculate_kpi.py                   # KPI计算脚本
│   ├── core_utilization_line_chart.py     # 核心利用率折线图
│   ├── power_cost_line_chart.py           # 节点电费折线图
│   ├── link_utilization_heatmap.py        # 链路利用率热力图
│   ├── consumer_task_stacked_area.py      # 消费者任务堆积面积图
│   └── scheduler_events.py                # 调度事件可视化脚本
└── out/
    └── visualization/                     # 图表输出目录
        ├── core_utilization.png/.svg      # 核心利用率图表
        ├── power_cost.png/.svg            # 节点电费图表
        ├── link_utilization_heatmap.png/.svg # 链路利用率热力图
        ├── consumer_task_stacked_area.png/.svg # 消费者任务图表
        └── scheduler_decision_plot.png/.svg # 调度决策图表
```

## 🔧 依赖环境

- Python 3.x
- pandas
- matplotlib
- numpy
- xml.etree.ElementTree (标准库)

## 📈 输出文件说明

每个图表会生成两种格式：
- **PNG格式**: 适合嵌入文档、演示文稿
- **SVG格式**: 矢量图，无损缩放，适合学术论文

所有文件保存在 `scratch/ns3-dsw/out/visualization/` 目录下。

## ⚠️ 注意事项

1. **数据文件依赖**: 运行前请确保仿真数据文件已存在于 `out/` 目录中
   - node_util.xml
   - power_cost_node2.xml
   - power_cost_node6.xml
   - power_cost_node9.xml
   - link_util.xml
   - pro_sink_stats.xml
   - scheduler_events.xml
   - flowstats.csv

2. **字体警告**: 如果看到中文字体警告，不影响图表生成，仅可能影响中文标签显示

3. **性能**: 热力图生成可能需要较长时间（~10秒），其他图表生成通常在1-2秒内完成

## 📊 图表分析示例

### 核心利用率分析
- 观察三个核心的负载均衡情况
- 识别高负载时间段
- 分析调度策略的有效性

### 电费分析
- 比较三个节点的能耗成本
- 识别成本增长最快的时段
- 优化能耗调度策略

### 链路利用率分析
- 发现网络瓶颈链路
- 识别不对称流量模式
- 优化链路配置

### 任务分配分析
- 分析调度器的实时分配策略
- 识别分配不均的时间窗口
- 评估负载均衡效果

## 🔄 工作流程

```
运行仿真 → 生成数据文件 → 运行可视化脚本 → 查看分析结果
```

## 📝 技术细节

### 数据处理流程

1. **XML解析**: 使用 `xml.etree.ElementTree` 解析仿真输出
2. **数据转换**: 转换为 pandas DataFrame 便于处理
3. **时间窗口聚合**: 根据需求对数据进行时间窗口聚合
4. **平滑处理**: 对高频数据应用滚动平均
5. **图表生成**: 使用 matplotlib 生成高质量图表

### 自定义配置

如需修改图表样式、颜色方案或时间窗口大小，请编辑对应的脚本文件中的参数。

## 🐛 故障排除

**问题1**: 脚本运行失败，提示"数据文件不存在"
- **解决**: 请先运行仿真，生成所需的数据文件

**问题2**: matplotlib字体警告
- **解决**: 这是常见的字体配置问题，不影响图表生成

**问题3**: 热力图生成过慢
- **解决**: 热力图涉及大量数据处理，这是正常现象

## 📞 技术支持

如有问题，请检查：
1. 数据文件是否存在且格式正确
2. Python依赖是否完整安装
3. 输出目录是否有写入权限

---

**版本**: 1.0
**最后更新**: 2025-11-12
