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

namespace ns3 {

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
};

/**
 * @brief 成本指标
 * @details 用于评估调度决策的成本
 */
struct CostMetrics
{
    double totalCost = 0.0;        // 总成本
    double processingCost = 0.0;   // 处理成本
    double queuePenalty = 0.0;     // 队列积压惩罚
    double waitingTime = 0.0;      // 预计等待时间 (秒)
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
