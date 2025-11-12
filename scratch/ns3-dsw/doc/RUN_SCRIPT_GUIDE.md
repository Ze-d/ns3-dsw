# 运行脚本使用指南

## 脚本位置
`/home/hurun/project/ns3-dsw/scratch/ns3-dsw/scripts/run.sh`

## 基本用法

### 默认运行（启用动态调度和完整日志）
```bash
cd /home/hurun/project/ns3-dsw/scratch/ns3-dsw
./scripts/run.sh
```

默认配置：
- ✅ 启用价格感知调度 (`--enablePriceAwareScheduling=1`)
- ✅ 完整日志输出 (`--logLevel=all`)
- ✅ 显示 TTTT/RTT 测量结果
- ✅ 显示调度决策过程
- ✅ 显示连接切换日志

## 命令行参数

### 1. `--enablePriceAwareScheduling <0|1>`
启用或禁用价格感知调度。

```bash
# 启用调度（默认）
./scripts/run.sh --enablePriceAwareScheduling 1

# 禁用调度（使用传统方式）
./scripts/run.sh --enablePriceAwareScheduling 0
```

### 2. `--queuePenaltyFactor <值>`
设置队列惩罚因子。

```bash
# 设置队列惩罚因子为 0.2
./scripts/run.sh --queuePenaltyFactor 0.2

# 设置队列惩罚因子为 0.05
./scripts/run.sh --queuePenaltyFactor 0.05
```

### 3. `--logLevel <级别>`
设置日志输出级别。

```bash
# 关闭所有日志（最高性能）
./scripts/run.sh --logLevel off

# 仅显示警告和错误
./scripts/run.sh --logLevel warn

# 显示信息、警告和错误（推荐用于生产环境）
./scripts/run.sh --logLevel info

# 显示调试信息（详细）
./scripts/run.sh --logLevel debug

# 显示所有日志（包括调试）
./scripts/run.sh --logLevel all
```

### 4. `--disableLogs`
快速禁用所有日志输出（等同于 `--logLevel off`）。

```bash
./scripts/run.sh --disableLogs
```

## 参数组合示例

### 示例1：完整动态调度测试
```bash
./scripts/run.sh \
  --enablePriceAwareScheduling 1 \
  --queuePenaltyFactor 0.1 \
  --logLevel all
```

### 示例2：高性能运行（无日志）
```bash
./scripts/run.sh \
  --enablePriceAwareScheduling 1 \
  --logLevel off
```

### 示例3：仅查看调度决策
```bash
./scripts/run.sh --logLevel info
```

### 示例4：调试模式（最详细）
```bash
./scripts/run.sh --logLevel debug
```

## 观察动态调度行为

### 查看调度决策日志
```bash
# 运行仿真并过滤调度相关日志
./scripts/run.sh 2>&1 | grep -E '(Scheduling Decision|Connection Switch)'

# 查看TTTT/RTT测量结果
./scripts/run.sh 2>&1 | grep -E '(TTTT measured|RTT measured)'

# 查看完整决策过程
./scripts/run.sh 2>&1 | grep -E '(Cost_Stay|Cost_Switch)'
```

### 保存日志到文件
```bash
# 保存所有输出到文件
./scripts/run.sh > simulation.log 2>&1

# 仅保存调度相关日志
./scripts/run.sh 2>&1 | grep -E '(Scheduling|Connection|TTTT|RTT)' > scheduler.log
```

## 输出示例

### 正常运行的日志输出
```
================================================
动态成本感知调度仿真 (Dynamic Cost-Aware Scheduling)
================================================
Price-Aware Scheduling: 1
Queue Penalty Factor: 0.1
Log Level: all

日志输出已启用！你将看到：
  - TTTT/RTT 测量结果
  - 调度决策过程（成本对比）
  - 连接切换日志
  - 网络探测结果

查看调度详细日志：
  ./ns3 run "$CMD" 2>&1 | grep -E '(Scheduling|Connection Switch|TTTT|RTT)'
```

### 调度决策日志示例
```
========== Scheduling Decision at T=1.5000s ==========
Evaluating 2 consumers:
  Consumer 6 (10.0.6.2:8080) - TotalCost: 45.2314 | ProcessingCost: 40.1250 | QueuePenalty: 0.0000 | WaitTime: 0.052s | TTTT: 0.011s
  Consumer 9 (10.0.9.2:8080) - TotalCost: 15.6789 | ProcessingCost: 10.5000 | QueuePenalty: 0.0000 | WaitTime: 0.085s | TTTT: 0.042s
========== Selected Consumer 9 with lowest cost 15.6789 ==========

========== Connection Switch Decision ==========
Current Target: Node 6 (10.0.6.2:8080)
  Cost_Stay: 45.2314 | Processing: 40.1250 | QueuePenalty: 0.0000 | WaitTime: 0.052s

Switch Target: Node 9 (10.0.9.2:8080)
  Cost_Switch: 15.6789 | Processing: 10.5000 | QueuePenalty: 0.0000 | WaitTime: 0.085s

Threshold (20% lower): 36.1851
Decision: SWITCH
========== EXECUTING SWITCH ==========
Cost reduced from 45.2314 to 15.6789 (saving 65.34%)
Closing connection to Node 6, connecting to Node 9
```

## 性能注意事项

| 日志级别 | 性能影响 | 推荐用途 |
|----------|----------|----------|
| off | 最高 | 生产运行、性能测试 |
| warn | 高 | 快速测试 |
| info | 中等 | 一般测试、观察关键信息 |
| debug | 低 | 开发调试 |
| all | 最低 | 详细调试、分析调度行为 |

## 故障排除

### 问题1：没有看到调度日志
**解决方案：**
```bash
# 确认启用了价格感知调度
./scripts/run.sh --enablePriceAwareScheduling 1

# 确认日志级别不是 off
./scripts/run.sh --logLevel all
```

### 问题2：日志太多影响查看
**解决方案：**
```bash
# 过滤关键日志
./scripts/run.sh 2>&1 | grep -E '(Scheduling|Connection Switch|TTTT|RTT)'

# 使用较低的日志级别
./scripts/run.sh --logLevel info
```

### 问题3：仿真运行缓慢
**解决方案：**
```bash
# 禁用日志提高速度
./scripts/run.sh --logLevel off
```

## 与测试脚本结合

```bash
# 运行完整测试
./scripts/run.sh --logLevel all

# 快速验证（无日志）
./scripts/run.sh --disableLogs
```

---

**提示：** 默认配置已优化为显示动态调度器的完整行为，适合学习和调试。如需性能测试，请使用 `--disableLogs` 参数。
