#pragma once

#include "../../scratch/ns3-dsw/src/dsw-structures.h"
#include "ns3/object.h"
#include "ns3/address.h"
#include "ns3/callback.h"
#include <vector>
#include <map>
#include <fstream>

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
     * @param loadDecayFactor 负载衰减因子 (0.0-1.0，影响高负载下的处理速度)
     */
    void Initialize(const std::vector<ConsumerState>& consumers,
                    const std::vector<double>& priceProfile,
                    double queuePenaltyFactor = 0.1,
                    double loadDecayFactor = 0.5);

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
     * @param utilization 当前利用率（指针，nullptr表示不更新）
     * @param queueLength 当前队列长度（指针，nullptr表示不更新）
     *
     * @details 使用指针参数可以精确控制需要更新的字段，避免覆盖
     */
    void UpdateConsumerState(uint32_t nodeId, const double* utilization, const uint32_t* queueLength);

    /**
     * @brief 计算指定消费者的调度成本
     * @param consumer 消费者状态
     * @param taskArrivalTime 任务到达时间
     * @return 成本指标
     */
    CostMetrics CalculateCost(const ConsumerState& consumer, double taskArrivalTime);

    /**
     * @brief 计算指定消费者的调度成本（使用EWMA平滑队列长度）
     * @param consumer 消费者状态
     * @param taskArrivalTime 任务到达时间
     * @param smoothedQueueLength 平滑后的队列长度
     * @return 成本指标
     */
    CostMetrics CalculateCost(const ConsumerState& consumer, double taskArrivalTime, double smoothedQueueLength);

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

    /**
     * @brief 设置负载衰减因子
     * @param factor 衰减因子 (0.0-1.0)
     * @details
     * 0.0: 固定处理速度（无衰减，方案1）
     * 0.5: 50%衰减（中等衰减）
     * 1.0: 100%衰减（高负载时处理速度大幅下降）
     */
    void SetLoadDecayFactor(double factor);

    /**
     * @brief 报告TTTT测量结果
     * @param consumerAddress 消费者地址
     * @param tttt TTTT测量值
     */
    void ReportTTTTMeasured(Address consumerAddress, Time tttt);

    /**
     * @brief 报告RTT测量结果
     * @param consumerAddress 消费者地址
     * @param rtt RTT测量值
     */
    void ReportRTTMeasured(Address consumerAddress, Time rtt);

    /**
     * @brief 记录调度事件到XML文件
     * @param producerNodeId 生产者节点ID
     * @param taskArrivalTime 任务到达时间
     * @param currentAddress 当前连接地址
     * @param newAddress 新目标地址
     * @param currentCost 当前成本
     * @param switchCost 切换成本
     * @param threshold 切换阈值
     * @param decision 决策结果
     */
    void LogSchedulingEvent(uint32_t producerNodeId,
                            double taskArrivalTime,
                            Address currentAddress,
                            Address newAddress,
                            CostMetrics currentCost,
                            CostMetrics switchCost,
                            double threshold,
                            const std::string& decision);

private:
    /**
     * @brief 更新EWMA队列长度
     * @param consumer 消费者状态引用
     * @param queueLength 新的队列长度
     *
     * @details
     * 使用指数加权移动平均算法更新队列长度的平滑值
     * formula: alpha = 1.0 - exp(-dt / EWMA_SPAN_SECONDS)
     * new_avg = alpha * new_value + (1 - alpha) * old_avg
     */
    void UpdateEwmaQueueLength(ConsumerState& consumer, uint32_t queueLength);

    /**
     * @brief 获取平滑后的队列长度
     * @param consumer 消费者状态引用
     * @return 平滑后的队列长度
     *
     * @details
     * 在查询前先进行"衰减"计算，确保长时间空闲后返回正确的值
     * 如果距离上次更新有很长时间，会向当前瞬时队列长度衰减
     */
    double GetSmoothedQueueLength(ConsumerState& consumer);

private:
    /**
     * @brief 计算任务开始处理的时间
     * @param consumer 消费者
     * @param currentTime 当前时间
     * @return 任务开始时间
     */
    double CalculateTaskStartTime(const ConsumerState& consumer, double currentTime) const;

    /**
     * @brief 预测TTTT（总任务传输时间）
     * @param consumer 消费者状态
     * @return 预测的TTTT值
     *
     * @details
     * 根据数据新鲜度决定使用实测值或估算值：
     * - 如果TTTT数据新鲜（< TTTT_FRESHNESS_THRESHOLD），使用实测值
     * - 否则使用 RTT * K-Factor 估算
     */
    Time PredictTTTT(const ConsumerState& consumer) const;

    static constexpr double TTTT_FRESHNESS_THRESHOLD = 0.25; // 数据新鲜度阈值（秒）

    /**
     * @brief 计算处理时间
     * @param consumer 消费者
     * @return 单任务处理时间 (秒)
     */
    double CalculateProcessingTime(const ConsumerState& consumer) const;

    std::vector<ConsumerState> m_consumers;  // 消费者状态列表
    std::vector<double> m_priceProfile;      // 电价曲线
    double m_queuePenaltyFactor;             // 队列积压惩罚系数
    double m_loadDecayFactor;                // 负载衰减因子
    bool m_initialized;                      // 初始化标志

    // XML记录相关
    std::ofstream m_xmlLogFile;              // 调度事件XML日志文件
};

} // namespace ns3
