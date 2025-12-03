#include "dswutils.h"
#include "link-utilization-monitor.h" // 链路占用率监控
#include "dsw-structures.h"           // 数据结构定义
#include "data-parser.h"              // CSV 解析器
#include "visualization-config.h"     // 可视化配置器
#include "monitor-config.h"           // 监控配置器
#include "topology-builder.h"         // 拓扑构建器
#include "application-manager.h"      // 应用管理器
#include "../../src/pro-sink-app/model/price-aware-scheduler.h"    // 价格感知调度器

#include "ns3/applications-module.h"
#include "ns3/config.h" // 用于 Config::SetDefault
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/names.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/pro-sink-app.h"
#include "ns3/string.h" // 用于 StringValue
#include "ns3/traffic-control-module.h" //Pacing

#include <algorithm>
#include <cctype>
#include <cmath> // hypot
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TopoFigureFlowmonCfg");

static std::ofstream g_xmlFile;
static std::ofstream g_utilXmlFile;

// 全局生产者映射，用于获取统计信息
static std::map<uint32_t, Ptr<MyProducer>> g_producers;

// 前向声明
void OnProducerTaskSent(uint32_t nodeId, uint32_t taskId, Address target,
                        uint32_t pendingTasks, uint32_t totalTasksSent, uint32_t totalTasksGenerated);

// 注释：数据结构定义已移到 dsw-structures.h

static std::vector<double>
LoadCsvPrices(const std::string& path)
{
    std::vector<double> prices;
    std::ifstream fin(path.c_str());
    if (!fin.is_open())
    {
        NS_FATAL_ERROR("Cannot open price file: " << path);
    }
    std::string line;
    uint32_t ln = 0;
    while (std::getline(fin, line))
    {
        ++ln;
        std::string s = DswUtils::Trim(line);
        if (s.empty() || s[0] == '#')
            continue;
        
        std::stringstream ss(s);
        std::string hour, minute, priceStr;

        // 1. 解析列
        std::getline(ss, hour, ',');
        std::getline(ss, minute, ',');
        std::getline(ss, priceStr);

        hour = DswUtils::Trim(hour);
        priceStr = DswUtils::Trim(priceStr);

        if (priceStr.empty())
        {
             NS_LOG_WARN("Skip price line " << ln << ": Price column is empty. Line: " << s);
             continue;
        }

        try
        {
            // 2. 转换第三列
            prices.push_back(std::stod(priceStr));
        }
        catch (const std::exception& e)
        {
            // 3. 检查是否为表头
            if (ln == 1 && (hour == "hour" || priceStr == "price"))
            {
                NS_LOG_WARN("Skip header in price.csv: " << s);
                continue;
            }
            NS_LOG_WARN("Skip invalid price line " << ln << ": " << s << " (" << e.what() << ")");
        }
    }
    NS_LOG_INFO("Loaded " << prices.size() << " price points from " << path);
    return prices;
}

// 注释：LoadCsvNodes 和 LoadCsvLinks 函数已移到 DataParser 模块
// 注释：IfRecord 结构已移到 dsw-structures.h

// ----------------------------- XML Trace 回调 --------------------------------

/**
 * @brief 当 Sink 完成一个任务时（Trace 回调）
 * @param nodeId 消费者的节点 ID (用于 "Core-Id")
 * @param producerId 任务来源的生产者 ID (用于 "Edge-Id")
 * @param taskId 任务的 ID (由生产者分配，用于 "Task-Id")
 * @param totalCompleted 该消费者节点累计完成的总任务数
 */
void
OnSinkTaskCompleted(uint32_t nodeId, uint32_t producerId, uint32_t taskId, uint32_t totalCompleted)
{
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "  <Event type=\"CoreComp\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Core-Id=\"Core-" << nodeId << "\""
                  << " Edge-Id=\"Edge-" << producerId << "\""
                  << " Task-Id=\"" << producerId << "-" << taskId << "\""
                  << " TotalCompleted=\"" << totalCompleted << "\"/>"
                  << std::endl;
    }
}

