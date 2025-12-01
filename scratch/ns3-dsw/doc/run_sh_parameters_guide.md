# run.sh 参数调整指南

## 概述

本指南详细介绍了 `scratch/ns3-dsw/scripts/run.sh` 脚本中可调参数的含义、作用机制、推荐值范围以及调优策略。这些参数主要用于控制**价格感知调度器**的行为，该调度器基于电价、网络负载和任务积压情况动态选择最优的处理节点。

---

## 参数分类与作用

### 1. 价格感知调度开关

#### `--enablePriceAwareScheduling` (默认: 1)

**作用**: 启用或禁用价格感知调度功能。

**取值**:
- `0`: 禁用价格感知调度（使用默认调度策略）
- `1`: 启用价格感知调度（推荐）

**调优建议**: 在开发调试阶段可设为0，实验阶段设为1。

---

### 2. 负载衰减相关参数

#### `--loadDecayFactor` (默认: 0.5)

**作用**: 控制消费者在高负载时的处理速度衰减程度。该参数直接影响处理时间的计算。

**数学模型**:
```
有效处理速率 = tasksPerSecond × (1.0 - utilization × loadDecayFactor)
处理时间 = 1 / 有效处理速率
```

**推荐值范围及场景**:

| 值 | 负载影响 | 适用场景 | 调度行为 |
|---|---------|----------|----------|
| 0.0 | 无衰减（固定速率） | 高性能计算、短任务 | 倾向于选择处理能力强的节点 |
| 0.3 | 轻微衰减 | 负载均衡场景 | 对负载变化不敏感，稳定性好 |
| **0.5** | **中等衰减** | **通用场景（推荐）** | **平衡性能和负载** |
| 0.8 | 强衰减 | 极端负载均衡场景 | 负载敏感，避免选择繁忙节点 |
| 1.0 | 完全衰减 | 极端负载均衡 | 对负载极其敏感 |

**调优策略**:
- **高利用率网络** (利用率 > 70%): 降低至 0.3-0.4
- **低利用率网络** (利用率 < 30%): 提升至 0.6-0.8
- **追求性能优先**: 降低至 0.2-0.3
- **追求负载均衡**: 提升至 0.7-0.9

---

### 3. 拥塞控制相关参数

#### `--maxCongestionPenalty` (默认: 0.5)

**作用**: 作为拥塞惩罚的软上限权重（W_base），通过tanh函数平滑限制拥塞成本。

**数学模型**:
```
拥塞成本 = W_base × tanh(T_delay / K_c)
```

**推荐值范围**: 0.1 - 2.0

**调优策略**:

| 场景 | 推荐值 | 原因 |
|------|--------|------|
| 延迟敏感应用 | 0.8 - 1.5 | 强惩罚拥塞，确保低延迟 |
| 容忍延迟应用 | 0.2 - 0.4 | 弱惩罚拥塞，允许延迟 |
| **通用场景** | **0.5** | **平衡延迟和成本** |
| 价格优先场景 | 0.1 - 0.3 | 优先考虑电价，最小化拥塞惩罚 |

#### `--congestionSensitivity` (默认: 2.0)

**作用**: 拥塞敏感度（K_c），以秒为单位。控制tanh函数对延迟的敏感程度。

**推荐值范围**: 0.5 - 10.0

**调优策略**:

| 网络延迟状况 | 推荐值 | 说明 |
|-------------|--------|------|
| 低延迟网络 (< 1s) | 0.5 - 1.0 | 更敏感的拥塞检测 |
| 中延迟网络 (1-3s) | **2.0 - 3.0** | **标准设置（推荐）** |
| 高延迟网络 (> 3s) | 4.0 - 8.0 | 降低敏感度避免过度惩罚 |
| 容忍高延迟 | 8.0 - 10.0 | 大幅降低拥塞敏感性 |

**注意**: K_c 与 MAX_CONGESTION_PENALTY 协同工作，K_c 越小，拥塞惩罚增长越陡峭。

---

### 4. 生产者压力感知参数

