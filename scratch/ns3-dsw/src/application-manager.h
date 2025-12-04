#pragma once

#include "dsw-structures.h"
#include "ns3/node-container.h"
#include "ns3/ipv4.h"
#include "ns3/applications-module.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/pro-sink-app.h" // MyProducer 和 MySink 类定义
#include "../../src/pro-sink-app/model/price-aware-scheduler.h"

#include <map>
#include <vector>
#include <string>

namespace ns3 {

// 应用安装结果
struct AppInstallResult
{
    ApplicationContainer apps;
    std::vector<Ptr<MyProducer>> producers;
    std::vector<Ptr<MySink>> sinks;
};

class ApplicationManager
{
public:
    ApplicationManager();

    // 安装所有 Pro-Sink 应用
    static AppInstallResult InstallProSinkApps(
        NodeContainer& nodes,
        const std::map<uint32_t, NodeSpec>& nodeSpecMap,
        const std::set<uint32_t>& nodeIds,
        const std::vector<Address>& sinkAddresses,
        bool enablePowerCoupling,
        const std::vector<double>& priceProfile,
        const std::string& powerCostXmlBase,
        const std::string& burstEventsXmlBase,
        double proAppStartTime,
        double proAppStopTime,
        Time simulationStep,
        Time proAppUpdateInterval,
        uint32_t proTaskSize,
        uint32_t proPacketSize,
        double warmupTime);

    /**
     * @brief 创建价格感知调度器
     * @param nodeSpecMap 节点规格映射
     * @param sinkAddresses 消费者地址列表
     * @param priceProfile 电价曲线
     * @param loadDecayFactor 负载衰减因子 (0.0-1.0，默认0.5)
     * @param maxCongestionPenalty 最大拥塞惩罚 (W_base，默认0.5)
     * @param congestionSensitivity 拥塞敏感度 (K_c，秒为单位，默认2.0)
     * @param maxProducerPenalty 最大生产者紧急度乘数 (W_prod，默认3.0)
     * @param producerSensitivity 生产者敏感度 (K_p，任务数为单位，默认100.0)
     * @return 调度器实例
     */
    static Ptr<PriceAwareScheduler> CreatePriceAwareScheduler(
        const std::map<uint32_t, NodeSpec>& nodeSpecMap,
        const std::vector<Address>& sinkAddresses,
        const std::vector<double>& priceProfile,
        double loadDecayFactor = 0.5,
        double maxCongestionPenalty = 0.5,
        double congestionSensitivity = 2.0,
        double maxProducerPenalty = 3.0,
        double producerSensitivity = 100.0,
        std::string logFilePath = "scratch/ns3-dsw/out/scheduler_events.xml");

private:
    // 禁用拷贝构造和赋值
    ApplicationManager(const ApplicationManager&) = delete;
    ApplicationManager& operator=(const ApplicationManager&) = delete;
};

} // namespace ns3