/**
 * @brief 包装回调：从 Producer 获取统计信息并调用 OnProducerTaskSent
 */
void
OnProducerTaskSentWrapper(uint32_t nodeId, uint32_t taskId, Address target)
{
    // 获取对应的 MyProducer 实例
    auto it = g_producers.find(nodeId);
    if (it != g_producers.end())
    {
        Ptr<MyProducer> producer = it->second;
        uint32_t pendingTasks = producer->GetPendingTasks();
        uint32_t totalTasksSent = producer->GetTotalTasksSent();
        uint32_t totalTasksGenerated = producer->GetTotalTasksGenerated();
        OnProducerTaskSent(nodeId, taskId, target, pendingTasks, totalTasksSent, totalTasksGenerated);
    }
    else
    {
        // 如果找不到producer，仍然调用但统计为0
        OnProducerTaskSent(nodeId, taskId, target, 0, 0, 0);
    }
}

/**
 * @brief 当 Producer 发送一个新任务时（Trace 回调）
 * @param nodeId 生产者的节点 ID (用于 "Edge-Id")
 * @param taskId 任务的 ID (在该生产者上是唯一的, 用于 "Task-Id")
 * @param target 任务发送的目标地址 (用于 "TargetIp")
 * @param pendingTasks 当前pending的任务数
 * @param totalTasksSent 已发送的任务总数
 * @param totalTasksGenerated 已生成的任务总数
 */
void
OnProducerTaskSent(uint32_t nodeId, uint32_t taskId, Address target,
                    uint32_t pendingTasks, uint32_t totalTasksSent, uint32_t totalTasksGenerated)
{
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "  <Event type=\"EdgeSend\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Edge-Id=\"Edge-" << nodeId << "\""
                  << " Task-Id=\"" << nodeId << "-" << taskId << "\""
                  << " TargetIp=\"" << InetSocketAddress::ConvertFrom(target).GetIpv4() << "\""
                  << " PendingTasks=\"" << pendingTasks << "\""
                  << " TotalSent=\"" << totalTasksSent << "\""
                  << " TotalGenerated=\"" << totalTasksGenerated << "\"/>"
                  << std::endl;
    }
}
/**
 * @brief 当 Sink 报告算力利用率时 (Trace 回调)
 * @param nodeId 消费者的节点 ID (用于 "Core-Id")
 * @param utilization 利用率 (0.0 到 1.0)
 */
void
OnSinkUtilization(uint32_t nodeId, double utilization)
{
    if (g_utilXmlFile.is_open())
    {
        g_utilXmlFile << "  <Event type=\"CoreUtil\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Core-Id=\"Core-" << nodeId << "\""
                  << " Utilization=\"" << utilization << "\"/>"
                  << std::endl;
    }
}

/**
 * @brief 当 Sink 报告队列长度时 (Trace 回调)
 * @param nodeId 消费者的节点 ID (用于 "Core-Id")
 * @param queueLength 队列中待处理的任务数
 */
void
OnSinkQueueLength(uint32_t nodeId, uint32_t queueLength)
{
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "  <Event type=\"CoreQueue\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Core-Id=\"Core-" << nodeId << "\""
                  << " QueueLength=\"" << queueLength << "\"/>"
                  << std::endl;
    }
}

// 全局调度器指针（用于静态回调）
static Ptr<PriceAwareScheduler> g_scheduler = nullptr;

/**
 * @brief 静态回调：更新消费者利用率
 */
void
OnSinkUtilizationForScheduler(uint32_t nodeId, double utilization)
{
    if (g_scheduler)
    {
        g_scheduler->UpdateConsumerState(nodeId, &utilization, nullptr);
    }
}

/**
 * @brief 静态回调：更新消费者队列长度
 */
void
OnSinkQueueLengthForScheduler(uint32_t nodeId, uint32_t queueLength)
{
    if (g_scheduler)
    {
        g_scheduler->UpdateConsumerState(nodeId, nullptr, &queueLength);
    }
}

