#include "price-aware-scheduler.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"
#include <iomanip>

NS_LOG_COMPONENT_DEFINE("PriceAwareScheduler");

namespace ns3 {

TypeId PriceAwareScheduler::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::PriceAwareScheduler")
        .SetParent<Object>()
        .SetGroupName("Applications")
        .AddConstructor<PriceAwareScheduler>();
    return tid;
}

PriceAwareScheduler::PriceAwareScheduler()
    : m_loadDecayFactor(0.5),
      m_maxCongestionPenalty(0.5),
      m_congestionSensitivity(2.0),
      m_maxProducerPenalty(3.0),
      m_producerSensitivity(100.0),
      m_initialized(false),
      m_xmlLogFile()
{
    NS_LOG_FUNCTION(this);
}

PriceAwareScheduler::~PriceAwareScheduler()
{
    NS_LOG_FUNCTION(this);

    // 关闭XML日志文件
    if (m_xmlLogFile.is_open())
    {
        m_xmlLogFile << "</SchedulerEvents>" << std::endl;
        m_xmlLogFile.close();
    }
}

void PriceAwareScheduler::Initialize(const std::vector<ConsumerState>& consumers,
                                     const std::vector<double>& priceProfile,
                                     double loadDecayFactor,
                                     double maxCongestionPenalty,
                                     double congestionSensitivity,
                                     double maxProducerPenalty,
                                     double producerSensitivity,
                                     std::string logFilePath)
{
    NS_LOG_FUNCTION(this << consumers.size() << priceProfile.size() << loadDecayFactor);

    m_consumers = consumers;
    m_priceProfile = priceProfile;
    m_loadDecayFactor = loadDecayFactor;
    m_maxCongestionPenalty = maxCongestionPenalty;
    m_congestionSensitivity = congestionSensitivity;
    m_maxProducerPenalty = maxProducerPenalty;
    m_producerSensitivity = producerSensitivity;
    m_initialized = true;

    // 打开XML日志文件
    m_xmlLogFile.open(logFilePath, std::ios::out | std::ios::trunc);
    if (m_xmlLogFile.is_open())
    {
        m_xmlLogFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        m_xmlLogFile << "<SchedulerEvents>" << std::endl;
        m_xmlLogFile << std::fixed << std::setprecision(6);
        NS_LOG_INFO("Scheduler XML log opened: " << logFilePath);
    }
    else
    {
        NS_LOG_WARN("Failed to open scheduler XML log file at: " << logFilePath);
    }

    NS_LOG_INFO("PriceAwareScheduler initialized with " << m_consumers.size()
              << " consumers, " << m_priceProfile.size() << " price points, "
              << "load decay factor " << loadDecayFactor
              << ", max congestion penalty " << maxCongestionPenalty
              << ", congestion sensitivity " << congestionSensitivity << "s"
              << ", max producer penalty " << maxProducerPenalty
              << ", producer sensitivity " << producerSensitivity << " tasks.");
}

