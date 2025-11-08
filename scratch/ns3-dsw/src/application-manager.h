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
     * @param queuePenaltyFactor 队列惩罚系数
     * @return 调度器实例
     */
    static Ptr<PriceAwareScheduler> CreatePriceAwareScheduler(
        const std::map<uint32_t, NodeSpec>& nodeSpecMap,
        const std::vector<Address>& sinkAddresses,
        const std::vector<double>& priceProfile,
        double queuePenaltyFactor = 0.1);

private:
    // 禁用拷贝构造和赋值
    ApplicationManager(const ApplicationManager&) = delete;
    ApplicationManager& operator=(const ApplicationManager&) = delete;
};

} // namespace ns3
