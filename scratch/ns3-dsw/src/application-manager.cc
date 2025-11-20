#include "application-manager.h"
#include "../../src/pro-sink-app/model/price-aware-scheduler.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/address.h"

NS_LOG_COMPONENT_DEFINE("ApplicationManager");

namespace ns3 {

ApplicationManager::ApplicationManager()
{
}

AppInstallResult
ApplicationManager::InstallProSinkApps(
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
    double warmupTime)
{
    AppInstallResult result;
    ApplicationContainer proApps;

    // 用于轮询分配消费者的索引
    uint32_t sinkRoundRobinIndex = 0;

    // 遍历所有节点，安装 Producer 或 Sink
    for (uint32_t nodeId : nodeIds)
    {
        // 检查该节点是否在 nodes.csv 中定义过
        if (nodeSpecMap.count(nodeId) == 0)
        {
            NS_LOG_DEBUG(
                "Node "
                << nodeId
                << " is router-only (in links.csv but not nodes.csv). Skipping Pro-Sink app.");
            continue;
        }

        const NodeSpec& ns = nodeSpecMap.at(nodeId);
        Ptr<Node> node = nodes.Get(nodeId);

        if (ns.type == NodeType::CONSUMER)
        {
            // 这是消费者 (Sink)
            Ptr<MySink> sinkApp = CreateObject<MySink>();

            bool canEnablePower = enablePowerCoupling && (ns.fullPower > 0.0);

            if (canEnablePower)
            {
                std::string powerCostXmlFile = powerCostXmlBase + "_node" + std::to_string(ns.id) + ".xml";
                NS_LOG_INFO("Node " << ns.id << " (Consumer) enabling power coupling. Output -> "
                                   << powerCostXmlFile);

                sinkApp->Setup(ns.id,
                               ns.appRate,
                               simulationStep,
                               proAppUpdateInterval,
                               warmupTime,
                               ns.basePower,
                               ns.fullPower,
                               ns.phaseOffset,
                               priceProfile,
                               powerCostXmlFile);
            }
            else
            {
                if (enablePowerCoupling)
                {
                    NS_LOG_WARN("Node " << ns.id << " (Consumer): Power coupling SKIPPED. "
                                       << "fullPower=" << ns.fullPower
                                       << " (must be > 0 in nodes.csv).");
                }
                sinkApp->Setup(ns.id,
                               ns.appRate,
                               simulationStep,
                               proAppUpdateInterval);
            }

            sinkApp->SetAttribute("TaskSize", UintegerValue(proTaskSize));
            sinkApp->SetAttribute("PacketSize", UintegerValue(proPacketSize));

            node->AddApplication(sinkApp);
            sinkApp->SetStartTime(Seconds(proAppStartTime));
            sinkApp->SetStopTime(Seconds(proAppStopTime));
            proApps.Add(sinkApp);
            result.sinks.push_back(sinkApp);
        }
        else if (ns.type == NodeType::PRODUCER)
        {
            // 这是生产者 (Producer)
            if (sinkAddresses.empty())
            {
                NS_LOG_WARN("Node " << nodeId
                                    << " (edge) is a producer, but no sinks are available. "
                                       "Skipping app installation.");
                continue;
            }
            Ptr<MyProducer> producerApp = CreateObject<MyProducer>();

            // 使用轮询 (Round-Robin) 方式将生产者分配给消费者
            Address targetSink = sinkAddresses[sinkRoundRobinIndex];

            // 更新索引，使其在 sinkAddresses 列表的大小上循环
            sinkRoundRobinIndex = (sinkRoundRobinIndex + 1) % sinkAddresses.size();

            producerApp->Setup(targetSink, // 传递单个 Address
                               ns.appRate,
                               proTaskSize,
                               proPacketSize,
                               simulationStep);

            // 同样设置 Attribute (与 example 保持一致)
            producerApp->SetAttribute("TaskSize", UintegerValue(proTaskSize));
            producerApp->SetAttribute("PacketSize", UintegerValue(proPacketSize));

            node->AddApplication(producerApp);
            producerApp->SetStartTime(Seconds(proAppStartTime));
            producerApp->SetStopTime(Seconds(proAppStopTime));
            proApps.Add(producerApp);
            result.producers.push_back(producerApp);
        }
        // (ns.type == UNKNOWN 的节点会被自动跳过)
    }

    result.apps = proApps;

    if (!sinkAddresses.empty())
    {
        NS_LOG_INFO("Installed " << result.sinks.size() << " consumers and "
                                 << result.producers.size() << " producers.");
    }
    else
    {
        NS_LOG_INFO("Installed " << result.sinks.size() << " consumers and "
                                 << result.producers.size() << " producers.");
    }

    return result;
}

Ptr<PriceAwareScheduler>
ApplicationManager::CreatePriceAwareScheduler(
    const std::map<uint32_t, NodeSpec>& nodeSpecMap,
    const std::vector<Address>& sinkAddresses,
    const std::vector<double>& priceProfile,
    double loadDecayFactor,
    double maxCongestionPenalty,
    double congestionSensitivity,
    double maxProducerPenalty,
    double producerSensitivity,
    std::string logFilePath)
{
    NS_LOG_FUNCTION(&nodeSpecMap << sinkAddresses.size() << priceProfile.size()
                    << loadDecayFactor << maxCongestionPenalty << congestionSensitivity
                    << maxProducerPenalty << producerSensitivity);

    std::vector<ConsumerState> consumers;

    // 遍历所有节点，创建消费者状态
    uint32_t consumerIndex = 0;
    for (const auto& kv : nodeSpecMap)
    {
        const NodeSpec& ns = kv.second;

        if (ns.type == NodeType::CONSUMER)
        {
            ConsumerState consumer;
            consumer.nodeId = ns.id;
            consumer.tasksPerSecond = ns.appRate;
            consumer.phaseOffsetHours = ns.phaseOffset;
            consumer.basePower = ns.basePower;
            consumer.fullPower = ns.fullPower;
            consumer.currentUtilization = 0.0;
            consumer.currentQueueLength = 0;

            // 从sinkAddresses中获取对应的地址
            if (consumerIndex < sinkAddresses.size())
            {
                consumer.sinkAddress = sinkAddresses[consumerIndex];
                consumerIndex++;
            }
            else
            {
                NS_LOG_WARN("Not enough sink addresses for consumer " << ns.id);
            }

            NS_LOG_INFO("Added consumer " << ns.id
                       << " with rate " << ns.appRate
                       << " tasks/s and phase offset " << ns.phaseOffset << "h");

            consumers.push_back(consumer);
        }
    }

    // 创建调度器
    Ptr<PriceAwareScheduler> scheduler = CreateObject<PriceAwareScheduler>();
    scheduler->Initialize(consumers, priceProfile, loadDecayFactor,
                          maxCongestionPenalty, congestionSensitivity,
                          maxProducerPenalty, producerSensitivity,logFilePath);

    NS_LOG_INFO("Created PriceAwareScheduler with " << consumers.size()
                << " consumers and load decay factor " << loadDecayFactor);

    return scheduler;
}

} // namespace ns3