Address PriceAwareScheduler::ScheduleNextTask(double taskArrivalTime, uint32_t producerPendingTasks)
{
    NS_LOG_FUNCTION(this << taskArrivalTime << producerPendingTasks);

    if (!m_initialized)
    {
        NS_LOG_ERROR("PriceAwareScheduler not initialized!");
        NS_ASSERT_MSG(false, "PriceAwareScheduler must be initialized before use");
    }

    if (m_consumers.empty())
    {
        NS_LOG_ERROR("No consumers available for scheduling!");
        NS_ASSERT_MSG(false, "No consumers available");
    }

    // 计算所有消费者的成本，选择最优
    double bestCost = std::numeric_limits<double>::max();
    const ConsumerState* bestConsumer = nullptr;

    NS_LOG_INFO("========== Scheduling Decision at T=" << taskArrivalTime << "s ==========");
    NS_LOG_INFO("Producer Pending Tasks: " << producerPendingTasks);
    NS_LOG_INFO("Evaluating " << m_consumers.size() << " consumers:");

    for (auto& consumer : m_consumers)
    {
        // 先更新并获取平滑队列长度（进行"衰减"）
        double smoothedQueueLength = GetSmoothedQueueLength(consumer);

        // 使用EWMA平滑值计算成本，传递生产者待处理任务数
        CostMetrics metrics = CalculateCost(consumer, taskArrivalTime, smoothedQueueLength, producerPendingTasks);

        InetSocketAddress consumerAddr = InetSocketAddress::ConvertFrom(consumer.sinkAddress);
        Time predictedTttt = PredictTTTT(consumer);
        NS_LOG_INFO("  Consumer " << consumer.nodeId
                    << " (" << consumerAddr.GetIpv4() << ":8080)"
                    << " - TotalCost: " << std::fixed << std::setprecision(4) << metrics.totalCost
                    << " | ProcessingCost: " << metrics.processingCost
                    << " | TimeCost: " << metrics.timeCost
                    << " | ProducerWeight: " << metrics.producerWeight
                    << " | WaitTime: " << std::setprecision(3) << metrics.waitingTime << "s"
                    << " | TTTT: " << predictedTttt.GetSeconds() << "s"
                    << " | SmoothedQueue: " << smoothedQueueLength);

        if (metrics.totalCost < bestCost)
        {
            bestCost = metrics.totalCost;
            bestConsumer = &consumer;
        }
    }

    NS_LOG_INFO("========== Selected Consumer " << bestConsumer->nodeId
               << " with lowest cost " << bestCost << " ==========");

    return bestConsumer->sinkAddress;
}

CostMetrics PriceAwareScheduler::CalculateCost(const ConsumerState& consumer,
                                               double taskArrivalTime)
{
    NS_LOG_FUNCTION(this << consumer.nodeId << taskArrivalTime);

    CostMetrics metrics;

    // 1. 计算任务开始时间
    double startTime = CalculateTaskStartTime(consumer, taskArrivalTime);

    // 2. 计算处理时间
    double processingTime = CalculateProcessingTime(consumer);

    // 3. 计算等待时间
    metrics.waitingTime = startTime - taskArrivalTime;

    // 4. 获取该时间点的电价
    double price = GetPriceAtTime(startTime, consumer.phaseOffsetHours);

    // 5. 计算处理成本
    metrics.processingCost = price * processingTime;

    // 6. 使用简化的时间成本计算（不考虑生产者压力）
    // 这个方法主要用于向后兼容，实际调度使用带smoothedQueueLength的版本
    Time predictedTTTT = PredictTTTT(consumer);
    double queueWaitTime = consumer.currentQueueLength * processingTime;
    double T_delay = predictedTTTT.GetSeconds() + queueWaitTime;

    // 基础拥塞成本（归一化）
    double Cost_cong = m_maxCongestionPenalty * std::tanh(T_delay / m_congestionSensitivity);
    metrics.timeCost = Cost_cong;

    // 7. 总成本（不包含生产者压力）
    metrics.totalCost = metrics.processingCost + metrics.timeCost;

    // 8. 保留queuePenalty以向后兼容
    metrics.queuePenalty = metrics.timeCost;

    return metrics;
}

