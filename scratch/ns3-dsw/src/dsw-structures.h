#pragma once

#include <string>
#include <vector>
#include <set>
#include <cmath>
#include <map>

#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/vector.h"
#include "ns3/data-rate.h"
#include "ns3/net-device.h"
#include "ns3/address.h"
#include "ns3/nstime.h"

namespace ns3 {

// ----------------------------- 常量定义 -----------------------------

/**
 * @brief EWMA平滑窗口 (Span)
 * @details 定义EWMA算法的平滑窗口为1.0秒，用于减少调度器抖动
 */
static constexpr double EWMA_SPAN_SECONDS = 1.0;

// ----------------------------- 配置结构 -----------------------------
enum class NodeType
{
    UNKNOWN,
    PRODUCER, // 生产者 (edge)
    CONSUMER  // 消费者 (core)
};

// ----------------------------- 调度相关结构 -----------------------------

/**
 * @brief 消费者状态信息
 * @details 用于价格感知调度器跟踪每个消费者的实时状态
 */
struct ConsumerState
{
    uint32_t nodeId = 0;              // 节点ID
    Ipv4Address ip;                   // IP地址
    Address sinkAddress;              // Socket地址 (IP + Port)
    double tasksPerSecond = 0.0;      // 任务处理速度 (任务/秒)
    double phaseOffsetHours = 0.0;    // 电价相位偏移 (小时)
    double currentUtilization = 0.0;  // 当前利用率 (0.0-1.0)
    uint32_t currentQueueLength = 0;  // 当前队列长度 (待处理任务数)
    double basePower = 0.0;           // 基础功率 (MW)
    double fullPower = 0.0;           // 满载功率 (MW)

    // --- TTTT/RTT/K-Factor 动态调度相关 ---
    Time m_lastMeasuredTTTT;          // 上次测量的TTTT
    Time m_lastMeasuredRTT;           // 上次测量的RTT
    Time m_lastTTTTTimestamp;         // 上次TTTT测量的时间戳
    double m_K_factor = 1.0;          // K-Factor = TTTT / RTT

    // --- EWMA 队列长度平滑相关 ---
    double m_ewmaQueueLength = 0.0;      // 队列长度的EWMA平滑值
    double m_lastQueueUpdateTime = 0.0;  // 上次EWMA更新的仿真时间 (秒)
};

/**
 * @brief 成本指标
 * @details 用于评估调度决策的成本
 */
struct CostMetrics
{
    // ==== 主要成本项（高层级） ====
    double totalCost = 0.0;        // 总成本
    double processingCost = 0.0;   // 处理成本 = 电价 × 处理时间
    double congestionCost = 0.0;   // 拥塞成本 = 基础拥塞成本 × 紧急度乘数
    double producerMultiplier = 1.0; // 紧急度乘数 = 1.0 + (W_prod × tanh(PendingTasks / K_p))

    // ==== 中间计算参数（中层级） ====
    double baseCongestionCost = 0.0; // 基础拥塞成本 = W_base × tanh(T_delay / K_c)
    double timeDelay = 0.0;         // T_delay = TTTT + waitTime
    double processingTime = 0.0;    // 处理时间
    double priceAtStart = 0.0;      // 任务开始时的电价
    double taskStartTime = 0.0;     // 任务开始时间
    double queueWaitTime = 0.0;     // 队列等待时间
    double TTTT = 0.0;              // 预测的TTTT值

    // ==== 底层参数（低层级） ====
    double waitingTime = 0.0;       // 预计等待时间 (秒)
    double queuePenalty = 0.0;      // 队列积压惩罚（向后兼容）
    uint32_t pendingTasks = 0;      // 生产者待处理任务数
    double smoothedQueueLength = 0.0; // 平滑队列长度
    double producerWeight = 1.0;    // 生产者压力权重（已废弃，使用producerMultiplier）
};

struct NodeSpec
{
    uint32_t id = 0;
    bool hasPos = false;
    double x = 0.0, y = 0.0;
    std::string name;
    NodeType type = NodeType::UNKNOWN; // Producer or Consumer
    double appRate = 0.0;              // lambda or consumerRate

    double basePower = 0.0;     // MW
    double fullPower = 0.0;     // MW
    double phaseOffset = 0.0;   // hours

    // --- Mixed-Payload: 泊松簇过程参数 ---
    double burstMean = 1.0;     // 平均突发大小 (泊松簇过程的簇大小均值)
};

struct LinkSpec
{
    uint32_t a = 0, b = 0; // 原始方向（用于确定 IP 顺序）
    std::string rate;      // e.g., "100Mbps"
    uint32_t id = 0;       // 标识符（可从 CSV 读取，若缺省则自动分配）
};

struct IfRecord
{
    uint32_t a = 0;
    uint32_t b = 0; // 与 CSV 中一致的原始次序
    std::string rate;
    std::string delay; // 标签展示（可能是计算值）
    double distanceUnits = 0.0;
    double distanceMeters = 0.0;
    uint32_t id = 0; // link id
    Ipv4InterfaceContainer ifc;
};

} // namespace ns3