#### `--maxProducerPenalty` (默认: 3.0)

**作用**: 生产者紧急度乘数的最大值（W_prod）。当生产者积压任务过多时，增加时间成本权重，迫使调度器选择更好的路径。

**数学模型**:
```
紧急度乘数 = 1.0 + (W_prod × tanh(ProducerPending / K_p))
最终时间成本 = 拥塞成本 × 紧急度乘数
```

**推荐值范围**: 1.0 - 5.0

**调优策略**:

| 生产者负载状况 | 推荐值 | 效果 |
|---------------|--------|------|
| 任务积压严重 | 4.0 - 5.0 | 强感知生产压力，快速调整 |
| **正常负载** | **2.0 - 3.0** | **标准设置（推荐）** |
| 负载较轻 | 1.0 - 2.0 | 弱感知生产者压力 |
| 不关注生产者压力 | 0.5 - 1.0 | 几乎不考虑生产者状态 |

#### `--producerSensitivity` (默认: 100.0)

**作用**: 生产者敏感度（K_p），以任务数为单位。控制多少待处理任务开始触发紧急调度。

**推荐值范围**: 10.0 - 500.0

**调优策略**:

| 任务特性 | 推荐值 | 说明 |
|----------|--------|------|
| 短任务、小规模 | 20 - 50 | 少量积压即触发紧急调度 |
| **中等任务** | **100 - 200** | **标准设置（推荐）** |
| 长任务、大规模 | 300 - 500 | 容忍更多积压才触发调度 |
| 完全不感知 | 1000+ | 禁用生产者压力感知 |

**临界点计算**: 当生产者积压任务数 = K_p 时，tanh(1.0) ≈ 0.7616，即乘数增长至 1.0 + W_prod × 0.7616。

---

### 5. 日志控制参数

#### `--logLevel` (默认: "off")

**作用**: 控制NS-3仿真系统的日志输出级别。

**取值**:
- `off`: 禁用所有日志（生产环境推荐）
- `all`: 启用所有日志（调试时使用）
- `PriceAwareScheduler`: 仅启用调度器日志
- `info`, `warn`, `error`: 指定级别

**调优建议**: 开发阶段使用 `all`，生产运行使用 `off`，仅排查问题时启用特定模块日志。

---

## 成本计算公式总览

调度器总成本由三部分组成：

```
TotalCost = ProcessingCost + TimeCost
```

### 1. 处理成本（ProcessingCost）
```
ProcessingCost = Price(T_start) × ProcessingTime
ProcessingTime = 1 / [tasksPerSecond × (1.0 - utilization × loadDecayFactor)]
```

### 2. 时间成本（TimeCost）
```
TimeCost = 拥塞成本 × 紧急度乘数
         = [W_base × tanh(T_delay / K_c)] × [1.0 + (W_prod × tanh(PendingTasks / K_p))]
其中:
T_delay = TTTT + (smoothedQueueLength × ProcessingTime)
```

### 3. 完整公式
```
TotalCost = Price(T_start) × ProcessingTime
          + [W_base × tanh((TTTT + smoothedQueueLength × ProcessingTime) / K_c)]
            × [1.0 + (W_prod × tanh(PendingTasks / K_p))]
```

---

## 预设配置方案

### 方案A: 性能优先
适用于追求最小化处理时间和延迟的场景。

```bash
sh run.sh \
  --enablePriceAwareScheduling=1 \
  --loadDecayFactor=0.2 \
  --maxCongestionPenalty=0.8 \
  --congestionSensitivity=1.5 \
  --maxProducerPenalty=4.0 \
  --producerSensitivity=50.0 \
  --logLevel=off
```

**特点**: 负载衰减弱、拥塞惩罚强、敏感度适中、生产者压力感知强。

### 方案B: 成本优先
适用于追求最小化电价成本的场景。

```bash
sh run.sh \
  --enablePriceAwareScheduling=1 \
  --loadDecayFactor=0.5 \
  --maxCongestionPenalty=0.2 \
  --congestionSensitivity=4.0 \
  --maxProducerPenalty=2.0 \
  --producerSensitivity=150.0 \
  --logLevel=off
```

