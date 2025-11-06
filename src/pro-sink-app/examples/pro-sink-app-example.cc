/*
pro-sink-app/example/pro-sink-example.cc
一个简单的p2p拓扑，装载了消费者和生产者应用 (已更新为 TCP Pacing)

to run:
./ns3 run pro-sink-app-example -- --consumerRatePerSecond=20 --lambda=25
*/


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// 包含流量控制模块
#include "ns3/traffic-control-module.h"

// 包含新建模块
#include "ns3/pro-sink-app-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTaskSimulationExample");

// --- 1. 定义 Trace Callback 函数 ---

/**
 * @brief 生产者 "TaskSent" Trace 的回调函数
 * @param nodeId 触发该 Trace 的生产者节点 ID
 * @param taskId 正在发送的任务 ID (总发送数)
 * @param targetAddress 任务发送的目标地址
 */
void
TaskSentCallback(uint32_t nodeId, uint32_t taskId, Address targetAddress)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds()
                  << "s: [TRACE] Producer Node " << nodeId << ": Task " << nodeId << "-" << taskId
                  << " sent to " << InetSocketAddress::ConvertFrom(targetAddress).GetIpv4());
}

/**
 * @brief 消费者 "TaskCompleted" Trace 的回调函数
 * @param nodeId 触发该 Trace 的消费者节点 ID
 * @param producerId 任务来源的生产者 ID
 * @param taskId 任务的任务 ID
 * @param totalCompleted 该节点已完成的总任务数
 */
void
TaskCompletedCallback(uint32_t nodeId, uint32_t producerId, uint32_t taskId, uint32_t totalCompleted)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [TRACE] Sink Node " << nodeId
                                               << ": Task " << producerId << "-" << taskId << " completed. Total: "
                                               << totalCompleted);
}

/**
 * @brief (新增) 消费者 "Utilization" Trace 的回调函数
 * @param utilization 算力利用率 (0.0 to 1.0)
 */
void
UtilizationCallback(uint32_t nodeId, double utilization)
{
    // MySink 内部已经用 NS_LOG_UNCOND 打印了利用率。
    // 这个 Trace 主要用于脚本进行二次处理，例如写入 Gnuplot 文件。
    // 我们在这里用 LOG_INFO 打印，以演示 Trace 已连接成功。
    //NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: [TRACE] Sink Node Utilization: " << utilization * 100.0 << "%");
}


int
main(int argc, char* argv[])
{
    // --- 参数定义 ---
    double lambda = 25.0; // 平均每秒生成任务数
    double simulationTime = 1.0; // 默认模拟 1 秒
    double simulationStepMs = 1.0; // tick长度
    double consumerRatePerSecond = 20.0; // 每秒消纳任务数
    double updateIntervalSec = 0.25; // (新增) 算力利用率报告间隔，默认 1 秒

    CommandLine cmd(__FILE__);
    cmd.AddValue("lambda", "生产者平均每秒生成的任务数", lambda);
    cmd.AddValue("simulationTime", "模拟总时长 (秒)", simulationTime);
    cmd.AddValue("step", "模拟步长 (毫秒)", simulationStepMs);
    cmd.AddValue("consumerRatePerSecond", "消费者每秒处理的任务数", consumerRatePerSecond);
    cmd.AddValue("updateInterval", "算力利用率报告间隔 (秒)", updateIntervalSec); // (新增)
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("P2PTaskSimulationExample", LOG_LEVEL_INFO);
    
    // (ProSinkApp 的日志由 MySink 内部的 NS_LOG_UNCOND 控制，无需在此启用)
    // LogComponentEnable("ProSinkApp", LOG_LEVEL_INFO); 

    // --- TCP Pacing 关键配置 ---
    
    // 设置默认的 TCP 拥塞控制算法 (Cubic)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", 
                       TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));

    // 1. 启用 Pacing
    Config::SetDefault("ns3::TcpSocketState::EnablePacing", 
                       BooleanValue(true));

    // 2. (可选) 启用初始窗口的 Pacing
    Config::SetDefault("ns3::TcpSocketState::PaceInitialWindow", 
                       BooleanValue(true));


    // --- 网络拓扑设置 ---
    NodeContainer producerNodes, consumerNodes;
    producerNodes.Create(1); // 生产者 Node 0
    consumerNodes.Create(1); // 消费者 Node 1

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 连接 Node 0 和 Node 1
    NetDeviceContainer p2pDevices;
    p2pDevices.Add(pointToPoint.Install(producerNodes.Get(0), consumerNodes.Get(0)));

    InternetStackHelper stack;
    stack.Install(producerNodes);
    stack.Install(consumerNodes);

    // --- 安装 FqCoDel 队列以支持 Pacing ---
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    tch.Install(p2pDevices);


    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(p2pDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- 应用层设置 ---
    Time simStep = MilliSeconds(simulationStepMs);
    Time updateInterval = Seconds(updateIntervalSec); // (新增)
    uint16_t port = 8080;
    uint32_t taskSize = 256 * 1024;
    uint32_t packetSize = 1024; // 必须与 MyProducer/MySink 中的 m_packetSize 匹配

    // 1. 配置并安装消费者应用 (MySink)
    Ptr<MySink> sinkApp = CreateObject<MySink>();
    
    // (修改) Setup 函数现在需要 3 个参数
    sinkApp->Setup(consumerRatePerSecond, simStep, updateInterval); 
    
    // 通过 Attribute 设置 TaskSize 和 PacketSize
    sinkApp->SetAttribute("TaskSize", UintegerValue(taskSize));
    sinkApp->SetAttribute("PacketSize", UintegerValue(packetSize));
    consumerNodes.Get(0)->AddApplication(sinkApp); // 安装在 Node 1
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(Seconds(simulationTime + 0.3));

    // 2. 配置并安装生产者应用 (MyProducer)
    Ptr<MyProducer> producerApp = CreateObject<MyProducer>();
    
    // MyProducer::Setup 现在接受单个 Address
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    
    producerApp->Setup(sinkAddress, lambda, taskSize, packetSize, simStep);
    // 通过 Attribute 设置 TaskSize 和 PacketSize
    producerApp->SetAttribute("TaskSize", UintegerValue(taskSize));
    producerApp->SetAttribute("PacketSize", UintegerValue(packetSize));
    producerNodes.Get(0)->AddApplication(producerApp); // 安装在 Node 0
    producerApp->SetStartTime(Seconds(0.1));
    producerApp->SetStopTime(Seconds(simulationTime));

    // --- 3. 连接 Trace Source ---
    sinkApp->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&TaskCompletedCallback));
    sinkApp->TraceConnectWithoutContext("Utilization", MakeCallback(&UtilizationCallback)); // (新增)
    producerApp->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSentCallback));

    // --- 运行仿真 ---
    NS_LOG_INFO("开始运行仿真...");
    Simulator::Stop(Seconds(simulationTime + 0.5));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("仿真结束。");
    return 0;
}