CostMetrics PriceAwareScheduler::CalculateCost(const ConsumerState& consumer,
                                               double taskArrivalTime,
                                               double smoothedQueueLength,
                                               uint32_t producerPendingTasks)
{
    NS_LOG_FUNCTION(this << consumer.nodeId << taskArrivalTime << smoothedQueueLength << producerPendingTasks);

    CostMetrics metrics;

    // 1. 预测TTTT（总任务传输时间）
    Time predictedTTTT = PredictTTTT(consumer);
    double arrivalTimeAtConsumer = taskArrivalTime + predictedTTTT.GetSeconds();

    // 2. 计算处理时间
    double processingTime = CalculateProcessingTime(consumer);

    // 3. 计算队列等待时间 - 使用平滑队列长度
    double queueWaitTime = smoothedQueueLength * processingTime;
    metrics.waitingTime = queueWaitTime;

    // 4. 计算任务开始时间
    double startTime = arrivalTimeAtConsumer + queueWaitTime;

    // 5. 获取该时间点的电价
    double price = GetPriceAtTime(startTime, consumer.phaseOffsetHours);

    // 6. 计算处理成本
    metrics.processingCost = price * processingTime;

    // ===== Tanh归一化成本函数 =====

    // 7. 计算总延迟 T_delay = TTTT + queueWaitTime
    double T_delay = predictedTTTT.GetSeconds() + queueWaitTime;

    // 8. 计算基础拥塞成本（归一化）
    // Cost_cong = m_maxCongestionPenalty * tanh(T_delay / m_congestionSensitivity)
    double Cost_cong = m_maxCongestionPenalty * std::tanh(T_delay / m_congestionSensitivity);

    // 9. 计算生产者紧急度乘数（归一化）
    // Multiplier = 1.0 + (m_maxProducerPenalty * tanh(ProducerPending / m_producerSensitivity))
    double Multiplier = 1.0 + (m_maxProducerPenalty * std::tanh(static_cast<double>(producerPendingTasks) / m_producerSensitivity));
    metrics.producerWeight = Multiplier;

    // 10. 计算最终时间成本
    metrics.timeCost = Cost_cong * Multiplier;

    // 11. 计算总成本 = 处理成本 + 时间成本
    metrics.totalCost = metrics.processingCost + metrics.timeCost;

    // 12. 保留queuePenalty以向后兼容（设置为时间成本）
    metrics.queuePenalty = metrics.timeCost;

    return metrics;
}

double PriceAwareScheduler::GetPriceAtTime(double timeHours, double phaseOffsetHours) const
{
    NS_LOG_FUNCTION(this << timeHours << phaseOffsetHours);

    const double priceProfileDuration = 24.0; // 小时
    const size_t numPricePoints = m_priceProfile.size();

    if (numPricePoints == 0)
    {
        NS_LOG_WARN("Price profile is empty, returning 0.0");
        return 0.0;
    }

    // 应用相位偏移并确保时间在 [0, 24) 范围内
    double effectiveTimeHours = std::fmod(timeHours + phaseOffsetHours, priceProfileDuration);

    // 确保 fmod 结果为正
    if (effectiveTimeHours < 0)
    {
        effectiveTimeHours += priceProfileDuration;
    }

    // 计算每个电价点代表的时间（小时）
    double priceStepHours = priceProfileDuration / numPricePoints;

    // 计算浮点索引
    double floatingIndex = (effectiveTimeHours + 1e-9) / priceStepHours;

    // 向下取整得到离散索引
    uint32_t index = static_cast<uint32_t>(std::floor(floatingIndex));

    // 确保索引在有效范围内
    index = std::min(index, static_cast<uint32_t>(numPricePoints - 1));

    return m_priceProfile[index];
}

double PriceAwareScheduler::CalculateTaskStartTime(const ConsumerState& consumer,
                                                   double currentTime) const
{
    // 预测TTTT（总任务传输时间）
    Time predictedTTTT = PredictTTTT(consumer);
    double arrivalTimeAtConsumer = currentTime + predictedTTTT.GetSeconds();

    // 计算处理时间和队列等待时间
    double processingTime = CalculateProcessingTime(consumer);
    double queueWaitTime = consumer.currentQueueLength * processingTime;

    double taskStartTime = arrivalTimeAtConsumer + queueWaitTime;

    NS_LOG_DEBUG("Consumer " << consumer.nodeId
                << ": TTTT=" << predictedTTTT.GetSeconds() << "s, "
                << "arrival=" << arrivalTimeAtConsumer << "s, "
                << "queueWait=" << queueWaitTime << "s, "
                << "taskStart=" << taskStartTime << "s");

    return taskStartTime;
}