**特点**: 弱化拥塞惩罚、强化电价优化、容忍延迟、生产者压力感知适中。

### 方案C: 负载均衡优先
适用于追求各节点负载均衡的场景。

```bash
sh run.sh \
  --enablePriceAwareScheduling=1 \
  --loadDecayFactor=0.8 \
  --maxCongestionPenalty=0.6 \
  --congestionSensitivity=2.5 \
  --maxProducerPenalty=3.0 \
  --producerSensitivity=200.0 \
  --logLevel=off
```

**特点**: 负载衰减强、对负载变化敏感、各参数平衡设置。

### 方案D: 默认配置
适用于通用场景的稳定配置。

```bash
sh run.sh \
  --enablePriceAwareScheduling=1 \
  --loadDecayFactor=0.5 \
  --maxCongestionPenalty=0.5 \
  --congestionSensitivity=2.0 \
  --maxProducerPenalty=3.0 \
  --producerSensitivity=100.0 \
  --logLevel=off
```

---

## 参数调优流程

### Step 1: 确定优先目标
明确调度策略的主要目标：
- 性能优先？成本优先？还是负载均衡？

### Step 2: 选择预设方案
根据目标选择接近的预设方案作为起点。

### Step 3: 单参数微调
一次只调整一个参数，观察效果：

#### 调整顺序建议
1. **优先调整**: `loadDecayFactor` (直接影响处理时间)
2. **其次调整**: `maxCongestionPenalty` + `congestionSensitivity` (控制延迟惩罚)
3. **最后调整**: `maxProducerPenalty` + `producerSensitivity` (生产者感知)

### Step 4: 观察指标
对比以下指标评估调优效果：
- **处理延迟** (平均/最大TTFB)
- **电价成本** (总处理成本)
- **负载均衡度** (节点利用率方差)
- **任务积压** (生产者队列长度)

### Step 5: 迭代优化
根据观察结果迭代调整参数，直到达到满意效果。

---

## 注意事项

1. **参数耦合**: 多个参数之间存在协同作用，调整时需整体考虑。

2. **Tanh函数的饱和性**: 当输入值超过3时，tanh函数接近饱和（≈1），此时再增加输入值效果不明显。

3. **EWMA平滑**: 队列长度使用指数加权移动平均进行平滑，避免短期波动影响决策。

4. **TTTT新鲜度**: TTTT数据超过0.25秒将使用RTT×K_Factor估算，可能影响精度。

5. **最小处理速度保证**: 即使在高负载下，处理速度也不会低于理论速度的10%。

---

## 常见问题

### Q1: 调度结果不符合预期？
A1: 检查 `--logLevel=PriceAwareScheduler` 启用调度器日志，查看每次决策的详细成本分解。

### Q2: 如何禁用价格感知调度？
A2: 设置 `--enablePriceAwareScheduling=0`，将使用默认调度策略。

### Q3: 参数设置是否影响仿真结果重现？
A3: 是的。所有参数都会记录在 `out/<timestamp>/run_info.txt` 中，确保可重现性。

### Q4: 如何比较不同参数配置的效果？
A4: 比较各输出目录中的关键指标：
- 平均处理延迟: `out/*/flowstats.csv`
- 节点利用率: `out/*/node_util.xml`
- 调度决策: `out/*/scheduler_events.xml`

### Q5: 生产者敏感度设置为0会怎样？
A5: `producerSensitivity` 不能为0（会导致除零错误）。如需禁用生产者压力感知，建议设为 1000+。

---

## 相关文件

- **调度器实现**: `src/pro-sink-app/model/price-aware-scheduler.{cc,h}`
- **输入数据**: `scratch/ns3-dsw/data/{nodes.csv, links.csv, daily_price.csv}`
- **输出结果**: `scratch/ns3-dsw/out/<timestamp>/`
- **运行脚本**: `scratch/ns3-dsw/scripts/run.sh`

---

**最后更新**: 2025-12-01
**版本**: v1.0
