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

#### `--loadDecayFactor` (默认: 0.6)

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
| **0.6** | **中等衰减** | **通用场景（推荐）** | **平衡性能和负载** |
| 0.8 | 强衰减 | 极端负载均衡场景 | 负载敏感，避免选择繁忙节点 |
| 1.0 | 完全衰减 | 极端负载均衡 | 对负载极其敏感 |

**调优策略**:
- **追求性能优先**: 降低至 0.1-0.4
- **追求负载均衡**: 提升至 0.6-0.9

---

### 3. 拥塞控制相关参数

#### `--maxCongestionPenalty` (默认: 10)

**作用**: 作为拥塞惩罚的软上限权重（W_base），通过tanh函数平滑限制拥塞成本。

**数学模型**:
```
拥塞成本 = W_base × tanh(T_delay / K_c)
```

**推荐值范围**: 5 - 15

**调优策略**:

| 场景 | 推荐值 | 原因 |
|------|--------|------|
| 延迟敏感应用 | 10 - 15 | 强惩罚拥塞，确保低延迟 |
| **通用场景** | **10** | **平衡延迟和成本** |
| 价格优先场景 | 5 - 10 | 优先考虑电价，最小化拥塞惩罚 |

#### `--congestionSensitivity` (默认: 4.0)

**作用**: 拥塞敏感度（K_c），以秒为单位。控制tanh函数对延迟的敏感程度。

**推荐值范围**: 2 - 10.0

**调优策略**:

| 网络延迟状况 | 推荐值 | 说明 |
|-------------|--------|------|
| 低延迟网络 (< 1s) | 2.0 - 4.0 | 更敏感的拥塞检测 |
| 中延迟网络 (1-3s) | **4.0** | **标准设置（推荐）** |
| 高延迟网络 (> 3s) | 4.0 - 10.0 | 大幅降低拥塞敏感性 |

**注意**: K_c 与 MAX_CONGESTION_PENALTY 协同工作，K_c 越小，拥塞惩罚增长越陡峭。

---

### 4. 生产者压力感知参数

#### `--maxProducerPenalty` (默认: 1.0)

**作用**: 生产者紧急度乘数的最大值（W_prod）。当生产者积压任务过多时，增加时间成本权重，迫使调度器选择更好的路径。

**数学模型**:
```
紧急度乘数 = 1.0 + (W_prod × tanh(ProducerPending / K_p))
最终时间成本 = 拥塞成本 × 紧急度乘数
```

**推荐值范围**: 1.0 - 3.0

**调优策略**:

| 生产者负载状况 | 推荐值 | 效果 |
|---------------|--------|------|
| 任务积压严重 | 2.0 - 5.0 | 强感知生产压力，快速调整 |
| **正常负载** | **1.0** | **标准设置（推荐）** |
| 负载较轻 | 0.5 - 1.0 | 弱感知生产者压力 |
| 不关注生产者压力 | 0 - 0.5 | 几乎不考虑生产者状态 |

#### `--producerSensitivity` (默认: 150.0)

**作用**: 生产者敏感度（K_p），以任务数为单位。控制多少待处理任务开始触发紧急调度。

**推荐值范围**: 20.0 - 300.0

**调优策略**:

| 任务特性 | 推荐值 | 说明 |
|----------|--------|------|
| 短任务、小规模 | 20 - 100 | 少量积压即触发紧急调度 |
| **中等任务** | **150** | **标准设置（推荐）** |
| 长任务、大规模 | 200 - 300 | 容忍更多积压才触发调度 |

**临界点计算**: 当生产者积压任务数 = K_p 时，tanh(1.0) ≈ 0.7616，即乘数增长至 1.0 + W_prod × 0.7616。

---

### 5. 日志控制参数

#### `--logLevel` (默认: "all")

**作用**: 控制NS-3仿真系统的日志输出级别。

**取值**:
- `off`: 禁用所有日志（生产环境推荐）
- `all`: 启用所有日志（调试时使用）
- `info`, `warn`, `error`: 指定级别

---

## 成本计算公式总览

调度器总成本由三部分组成：

$$
TotalCost = ProcessingCost + TimeCost
$$

### 1. 处理成本（ProcessingCost）
$$
ProcessingCost = Price(T_{start}) × ProcessingTime
$$
$$
ProcessingTime =  \frac {1}{[tasksPerSecond × (1.0 - utilization × loadDecayFactor)]} 
$$

### 2. 时间成本（TimeCost）
$$
TimeCost = 拥塞成本 × 紧急度乘数
         = [W_base × tanh(T_{delay} / K_c)] × [1.0 + (W_{prod} × tanh(PendingTasks / K_p))]
$$  
  
其中:
$$
T_{delay} = TTTT + (smoothedQueueLength × ProcessingTime) \\ = TTTT + waitTime
$$

### 3. 完整公式