double PriceAwareScheduler::CalculateProcessingTime(const ConsumerState& consumer) const
{
    // 处理时间 = 1 / 任务处理速度
    // 注意：这里假设consumer.tasksPerSecond是理论最大处理速度
    // m_loadDecayFactor控制高负载时的性能衰减程度

    if (consumer.tasksPerSecond <= 0.0)
    {
        NS_LOG_WARN("Consumer " << consumer.nodeId
                   << " has invalid tasksPerSecond: " << consumer.tasksPerSecond);
        return 1.0; // 默认1秒
    }

    // 考虑当前负载对处理速度的影响
    // 负载衰减模型：effectiveRate = tasksPerSecond * (1.0 - utilization * loadDecayFactor)
    // - loadDecayFactor = 0.0: 固定处理速度（无衰减）
    // - loadDecayFactor = 0.5: 中等衰减（50%）
    // - loadDecayFactor = 1.0: 完全衰减（高负载时速度大幅下降）
    double loadFactor = std::min(consumer.currentUtilization, 1.0);
    double effectiveRate = consumer.tasksPerSecond * (1.0 - loadFactor * m_loadDecayFactor);

    // 保证最低处理速度为理论速度的10%
    effectiveRate = std::max(effectiveRate, consumer.tasksPerSecond * 0.1);

    return 1.0 / effectiveRate;
}

void PriceAwareScheduler::UpdateConsumerState(uint32_t nodeId,
                                              const double* utilization,
                                              const uint32_t* queueLength)
{
    NS_LOG_FUNCTION(this << nodeId << utilization << queueLength);

    for (auto& consumer : m_consumers)
    {
        if (consumer.nodeId == nodeId)
        {
            // 只有在指针非空时才更新相应字段
            if (utilization != nullptr)
            {
                consumer.currentUtilization = *utilization;
            }

            if (queueLength != nullptr)
            {
                consumer.currentQueueLength = *queueLength;
                // 只有在队列长度更新时，才调用EWMA更新
                UpdateEwmaQueueLength(consumer, *queueLength);
            }

            NS_LOG_DEBUG("Updated Consumer " << nodeId
                        << " State: Util=" << consumer.currentUtilization  
                        << ", Queue=" << consumer.currentQueueLength       
                        << " (Input: Util=" << (utilization ? "Set" : "Skip") 
                        << ", Queue=" << (queueLength ? "Set" : "Skip") << ")");
            return;
        }
    }

    NS_LOG_WARN("Consumer with nodeId " << nodeId << " not found!");
}

void PriceAwareScheduler::SetLoadDecayFactor(double factor)
{
    NS_LOG_FUNCTION(this << factor);
    m_loadDecayFactor = factor;
    NS_LOG_INFO("Load decay factor set to " << m_loadDecayFactor);
}

const std::vector<ConsumerState>& PriceAwareScheduler::GetConsumers() const
{
    return m_consumers;
}

void PriceAwareScheduler::ReportTTTTMeasured(Address consumerAddress, Time tttt)
{
    for (auto& consumer : m_consumers)
    {
        if (consumer.sinkAddress == consumerAddress)
        {
            consumer.m_lastMeasuredTTTT = tttt;
            consumer.m_lastTTTTTimestamp = Simulator::Now();

            // 1. 安全检查：防止除以零
            if (consumer.m_lastMeasuredRTT.IsZero()) {
                 NS_LOG_WARN("RTT is zero, cannot update K-factor for node " << consumer.nodeId);
                 return; 
            }

            // 2. 计算瞬时 K 值
            double currentK = tttt.GetSeconds() / consumer.m_lastMeasuredRTT.GetSeconds();

            // 3. 异常值过滤 (Sanity Check)
            // K因子理论上不应小于1（任务时间不可能短于RTT），也不应过大（除非断网）
            // 这里设置一个宽松的范围，比如 [1.0, 1000.0]
            if (currentK < 1.0) currentK = 1.0; 
            
            // 4. EWMA 平滑更新 (Alpha 通常取 0.125 或 0.25)
            // 新值占 0.25，历史值占 0.75，防止 K 值剧烈抖动
            double alpha = 0.25;
            if (consumer.m_K_factor == 1.0) { 
                // 如果是第一次（假设初值为1.0），直接赋值，不要平滑
                consumer.m_K_factor = currentK;
            } else {
                consumer.m_K_factor = (1.0 - alpha) * consumer.m_K_factor + alpha * currentK;
            }

            NS_LOG_INFO("Updated K-Factor for consumer " << consumer.nodeId
                      << ": Raw=" << currentK 
                      << ", Smoothed=" << consumer.m_K_factor);

            return;
        }
    }
}

