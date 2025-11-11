#include "price-aware-scheduler.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"

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
    : m_queuePenaltyFactor(0.1),
      m_loadDecayFactor(0.5),
      m_initialized(false)
{
    NS_LOG_FUNCTION(this);
}

PriceAwareScheduler::~PriceAwareScheduler()
{
    NS_LOG_FUNCTION(this);
}

void PriceAwareScheduler::Initialize(const std::vector<ConsumerState>& consumers,
                                     const std::vector<double>& priceProfile,
                                     double queuePenaltyFactor,
                                     double loadDecayFactor)
{
    NS_LOG_FUNCTION(this << consumers.size() << priceProfile.size() << queuePenaltyFactor << loadDecayFactor);

    m_consumers = consumers;
    m_priceProfile = priceProfile;
    m_queuePenaltyFactor = queuePenaltyFactor;
    m_loadDecayFactor = loadDecayFactor;
    m_initialized = true;

    NS_LOG_INFO("PriceAwareScheduler initialized with " << m_consumers.size()
              << " consumers, " << m_priceProfile.size() << " price points, "
              << "load decay factor " << loadDecayFactor << ".");
}

Address PriceAwareScheduler::ScheduleNextTask(double taskArrivalTime)
{
    NS_LOG_FUNCTION(this << taskArrivalTime);

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

    for (const auto& consumer : m_consumers)
    {
        CostMetrics metrics = CalculateCost(consumer, taskArrivalTime);

        NS_LOG_DEBUG("Consumer " << consumer.nodeId
                    << ": Cost=" << metrics.totalCost
                    << " (Processing=" << metrics.processingCost
                    << ", QueuePenalty=" << metrics.queuePenalty
                    << ", WaitTime=" << metrics.waitingTime << "s)");

        if (metrics.totalCost < bestCost)
        {
            bestCost = metrics.totalCost;
            bestConsumer = &consumer;
        }
    }

    NS_LOG_INFO("Selected Consumer " << bestConsumer->nodeId
               << " with cost " << bestCost << " at time " << taskArrivalTime << "s");

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

    // 6. 计算队列积压惩罚
    metrics.queuePenalty = consumer.currentQueueLength * m_queuePenaltyFactor;

    // 7. 总成本
    metrics.totalCost = metrics.processingCost + metrics.queuePenalty;

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
    // 任务开始时间 = 当前时间 + 队列中任务预计处理时间
    double processingTime = CalculateProcessingTime(consumer);
    double queueWaitTime = consumer.currentQueueLength * processingTime;

    return currentTime + queueWaitTime;
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
                                              double utilization,
                                              uint32_t queueLength)
{
    NS_LOG_FUNCTION(this << nodeId << utilization << queueLength);

    for (auto& consumer : m_consumers)
    {
        if (consumer.nodeId == nodeId)
        {
            consumer.currentUtilization = utilization;
            consumer.currentQueueLength = queueLength;

            NS_LOG_DEBUG("Updated Consumer " << nodeId
                        << ": Util=" << utilization
                        << ", Queue=" << queueLength);
            return;
        }
    }

    NS_LOG_WARN("Consumer with nodeId " << nodeId << " not found!");
}

void PriceAwareScheduler::SetQueuePenaltyFactor(double factor)
{
    NS_LOG_FUNCTION(this << factor);
    m_queuePenaltyFactor = factor;
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

} // namespace ns3
