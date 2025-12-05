## 第一部分：成本指标 (<Cost> 标签)

这些是高层次的决策指标，直接影响调度器的选择：

| 参数                 | 含义              | 计算公式                                      |
|--------------------|-----------------|-------------------------------------------|
| totalCost          | 总成本（调度器优化的目标函数） | processingCost + congestionCost           |
| processingCost     | 处理成本（电价成本）      | priceAtStart × processingTime             |
| congestionCost     | 拥塞成本（队列积压惩罚）    | baseCongestionCost × producerMultiplier   |
| producerMultiplier | 生产者紧急度乘数        | 1.0 + (W_prod × tanh(PendingTasks / K_p)) |

---
  
## 第二部分：中间计算参数 (<Intermediate> 标签)

这些是中层级的计算中间值，帮助理解成本是如何计算的：
| 参数                 | 含义         | 计算公式                         |
|--------------------|------------|------------------------------|
| timeDelay          | 总延迟时间      | TTTT + queueWaitTime         |
| baseCongestionCost | 基础拥塞成本     | W_base × tanh(T_delay / K_c) |
| TTTT               | 预测的总任务传输时间 | 基于历史测量或 RTT ×K-Factor       |
| taskStartTime      | 任务开始处理的时间  | 当前时间 + TTTT + queueWaitTime  |
| priceAtStart       | 任务开始时的电价   | 根据 taskStartTime 和相位偏移询     |
---

## 第三部分：底层参数 (<Parameters> 标签)

这些是最底层的输入参数和即时状态：
| 参数                  | 含义        | 描述                                   |
|---------------------|-----------|--------------------------------------|
| processingTime      | 单任务处理时间   | 1.0 /effectiveTasksPerSecond        |
| queueWaitTime       | 队列等待时间    | smoothedQueueLength × processingTime |
| smoothedQueueLength | 平滑队列长度    | EWMA算法平滑后的队列度                       |
| pendingTasks        | 生产者待处理任务数 | 当前生产者累积的未发送任数                       |
---
🎯 参数关联关系
成本计算流程
1. 预测 TTTT (总任务传输时间)
2. 计算队列等待时间 = 平滑队列长度 × 处理时间
3. 任务开始时间 = 当前时间 + TTTT + 队列等待时间
4. 查询电价 = priceAtTime(任务开始时间, 相位偏移)
5. 处理成本 = 电价 × 处理时间
6. 总延迟 = TTTT + 队列等待时间
7. 基础拥塞成本 = W_base × tanh(总延迟 / K_c)
8. 生产者紧急度乘数 = 1.0 + (W_prod × tanh(待处理任务数 / K_p))
9. 最终拥塞成本 = 基础拥塞成本 × 紧急度乘数
10. 总成本 = 处理成本 + 拥塞成本
决策逻辑
- 调度器计算所有消费者的 totalCost
- 选择 totalCost 最低的消费者
- 如果新目标的成本比当前目标低很多（超过阈值），则切换；否则保持
---
🔍 实际案例分析

``` xml
  <Event time="0.221259" producerNode="14" decision="STAY">
    <CurrentTarget nodeId="6" ip="10.0.2.2">
      <Cost total="4.870487" processingCost="4.386182" 
            congestionCost="0.484305" producerMultiplier="1.026665"/>
      <Intermediate timeDelay="0.078662" baseCongestionCost="0.471727" 
            TTTT="0.041045" taskStartTime="0.299920" priceAtStart="120.620000"/>
      <Parameters processingTime="0.036364" queueWaitTime="0.037617" 
            smoothedQueueLength="1.034459" pendingTasks="1"/>
    </CurrentTarget>
    ...
  </Event>
```

解读：
- 时间：0.22秒时，生产者节点14做出调度决策
- 决策：保持当前目标（节点6）
- 原因：当前目标总成本4.87 < 切换阈值3.78
- 状态：队列长度约1个任务，电价较高（120.62）