void PriceAwareScheduler::ReportRTTMeasured(Address consumerAddress, Time rtt)
{
    NS_LOG_FUNCTION(this << consumerAddress << rtt.GetSeconds());

    for (auto& consumer : m_consumers)
    {
        if (consumer.sinkAddress == consumerAddress)
        {
            consumer.m_lastMeasuredRTT = rtt;

            NS_LOG_DEBUG("RTT measured for consumer " << consumer.nodeId
                      << ": " << rtt.GetSeconds() << "s");

            return;
        }
    }

    NS_LOG_WARN("Consumer with address " << consumerAddress << " not found!");
}

void PriceAwareScheduler::ReportRTTMeasured(uint32_t nodeId, Time rtt)
{
    NS_LOG_FUNCTION(this << nodeId << rtt.GetSeconds());

    for (auto& consumer : m_consumers)
    {
        if (consumer.nodeId == nodeId)
        {
            consumer.m_lastMeasuredRTT = rtt;

            NS_LOG_DEBUG("RTT measured for consumer " << consumer.nodeId
                      << ": " << rtt.GetSeconds() << "s");

            return;
        }
    }

    NS_LOG_WARN("Consumer with Node ID " << nodeId << " not found!");
}

Time PriceAwareScheduler::PredictTTTT(const ConsumerState& consumer) const
{
    NS_LOG_FUNCTION(consumer.nodeId);

    Time now = Simulator::Now();
    Time timeSinceLastTTTT = now - consumer.m_lastTTTTTimestamp;

    // 如果TTTT数据新鲜，使用实测值
    if (timeSinceLastTTTT < Seconds(TTTT_FRESHNESS_THRESHOLD))
    {
        NS_LOG_DEBUG("Using fresh TTTT measurement: " << consumer.m_lastMeasuredTTTT.GetSeconds() << "s");
        return consumer.m_lastMeasuredTTTT;
    }
    else
    {
        // 数据已过时，使用 RTT 和 K-Factor 估算
        Time predictedTTTT = consumer.m_lastMeasuredRTT * consumer.m_K_factor;

        NS_LOG_DEBUG("TTTT data stale, using estimated TTTT = RTT * K = "
                    << consumer.m_lastMeasuredRTT.GetSeconds() << " * " << consumer.m_K_factor
                    << " = " << predictedTTTT.GetSeconds() << "s");

        return predictedTTTT;
    }
}

void PriceAwareScheduler::LogSchedulingEvent(uint32_t producerNodeId,
                                             double taskArrivalTime,
                                             Address currentAddress,
                                             Address newAddress,
                                             CostMetrics currentCost,
                                             CostMetrics switchCost,
                                             double threshold,
                                             const std::string& decision)
{
    if (!m_xmlLogFile.is_open())
    {
        return;
    }

    // 转换地址为IP字符串
    InetSocketAddress currentAddr = InetSocketAddress::ConvertFrom(currentAddress);
    InetSocketAddress newAddr = InetSocketAddress::ConvertFrom(newAddress);

    // 查找对应的消费者节点ID
    uint32_t currentConsumerId = 0;
    uint32_t newConsumerId = 0;

    for (const auto& consumer : m_consumers)
    {
        if (consumer.sinkAddress == currentAddress)
        {
            currentConsumerId = consumer.nodeId;
        }
        if (consumer.sinkAddress == newAddress)
        {
            newConsumerId = consumer.nodeId;
        }
    }

    // 写入XML事件
    m_xmlLogFile << "  <Event time=\"" << taskArrivalTime << "\""
                 << " producerNode=\"" << producerNodeId << "\""
                 << " decision=\"" << decision << "\">" << std::endl;

    m_xmlLogFile << "    <CurrentTarget nodeId=\"" << currentConsumerId
                 << "\" ip=\"" << currentAddr.GetIpv4()
                 << "\" cost=\"" << currentCost.totalCost
                 << "\" processingCost=\"" << currentCost.processingCost
                 << "\" queuePenalty=\"" << currentCost.queuePenalty
                 << "\" waitTime=\"" << currentCost.waitingTime << "\"/>" << std::endl;

    m_xmlLogFile << "    <SwitchTarget nodeId=\"" << newConsumerId
                 << "\" ip=\"" << newAddr.GetIpv4()
                 << "\" cost=\"" << switchCost.totalCost
                 << "\" processingCost=\"" << switchCost.processingCost
                 << "\" queuePenalty=\"" << switchCost.queuePenalty
                 << "\" waitTime=\"" << switchCost.waitingTime << "\"/>" << std::endl;

    m_xmlLogFile << "    <Threshold value=\"" << threshold << "\"/>" << std::endl;
    m_xmlLogFile << "  </Event>" << std::endl;
    m_xmlLogFile.flush();  // 确保写入磁盘
}