$$
TotalCost = Price(T_{start}) \times ProcessingTime
          + \underbrace{[W_{base} \times \tanh(\frac{TTTT + waitTime}{K_c})]}_{\text{拥塞成本}}
            \times \underbrace{[1.0 + (W_{prod} \times \tanh(\frac{PendingTasks}{K_p}))]}_{\text{紧急度乘数}}
$$

### 4. 公式参数详解

| 符号 | run.sh 参数 | 含义 | 作用 |
|------|-------------|------|------|
| **loadDecayFactor** | `--loadDecayFactor` | 负载衰减因子 | 控制高负载时处理速度的衰减程度。值越大，高负载节点的惩罚越严重 |
| **utilization** | (自动测量) | 节点当前利用率 | 反映节点负载状况，范围0-1 |
| **tasksPerSecond** | (自动测量) | 节点理论处理速率 | 节点的理论最大处理能力 |
| **TTTT** | (自动测量) | 传输时间(TTFB) | 首次字节到达时间，反映网络延迟 |
| **smoothedQueueLength** | (自动测量) | 平滑队列长度 | 使用EWMA平滑的任务队列长度，避免短期波动 |
| **W_base** | `--maxCongestionPenalty` | 拥塞惩罚基数 | 作为拥塞惩罚的软上限权重 |
| **K_c** | `--congestionSensitivity` | 拥塞敏感度 | 控制tanh函数对延迟的敏感程度（单位：秒） |
| **W_prod** | `--maxProducerPenalty` | 生产者惩罚权重 | 生产者紧急度乘数的最大值 |
| **K_p** | `--producerSensitivity` | 生产者敏感度 | 控制多少待处理任务触发紧急调度（单位：个） |
| **PendingTasks** | (自动测量) | 生产者待处理任务数 | 当前生产者队列中的任务总数 |

**公式执行流程**：
1. 计算有效处理速率：`tasksPerSecond × (1.0 - utilization × loadDecayFactor)`
2. 计算处理时间：`1 / 有效处理速率`
3. 计算总延迟：`TTTT + (smoothedQueueLength × ProcessingTime)`
4. 计算拥塞成本：`W_base × tanh(总延迟 / K_c)`
5. 计算紧急度乘数：`1.0 + (W_prod × tanh(PendingTasks / K_p))`
6. 计算总成本：`Price(T_start) × ProcessingTime + 拥塞成本 × 紧急度乘数`

### run.sh 控制的五个调度器参数速查

在 `scratch/ns3-dsw/scripts/run.sh` 中可以调整的五个参数与公式变量的对应关系：

| run.sh 参数 | 公式变量 | 默认值 | 参数说明 |
|------------|---------|--------|----------|
| `--loadDecayFactor` | `loadDecayFactor` | 0.6 | 负载衰减因子（见第2章详细说明） |
| `--maxCongestionPenalty` | `W_base` | 10 | 拥塞惩罚基数（见第3章详细说明） |
| `--congestionSensitivity` | `K_c` | 4.0 | 拥塞敏感度（见第3章详细说明） |
| `--maxProducerPenalty` | `W_prod` | 1.0 | 生产者惩罚权重（见第4章详细说明） |
| `--producerSensitivity` | `K_p` | 150.0 | 生产者敏感度（见第4章详细说明） |

---

## 预设配置方案

### 方案A: 性能优先
适用于追求最小化处理时间和延迟的场景。

```bash
bash scratch/ns3-dsw/scripts/run.sh --enablePriceAwareScheduling 1 --loadDecayFactor 0.3 --maxCongestionPenalty 15 --congestionSensitivity 2 --maxProducerPenalty 2.0 --producerSensitivity 75.0 --logLevel all
```

**特点**: 负载衰减弱、拥塞惩罚强、敏感度适中、生产者压力感知强。

### 方案B: 电价优先
适用于追求最小化电价成本的场景。

```bash
bash scratch/ns3-dsw/scripts/run.sh --enablePriceAwareScheduling 1 --loadDecayFactor 0.1 --maxCongestionPenalty 5 --congestionSensitivity 10.0 --maxProducerPenalty 0.1 --producerSensitivity 250.0 --logLevel all
```

**特点**: 弱化拥塞惩罚、强化电价优化、容忍延迟、生产者压力感知适中。

### 方案C: 负载均衡优先
适用于追求各节点负载均衡的场景。

```bash
bash scratch/ns3-dsw/scripts/run.sh --enablePriceAwareScheduling 1 --loadDecayFactor 0.6 --maxCongestionPenalty 10 --congestionSensitivity 4.0 --maxProducerPenalty 1.0 --producerSensitivity 150.0 --logLevel off
```

**特点**: 负载衰减强、对负载变化敏感、各参数平衡设置。

---

**最后更新**: 2025-12-4  
**版本**: v1.1  
**Auth**: hurun
