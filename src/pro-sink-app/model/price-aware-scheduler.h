#pragma once

#include "../../scratch/ns3-dsw/src/dsw-structures.h"
#include "ns3/object.h"
#include "ns3/address.h"
#include "ns3/callback.h"
#include <vector>
#include <map>

namespace ns3 {

/**
 * @class PriceAwareScheduler
 * @brief 价格感知启发式调度器
 *
 * @details
 * 该调度器利用已知的电价曲线和消费者实时状态，在任务发送前计算
 * 最优目标消费者，实现：
 * - 最小化总计算成本 (电价 × 处理时间)
 * - 最小化任务积压 (队列长度惩罚)
 *
 * 算法核心：
 * Cost = Price(T_start) * ProcessingTime + QueuePenalty
 * 其中 T_start = Now + QueueLength / TasksPerSecond
 */
class PriceAwareScheduler : public Object
{
public:
    static TypeId GetTypeId(void);

    PriceAwareScheduler();
    virtual ~PriceAwareScheduler();

    /**
     * @brief 初始化调度器
     * @param consumers 消费者状态列表
     * @param priceProfile 电价曲线 (288个点，5分钟间隔)
     * @param queuePenaltyFactor 队列积压惩罚系数
     */
    void Initialize(const std::vector<ConsumerState>& consumers,
                    const std::vector<double>& priceProfile,
                    double queuePenaltyFactor = 0.1);

    /**
     * @brief 调度下一个任务到最优消费者
     * @param taskArrivalTime 任务到达时间 (仿真时间，秒)
     * @return 最优消费者的Socket地址
     *
     * @details
     * 对所有消费者计算成本，选择成本最低的：
     * 1. 计算该任务的预计开始时间
     * 2. 查询该时间点的电价
     * 3. 计算总成本
     * 4. 返回成本最低的消费者地址
     */
    Address ScheduleNextTask(double taskArrivalTime);

    /**
     * @brief 更新消费者状态
     * @param nodeId 节点ID
     * @param utilization 当前利用率
     * @param queueLength 当前队列长度
     */
    void UpdateConsumerState(uint32_t nodeId, double utilization, uint32_t queueLength);

    /**
     * @brief 计算指定消费者的调度成本
     * @param consumer 消费者状态
     * @param taskArrivalTime 任务到达时间
     * @return 成本指标
     */
    CostMetrics CalculateCost(const ConsumerState& consumer, double taskArrivalTime);

    /**
     * @brief 获取价格预测
     * @param timeHours 预测时间点 (小时)
     * @param phaseOffsetHours 相位偏移 (小时)
     * @return 该时间点的电价
     */
    double GetPriceAtTime(double timeHours, double phaseOffsetHours) const;

    /**
     * @brief 设置队列惩罚系数
     * @param factor 惩罚系数 (默认0.1)
     */
    void SetQueuePenaltyFactor(double factor);

    /**
     * @brief 获取消费者列表
     * @return 消费者状态列表的常量引用
     */
    const std::vector<ConsumerState>& GetConsumers() const;

private:
    /**
     * @brief 计算任务开始处理的时间
     * @param consumer 消费者
     * @param currentTime 当前时间
     * @return 任务开始时间
     */
    double CalculateTaskStartTime(const ConsumerState& consumer, double currentTime) const;

    /**
     * @brief 计算处理时间
     * @param consumer 消费者
     * @return 单任务处理时间 (秒)
     */
    double CalculateProcessingTime(const ConsumerState& consumer) const;

    std::vector<ConsumerState> m_consumers;  // 消费者状态列表
    std::vector<double> m_priceProfile;      // 电价曲线
    double m_queuePenaltyFactor;             // 队列积压惩罚系数
    bool m_initialized;                      // 初始化标志
};

} // namespace ns3