void PriceAwareScheduler::UpdateEwmaQueueLength(ConsumerState& consumer, uint32_t queueLength)
{
    // 1. 获取当前仿真时间 (秒)
    double time_now = Simulator::Now().GetSeconds();

    // 2. 获取瞬时队列长度
    int q_inst = static_cast<int>(queueLength);

    // 3. 计算时间增量 (dt)
    double dt = time_now - consumer.m_lastQueueUpdateTime;

    if (consumer.m_lastQueueUpdateTime == 0.0 || dt <= 0.0)
    {
        // 第一次更新, 或在同一仿真时刻发生多次更新
        consumer.m_ewmaQueueLength = static_cast<double>(q_inst);
    }
    else
    {
        // 4. 计算 alpha (基于时间的权重), 确保 #include <cmath>
        // alpha = 1.0 - exp(-dt / span)
        double alpha = 1.0 - std::exp(-dt / EWMA_SPAN_SECONDS);

        // 5. 应用 EWMA 公式: new_avg = alpha * new_value + (1 - alpha) * old_avg
        consumer.m_ewmaQueueLength = (alpha * static_cast<double>(q_inst)) +
                                      ((1.0 - alpha) * consumer.m_ewmaQueueLength);
    }

    // 6. 更新时间戳
    consumer.m_lastQueueUpdateTime = time_now;

    NS_LOG_DEBUG("Consumer " << consumer.nodeId
                << ": Updated EWMA queue length to " << consumer.m_ewmaQueueLength
                << " (instantaneous: " << q_inst << ")");
}

double PriceAwareScheduler::GetSmoothedQueueLength(ConsumerState& consumer)
{
    // 【关键步骤】: "衰减"到当前时间
    // 在两次查询之间, 队列可能长时间未变 (空闲)。
    // 我们必须计算这段空闲时间的衰减, 否则将返回一个过时的"高"值。

    double time_now = Simulator::Now().GetSeconds();
    double dt = time_now - consumer.m_lastQueueUpdateTime;

    if (dt > 0.0)
    {
        // 队列在这段时间 (dt) 内未变化, 其"瞬时值"被视为当前队列大小
        int q_inst = static_cast<int>(consumer.currentQueueLength);

        double alpha = 1.0 - std::exp(-dt / EWMA_SPAN_SECONDS);

        // 将 EWMA 值向当前的瞬时值 (q_inst) "衰减"
        consumer.m_ewmaQueueLength = (alpha * static_cast<double>(q_inst)) +
                                      ((1.0 - alpha) * consumer.m_ewmaQueueLength);

        consumer.m_lastQueueUpdateTime = time_now;
    }

    NS_LOG_DEBUG("Consumer " << consumer.nodeId
                << ": Get smoothed queue length = " << consumer.m_ewmaQueueLength);
    return consumer.m_ewmaQueueLength;
}

} // namespace ns3
