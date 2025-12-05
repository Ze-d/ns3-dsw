# ns3算网仿真项目

## 项目简介

本项目是一个基于 ns-3 的网络模拟器，专门用于算网融合环境的拓扑可视化和链路利用率监控。项目实现了负载感知调度算法，支持电价感知调度，能够仿真计算任务的生成、调度和执行过程。

## 项目结构

```
scratch/ns3-dsw/
├── src/                           # 源代码目录
│   ├── application-manager.cc     # 应用程序管理器
│   ├── application-manager.h      # 应用程序管理器头文件
│   ├── data-parser.cc             # 数据解析器
│   ├── data-parser.h              # 数据解析器头文件
│   ├── dsw-structures.h           # 核心数据结构定义
│   └── topo_figure_flowmon_cfg_integrated.cc  # 主仿真程序
├── data/                          # 输入数据目录
│   ├── nodes.csv                  # 节点配置
│   ├── links.csv                  # 链路配置
│   └── daily_price.csv            # 电价数据
├── out/                           # 输出结果目录
├── analization/                   # 数据分析模块
├── scripts/                       # 脚本目录
│   ├── run.sh                     # 运行脚本
│   ├── test.sh                    # 测试脚本
│   └── run_analization.sh         # 分析脚本
└── README.md                      # 本文件
```

## 运行

### 环境要求

- ns-3 网络模拟器
- 支持 C++17 的编译器
- Python 3.x (用于数据分析)

### 快速开始

使用集成脚本运行仿真.推荐配置：

* 负载均衡 

```bash

bash scratch/ns3-dsw/scripts/run.sh 

```

* 电价优先  

```bash

bash scratch/ns3-dsw/scripts/run.sh --enablePriceAwareScheduling 1 --loadDecayFactor 0.1 --maxCongestionPenalty 5 --congestionSensitivity 10 --maxProducerPenalty 0.1 --producerSensitivity 250 --logLevel all 

```

### 参数说明

详细参数说明请参考：[run.sh指导文件](scratch/ns3-dsw/doc/run_sh_parameters_guide.md)

常用参数：
- `--logLevel`: 日志级别 (none, error, warn, info, debug, all)
- `--enablePriceAwareScheduling`: 是否启用电价感知调度 (0/1)
- `--loadDecayFactor`: 负载衰减因子 (0-1,越大)
- `--maxCongestionPenalty`: 最大拥塞惩罚
- `--congestionSensitivity`: 拥塞敏感度
- `--maxProducerPenalty`: 最大生产者惩罚
- `--producerSensitivity`: 生产者敏感度

## 数据文件

### 输入数据

仿真使用的输入数据位于 `scratch/ns3-dsw/data/` 目录：

1. **nodes.csv**: 节点配置
   - 定义网络中所有节点的位置、资源容量等属性
   - 格式：节点ID, X坐标, Y坐标, CPU容量, 内存容量, 类型

2. **links.csv**: 链路配置
   - 定义节点间的网络链路及带宽、延迟等属性
   - 格式：源节点, 目标节点, 带宽, 延迟, 丢包率

3. **daily_price.csv**: 电价数据
   - 定义一天中不同时段的电价
   - 格式：时间段, 电价 (¥/MWh)

### 输出数据

仿真完成后，所有结果保存在 `scratch/ns3-dsw/out/` 下的时间戳子文件夹中：

```
out/20251204_143628/
├── burst_events_node*.xml         # 突发事件记录
├── console_output.log             # 控制台输出日志
├── link_util.xml                  # 链路利用率统计
├── node_util.xml                  # 节点利用率统计
├── power_cost_node*.xml           # 各节点电力成本
├── pro_sink_stats.xml             # 协议统计
├── run_info.txt                   # 运行信息
├── scheduler_events.xml           # 调度事件记录
└── topo_figure.xml                # 拓扑图文件
```

## 输出结果说明

| 文件名 | 说明 |
|--------|------|
| link_util.xml | 链路利用率统计，包含每条链路的带宽使用情况 |
| node_util.xml | 节点利用率统计，包含CPU、内存等资源使用情况 |
| burst_events_node*.xml | 节点突发事件记录，包含任务突发情况 |
| pro_sink_stats.xml | 协议Sink节点统计信息 |
| power_cost_node*.xml | 各节点的电力成本统计 |
| scheduler_events.xml | 调度器事件记录，包含调度决策过程 |
| run_info.txt | 仿真运行的基本信息（配置参数等） |
| console_output.log | 完整的控制台输出日志 |
| topo_figure.xml | NetAnim 动画文件，用于可视化仿真过程 |

## 可视化

项目提供了可视化模块，能够根据仿真结果生成多种图表，帮助分析仿真数据。

### 使用方法

使用分析脚本运行可视化：

```bash
sh scratch/ns3-dsw/scripts/run_analization.sh /media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/project/ns3-dsw/scratch/ns3-dsw/out/20251204_143628
```

脚本会在仿真结果目录中创建 `visualization` 文件夹，存放所有生成的图表。

### 生成的可视化图表

生成的图表文件包括多种格式（PNG 和 SVG）：

#### 资源利用率
- `core_utilization.png/svg`: 核心资源利用率分析
- `link_utilization_heatmap.png/svg`: 链路利用率热力图

#### 任务和延迟
- `task_latency_timeseries.png/svg`: 任务延迟时间序列
- `latency_boxplot.png`: 延迟分布箱线图
- `latency_histogram.png`: 延迟直方图
- `interarrival_time_cdf.png/svg`: 任务到达间隔累计分布函数

#### 负载和队列
- `producer_backlog_evolution.png/svg`: 生产者队列演变
- `consumer_task_stacked_area.png/svg`: 消费者任务堆叠面积图
- `traffic_generation_impulse.png/svg`: 流量生成脉冲图

#### 成本分析
- `power_cost.png/svg`: 电力成本分析
- `price_per_MWh.png/svg`: 电价（每兆瓦时）

#### 调度分析
- `scheduler_decision_plot.png/svg`: 调度决策可视化

## 测试

在提交代码或修改后，必须运行测试脚本验证修改的正确性：

```bash
sh scratch/ns3-dsw/scripts/test.sh
```

测试脚本会编译项目并运行基本测试用例，确保所有功能正常工作。

## 故障排除

### 常见问题

1. **编译错误**
   - 确认已安装 ns-3 网络模拟器
   - 检查编译器版本是否支持 C++17
   - 确保所有依赖项已正确安装

2. **运行失败**
   - 检查输入数据文件格式是否正确
   - 确认路径设置正确
   - 查看控制台输出日志获取详细错误信息

3. **可视化失败**
   - 确认已安装 Python 3.x
   - 检查 Python 依赖包是否完整：`pip install matplotlib pandas seaborn`
   - 确保仿真结果文件完整且格式正确

### 调试技巧

- 使用 `--logLevel all` 开启详细日志输出
- 查看 `console_output.log` 获取运行时详细信息
- 使用 `run_info.txt` 了解仿真配置参数
- 利用 NetAnim 打开 `topo_figure.xml` 文件进行动画回放

## 贡献指南

1. 遵循现有代码风格和命名规范
2. 修改前先运行测试脚本，确保通过所有测试
3. 为新功能添加相应的文档说明
4. 提交前进行充分测试

## 许可证

本项目遵循 MIT 许可证。详细信息请参阅 LICENSE 文件。

## 联系信息

如有问题或建议，请提交 Issue 或联系项目维护者。