// ----------------------------- 主程序 -----------------------------
int
main(int argc, char* argv[])
{
    std::string nodesCsv = "scratch/nodes.csv";
    std::string linksCsv = "scratch/links.csv";
    std::string logLevel = "info"; // off|warn|info|debug|all
    double warmupTime = 6.0;     // 仿真预热时间 (s) - [MODIFIED]
    double simDuration = 24.0;   // 实际仿真时间 (s, 1s=1h) - [MODIFIED]
    
    std::string flowmonXml = "scratch/ns3-dsw/out/flowmon.xml";
    std::string statsCsv = "scratch/ns3-dsw/out/flowstats.csv"; // 若非空则导出 CSV 指标
    std::string animXml = "scratch/ns3-dsw/out/topo_figure.xml";
    std::string dotPath = ""; // 若非空导出 .dot
    double dotScale = 80.0;   // dot 坐标缩放
    bool enablePcap = false;
    bool enableAnim = true;

    // 距离->时延控制（默认启用）
    bool delayByDist = true;       // 1=按距离计算，0=用 CSV delay
    double meterPerUnit = 50000.0; // 1 坐标单位=多少米；默认 50 km，得到毫秒级时延
    double propSpeed = 2e8;        // 传播速度 m/s（光纤近似）
    double delayFactor = 1.0;      // 额外缩放系数

    // --- Pro-Sink App 参数 ---
    double simulationStepMs = 1.0;                             // 默认步长 1ms
    double proAppUpdateIntervalSec = 0.25;                      // 默认算力更新间隔 0.25s
    std::string proSinkXmlFile = "pro_sink_stats.xml"; // 默认 XML 输出文件名
    std::string nodeUtilXmlFile = "node_util.xml"; // 默认利用率 XML 输出文件名

    // --- 功率/成本 参数 ---
    bool enablePowerCoupling = false; // 默认关闭
    std::string priceCsv = "daily_price.csv";
    std::string powerCostXmlBase = "scratch/ns3-dsw/out/power_cost"; 

    // --- 链路监控参数 ---
    double linkUtilIntervalSec = 0.25; // 默认 0.25s 轮询
    std::string linkUtilXmlFile = "scratch/ns3-dsw/out/link_util.xml"; // 默认 XML
    bool enableLinkUtil = true; // 默认启用

    // --- 价格感知调度参数 ---
    bool enablePriceAwareScheduling = false; // 默认关闭
    double loadDecayFactor = 0.5; // 负载衰减因子 (0.0=无衰减, 1.0=完全衰减)
    std::string schedulerLogPath = "scratch/ns3-dsw/out/scheduler_events.xml";
    // Tanh 归一化成本函数参数
    double maxCongestionPenalty = 0.5;    // W_base: 最大拥塞惩罚
    double congestionSensitivity = 2.0;   // K_c: 拥塞敏感度（秒）
    double maxProducerPenalty = 3.0;      // W_prod: 最大生产者紧急度乘数
    double producerSensitivity = 100.0;   // K_p: 生产者敏感度（任务数）

    // --- End MODIFICATION ---

    CommandLine cmd;
    cmd.AddValue("nodes", "CSV of nodes: id[,x,y[,name,rate,baseP,fullP,phase]]", nodesCsv);
    cmd.AddValue("links", "CSV of links: a,b,rate[,id]", linksCsv);
    cmd.AddValue("warmupTime", "Simulation warmup time (s)", warmupTime);
    cmd.AddValue("simDuration", "Actual simulation duration (s) (1s=1h)", simDuration);
    cmd.AddValue("pcap", "Enable pcap on all links (0/1)", enablePcap);
    cmd.AddValue("anim", "Enable NetAnim output (0/1)", enableAnim);
    cmd.AddValue("log", "Log level: off|warn|info|debug|all", logLevel);
    cmd.AddValue("flowXml", "FlowMonitor XML output", flowmonXml);
    cmd.AddValue("statsCsv", "Write per-flow stats to CSV (path)", statsCsv);
    cmd.AddValue("animXml", "NetAnim XML output", animXml);
    cmd.AddValue("dot", "Write Graphviz .dot to this path (empty to disable)", dotPath);
    cmd.AddValue("dotScale", "Scale factor for coordinates in .dot", dotScale);

    // 按距离计算时延的参数
    cmd.AddValue("delayByDist", "If 1, compute link delay from node distance", delayByDist);
    cmd.AddValue("meterPerUnit", "Meters per coordinate unit", meterPerUnit);
    cmd.AddValue("propSpeed", "Propagation speed (m/s)", propSpeed);
    cmd.AddValue("delayFactor", "Extra multiplier for computed delay", delayFactor);

    // --- Pro-Sink App 命令行参数 ---
    cmd.AddValue("simulationStep", "Simulation step for Pro-Sink App (ms)", simulationStepMs);
    cmd.AddValue("proAppUpdateInterval", "Pro-Sink App utilization/power report interval (s)", proAppUpdateIntervalSec);
    cmd.AddValue("proSinkXml", "Pro-Sink App XML output file", proSinkXmlFile);
    cmd.AddValue("nodeUtilXml", "Node Utilization XML output file", nodeUtilXmlFile);

    // --- Added power params ---
    cmd.AddValue("enablePowerCoupling", "Enable Power/Cost Coupling features (0/1)", enablePowerCoupling);
    cmd.AddValue("priceCsv", "Path to the 288-point (5min) price CSV", priceCsv);
    cmd.AddValue("powerCostXmlBase", "Base path for power/cost XML output", powerCostXmlBase);

    // --- 链路监控命令行参数 ---
    cmd.AddValue("enableLinkUtil", "Enable Link Utilization Monitor (0/1)", enableLinkUtil);
    cmd.AddValue("linkUtilInterval", "Link Utilization Monitor poll interval (s)", linkUtilIntervalSec);
    cmd.AddValue("linkUtilXml", "Link Utilization Monitor XML output file", linkUtilXmlFile);

    // --- 价格感知调度命令行参数 ---
    cmd.AddValue("enablePriceAwareScheduling", "Enable Price-Aware Task Scheduling (0/1)", enablePriceAwareScheduling);
    cmd.AddValue("loadDecayFactor", "Load decay factor for price-aware scheduling (0.0-1.0)", loadDecayFactor);
    cmd.AddValue("maxCongestionPenalty", "Max congestion penalty (W_base)", maxCongestionPenalty);
    cmd.AddValue("congestionSensitivity", "Congestion sensitivity in seconds (K_c)", congestionSensitivity);
    cmd.AddValue("maxProducerPenalty", "Max producer urgency multiplier (W_prod)", maxProducerPenalty);
    cmd.AddValue("producerSensitivity", "Producer sensitivity in tasks (K_p)", producerSensitivity);
    cmd.AddValue("schedulerLogPath", "Path to scheduler events XML log", schedulerLogPath);

    cmd.Parse(argc, argv);
    VisualizationConfig::SetupLogging(logLevel);

    // --- 创建和配置链路监控器 ---
    Ptr<LinkUtilizationMonitor> linkMonitor = nullptr;
    if (enableLinkUtil)
    {
        linkMonitor = CreateObject<LinkUtilizationMonitor>();
        linkMonitor->SetPollInterval(Seconds(linkUtilIntervalSec));
        linkMonitor->SetXmlOutput(linkUtilXmlFile);
        NS_LOG_INFO("Link Utilization Monitor enabled. Interval=" << linkUtilIntervalSec
                                                                 << "s, Output=" << linkUtilXmlFile);
    }

    // --- TCP Pacing 关键配置 (从 example 复制) ---
    NS_LOG_INFO("Enabling TCP Cubic and Pacing...");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                       TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", 
                       BooleanValue(true));
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", 
                       BooleanValue(true));
    // --- TCP Pacing ---

    // 确保 XML 输出在 scratch/ns3-dsw/out/ 目录下
    // 检查路径是否已经包含 scratch/ns3-dsw/out/ 前缀
    if (proSinkXmlFile.find("scratch/ns3-dsw/out/") == std::string::npos)
    {
        proSinkXmlFile = "scratch/ns3-dsw/out/" + proSinkXmlFile;
    }
    if (nodeUtilXmlFile.find("scratch/ns3-dsw/out/") == std::string::npos)
    {
        nodeUtilXmlFile = "scratch/ns3-dsw/out/" + nodeUtilXmlFile;
    }


    // --- 解析 Pro-Sink 时间参数 ---
    Time simulationStep = MilliSeconds(simulationStepMs);
    Time proAppUpdateInterval = Seconds(proAppUpdateIntervalSec);
    double proAppStartTime = warmupTime;
    double proAppStopTime = proAppStartTime + simDuration;
    NS_LOG_INFO("Pro-Sink simulation step: " << simulationStep);
    NS_LOG_INFO("Pro-Sink Apps will run from " << proAppStartTime << "s to " << proAppStopTime << "s (Duration: " << simDuration << "s)");
    
    if (enablePowerCoupling)
    {
        NS_LOG_INFO("Power/Cost Coupling ENABLED. Update interval: " << proAppUpdateInterval.GetSeconds() << "s");
    }
    else
    {
        NS_LOG_INFO("Power/Cost Coupling DISABLED.");
    }

    std::vector<double> priceProfile;
    if (enablePowerCoupling)
    {
        priceProfile = LoadCsvPrices(priceCsv);
        if (priceProfile.empty())
        {
            NS_FATAL_ERROR("Failed to load price profile or profile is empty: " << priceCsv);
        }
        if (priceProfile.size() != 288)
        {
            NS_LOG_WARN("Price profile does not contain 288 points (got " << priceProfile.size() 
                        << "). Time scaling may be incorrect.");
        }
    }

    // --- 读取配置 ---
    auto nodeSpecs = DataParser::LoadCsvNodes(nodesCsv);
    auto linkSpecs = DataParser::LoadCsvLinks(linksCsv);
    if (nodeSpecs.empty())
        NS_FATAL_ERROR("No nodes parsed from " << nodesCsv);
    if (linkSpecs.empty())
        NS_FATAL_ERROR("No links parsed from " << linksCsv);

    // 节点集合与最大 ID
    std::set<uint32_t> nodeIds;
    uint32_t maxId = 0;
    for (const auto& n : nodeSpecs)
    {
        nodeIds.insert(n.id);
        maxId = std::max(maxId, n.id);
    }
    for (const auto& l : linkSpecs)
    {
        nodeIds.insert(l.a);
        nodeIds.insert(l.b);
        maxId = std::max(maxId, std::max(l.a, l.b));
    }

    NS_LOG_INFO("Nodes in config: " << nodeIds.size() << " (max id=" << maxId << ")");
    NS_LOG_INFO("Links in config: " << linkSpecs.size());

    // 使用 TopologyBuilder 构建拓扑
    auto buildResult = TopologyBuilder::BuildTopology(nodeSpecs, linkSpecs, maxId, delayByDist,
                                                       meterPerUnit, propSpeed, delayFactor,
                                                       enablePcap);

    NodeContainer& nodes = buildResult.nodes;
    std::map<std::pair<uint32_t, uint32_t>, ns3::IfRecord>& ifMap = buildResult.ifMap;
    std::vector<ns3::Vector>& pos = buildResult.positions;

    // 构建 NodeSpec 映射表
    std::map<uint32_t, ns3::NodeSpec> nodeSpecMap;
    for (const auto& ns : nodeSpecs)
    {
        nodeSpecMap[ns.id] = ns;
    }

    // --- 注册链路监控 ---
    if (enableLinkUtil && linkMonitor)
    {
        NS_LOG_INFO("Registering links with LinkUtilizationMonitor (post-TCH)...");
        // 遍历 ifMap, 它包含了所有已安装链路的 NetDevice 信息
        for (const auto& kv : ifMap)
        {
            const ns3::IfRecord& rec = kv.second;
            // rec.a 和 rec.b 是原始的 linkSpec.a 和 linkSpec.b
            // rec.ifc.Get(0) 对应 rec.a 上的设备
            // rec.ifc.Get(1) 对应 rec.b 上的设备
            linkMonitor->RegisterLink(rec.id,       // linkId
                                      rec.a,        // nodeAId
                                      rec.b,        // nodeBId
                                      rec.ifc.Get(0).first->GetNetDevice(rec.ifc.Get(0).second), // devA
                                      rec.ifc.Get(1).first->GetNetDevice(rec.ifc.Get(1).second), // devB
                                      DataRate(rec.rate)); // rate
        }
    }

    if (ifMap.empty())
    {
        NS_FATAL_ERROR("No valid links created.");
    }

    // --- 构建节点 ID 到 IP 的映射，并收集消费者地址 ---
    // Pro-Sink App 需要知道目标 IP 地址
    std::map<uint32_t, Ipv4Address> nodeIpMap;
    for (uint32_t nodeId : nodeIds)
    {
        if (nodeId == 0) continue; 

        Ptr<Ipv4> ipv4 = nodes.Get(nodeId)->GetObject<Ipv4>();
        if (ipv4->GetNInterfaces() <= 1)
        {
            NS_LOG_WARN("Node " << nodeId << " has no P2P interfaces, cannot be reached.");
            continue;
        }
        Ipv4Address ip = ipv4->GetAddress(1, 0).GetLocal();
        nodeIpMap[nodeId] = ip;
        NS_LOG_DEBUG("Node " << nodeId << " mapped to routable IP: " << ip);
    }

    uint16_t proPort = 8080;            // Pro-Sink App 使用的端口 (必须与 MySink::m_port 匹配)
    std::vector<Address> sinkAddresses; // 存储所有消费者的地址
    bool hasProducers = false;

    // --- 遍历 nodeSpecs 收集消费者地址 ---
    for (const auto& ns : nodeSpecs)
    {
        if (ns.type == ns3::NodeType::CONSUMER)
        {
            if (nodeIpMap.count(ns.id))
            {
                sinkAddresses.push_back(InetSocketAddress(nodeIpMap[ns.id], proPort));
                NS_LOG_INFO("Consumer " << ns.id << " (core) ... Rate=" << ns.appRate
                                    << " P_base=" << ns.basePower << ", P_full=" << ns.fullPower
                                    << ", Phase=" << ns.phaseOffset << "h");
            }
            else
            {
                NS_LOG_WARN("Specified consumer node " << ns.id << " not found or has no IP.");
            }
        }
        else if (ns.type == ns3::NodeType::PRODUCER)
        {
            hasProducers = true;
        }
    }

    if (sinkAddresses.empty() && hasProducers)
    {
        NS_FATAL_ERROR("Producers (edge nodes) exist, but no valid consumer (core nodes) addresses "
                       "were found.");
    }

    // 路由
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    NS_LOG_INFO("Global routes populated.");

    // --- 安装 Pro-Sink 应用 ---
    // Pro-Sink 应用参数 (硬编码)
    uint32_t proTaskSize = 256 * 1024; // (Bytes)
    uint32_t proPacketSize = 1024;     // (Bytes)

    // --- 创建价格感知调度器 ---
    Ptr<PriceAwareScheduler> scheduler = nullptr;
    if (enablePriceAwareScheduling)
    {
        NS_LOG_INFO("Creating PriceAwareScheduler...");
        scheduler = ApplicationManager::CreatePriceAwareScheduler(
            nodeSpecMap, sinkAddresses, priceProfile, loadDecayFactor,
            maxCongestionPenalty, congestionSensitivity,
            maxProducerPenalty, producerSensitivity, schedulerLogPath);
        g_scheduler = scheduler; // 设置全局变量
        NS_LOG_INFO("PriceAwareScheduler created successfully with load decay factor " << loadDecayFactor);
    }

    // 使用 ApplicationManager 安装所有应用
    auto installResult = ApplicationManager::InstallProSinkApps(
        nodes, nodeSpecMap, nodeIds, sinkAddresses, enablePowerCoupling, priceProfile,
        powerCostXmlBase, proAppStartTime, proAppStopTime, simulationStep,
        proAppUpdateInterval, proTaskSize, proPacketSize, warmupTime);

    std::vector<Ptr<MyProducer>>& producers = installResult.producers;
    std::vector<Ptr<MySink>>& sinks = installResult.sinks;

    // --- 打开 XML 文件并连接 Traces ---
    g_xmlFile.open(proSinkXmlFile);
    if (!g_xmlFile.is_open())
    {
        NS_LOG_ERROR("Failed to open " << proSinkXmlFile << " for writing.");
    }
    else
    {
        g_xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        g_xmlFile << "<ProSinkStats simulationStep=\"" << simulationStep << "\" duration=\""
                  << simDuration << "\">" << std::endl;
    }
    // --- Open core util XML file ---
    g_utilXmlFile.open(nodeUtilXmlFile);
    if (!g_utilXmlFile.is_open())
    {
        NS_LOG_ERROR("Failed to open " << nodeUtilXmlFile << " for writing.");
    }
    else
    {
        g_utilXmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        g_utilXmlFile << "<NodeUtilizationStats simulationStep=\"" << simulationStep << "\" duration=\""
                        << simDuration << "\">" << std::endl;
    }

    // 连接回调函数
    for (auto& sink : sinks)
    {
        sink->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&OnSinkTaskCompleted));
        sink->TraceConnectWithoutContext("Utilization", MakeCallback(&OnSinkUtilization));
        sink->TraceConnectWithoutContext("QueueLength", MakeCallback(&OnSinkQueueLength));

        // 如果启用了价格感知调度，将Sink状态传递给调度器
        if (scheduler)
        {
            // 使用静态回调函数
            sink->TraceConnectWithoutContext("Utilization", MakeCallback(&OnSinkUtilizationForScheduler));
            sink->TraceConnectWithoutContext("QueueLength", MakeCallback(&OnSinkQueueLengthForScheduler));
            NS_LOG_INFO("Connected sink " << sink->GetNode()->GetId() << " to PriceAwareScheduler");
        }
    }
    for (auto& producer : producers)
    {
        // 将producer添加到全局映射中，以便在trace回调中获取统计信息
        g_producers[producer->GetNode()->GetId()] = producer;

        producer->TraceConnectWithoutContext("TaskSent", MakeCallback(&OnProducerTaskSentWrapper));

        // 配置价格感知调度
        if (scheduler)
        {
            producer->EnablePriceAwareScheduling(true);
            producer->SetScheduler(scheduler);
            NS_LOG_INFO("Configured producer on node " << producer->GetNode()->GetId()
                       << " with PriceAwareScheduler");
        }
    }

    // NetAnim：高亮 server/client
    if (enableAnim)
    {
        AnimationInterface anim(animXml);
        for (uint32_t id : nodeIds)
        {
            auto n = nodes.Get(id);
            std::ostringstream label;
            auto nm = Names::FindName(n);
            if (nm.empty())
                label << id;
            else
                label << id << ":" << nm;
            anim.UpdateNodeDescription(n, label.str());
            
            // 根据新类型为节点着色
            if (nodeSpecMap.count(id))
            {
                if(nodeSpecMap.at(id).type == ns3::NodeType::PRODUCER)
                {
                    anim.UpdateNodeColor(n, 255, 0, 0); // 红色 (Producer)
                }
                else if (nodeSpecMap.at(id).type == ns3::NodeType::CONSUMER)
                {
                     anim.UpdateNodeColor(n, 0, 0, 255); // 蓝色 (Consumer)
                }
                else
                {
                    anim.UpdateNodeColor(n, 100, 100, 100); // 灰色 (Router)
                }
            }
        }
        NS_LOG_INFO("NetAnim written: " << animXml);
    }

    // FlowMonitor
    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> monitor = fmh.InstallAll();

    // --- 设置总仿真停止时间 ---
    Time simStopTime = Seconds(proAppStopTime + 0.5); // 增加 0.5s 缓冲
    Simulator::Stop(simStopTime);
    NS_LOG_INFO("Simulation will stop at " << simStopTime.GetSeconds() << "s.");

    // 调度链路监控器的启动和停止
    if (enableLinkUtil && linkMonitor)
    {
        // 在 App 启动时开始监控
        Simulator::Schedule(Seconds(proAppStartTime), &LinkUtilizationMonitor::Start, linkMonitor);
        // 在仿真停止时停止监控
        Simulator::Schedule(simStopTime, &LinkUtilizationMonitor::Stop, linkMonitor);
    }

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::ofstream csv;
    if (!statsCsv.empty())
    {
        csv.open(statsCsv);
        csv << "flowId,src,dst,proto,sPort,dPort,tx,rx,lost,throughput_Mbps,avgDelay_ms,avgJitter_"
               "ms\n";
    }

    std::cout << "\n========== FlowMonitor per-flow statistics ==========\n";
    double sumThr = 0.0, sumDelay = 0.0, sumJit = 0.0;
    uint32_t rxFlows = 0;

    for (const auto& kv : stats)
    {
        FlowId id = kv.first;
        const auto& st = kv.second;
        auto t = classifier->FindFlow(id);

        double dur = st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds();
        if (dur <= 0.0)
            dur = 1e-9;
        double thr = st.rxBytes * 8.0 / dur / 1e6;
        double dly = (st.rxPackets > 0) ? (st.delaySum.GetSeconds() / st.rxPackets * 1000.0) : 0.0;
        double jit =
            (st.rxPackets > 1) ? (st.jitterSum.GetSeconds() / (st.rxPackets - 1) * 1000.0) : 0.0;

        std::cout << "Flow " << id << "  " << t.sourceAddress << " -> " << t.destinationAddress
                  << "  proto=" << (uint32_t)t.protocol << " sPort=" << t.sourcePort
                  << " dPort=" << t.destinationPort << "\n"
                  << "  TxPkts=" << st.txPackets << " RxPkts=" << st.rxPackets
                  << " Lost=" << st.lostPackets << "  Throughput=" << std::fixed
                  << std::setprecision(3) << thr << " Mbps"
                  << "  AvgDelay=" << std::setprecision(3) << dly << " ms"
                  << "  AvgJitter=" << std::setprecision(3) << jit << " ms\n";

        if (!statsCsv.empty())
        {
            csv << id << "," << t.sourceAddress << "," << t.destinationAddress << ","
                << (uint32_t)t.protocol << "," << t.sourcePort << "," << t.destinationPort << ","
                << st.txPackets << "," << st.rxPackets << "," << st.lostPackets << "," << std::fixed
                << std::setprecision(6) << thr << "," << dly << "," << jit << "\n";
        }
        if (st.rxPackets > 0)
        {
            ++rxFlows;
            sumThr += thr;
            sumDelay += dly;
            sumJit += jit;
        }
    }
    if (!statsCsv.empty())
        csv.close();

    if (rxFlows > 0)
    {
        std::cout << "\n--- Aggregated over " << rxFlows << " received flows ---  "
                  << "MeanThr=" << (sumThr / rxFlows) << " Mbps  "
                  << "MeanDelay=" << (sumDelay / rxFlows) << " ms  "
                  << "MeanJitter=" << (sumJit / rxFlows) << " ms\n";
    }

    monitor->SerializeToXmlFile(flowmonXml, true, true);
    NS_LOG_INFO("FlowMonitor XML written: " << flowmonXml);


    // Graphviz 可视化导出
    if (!dotPath.empty())
    {
        VisualizationConfig::WriteGraphvizDot(dotPath, nodeIds, pos, ifMap, dotScale);
    }

    // --- 关闭 XML 文件 ---
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "</ProSinkStats>" << std::endl;
        g_xmlFile.close();
        std::cout << "[stats] Pro-Sink XML written: " << proSinkXmlFile << std::endl;
    }
    // --- core util XML file ---
    if (g_utilXmlFile.is_open())
    {
        g_utilXmlFile << "</NodeUtilizationStats>" << std::endl;
        g_utilXmlFile.close();
        std::cout << "[stats] Node Utilization XML written: " << nodeUtilXmlFile << std::endl;
    }

    Simulator::Destroy();
    std::cout << "\nDone.\n";
    return 0;
}