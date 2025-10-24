/*
pro-sink-app/example/pro-sink-example.cc
一个简单的p2p拓扑，装载了消费者和生产者应用

to run:
./ns3 run pro-sink-app-example

如果运行不了
./ns3 configure --enable-example
*/


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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
                  << "s: [TRACE] Producer Node " << nodeId << ": Task " << taskId
                  << " sent to " << InetSocketAddress::ConvertFrom(targetAddress).GetIpv4());
}

/**
 * @brief 消费者 "TaskCompleted" Trace 的回调函数
 * @param nodeId 触发该 Trace 的消费者节点 ID
 * @param taskId 刚处理完成的任务 ID
 * @param totalCompleted 该节点已完成的总任务数
 */
void
TaskCompletedCallback(uint32_t nodeId, uint32_t taskId, uint32_t totalCompleted)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [TRACE] Sink Node " << nodeId
                                               << ": Task " << taskId << " completed. Total: "
                                               << totalCompleted);
}

int
main(int argc, char* argv[])
{
    // --- 参数定义 ---
    double lambda = 60.0; // 平均每秒生成任务数
    double simulationTime = 0.4;
    double simulationStepMs = 20.0; // tick长度
    double consumerRatePerSecond = 40.0; // 每秒消纳任务数

    CommandLine cmd(__FILE__);
    cmd.AddValue("lambda", "生产者平均每秒生成的任务数", lambda);
    cmd.AddValue("simulationTime", "模拟总时长 (秒)", simulationTime);
    cmd.AddValue("step", "模拟步长 (毫秒)", simulationStepMs);
    cmd.AddValue("consumerRatePerSecond", "消费者每秒处理的任务数", consumerRatePerSecond);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("P2PTaskSimulationExample", LOG_LEVEL_INFO);
    LogComponentEnable("ProSinkApp", LOG_LEVEL_INFO); // 开启日志 (INFO及更高级别)

    // --- 网络拓扑设置 ---
    NodeContainer producerNodes, consumerNodes;
    producerNodes.Create(1);
    consumerNodes.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer producerDevices =
        pointToPoint.Install(producerNodes.Get(0), consumerNodes.Get(0));

    InternetStackHelper stack;
    stack.Install(producerNodes);
    stack.Install(consumerNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(producerDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- 应用层设置 ---
    Time simStep = MilliSeconds(simulationStepMs);
    uint16_t port = 8080;

    // 1. 配置并安装消费者应用 (MySink)
    Ptr<MySink> sinkApp = CreateObject<MySink>();
    sinkApp->Setup(consumerRatePerSecond, simStep);
    consumerNodes.Get(0)->AddApplication(sinkApp);
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(Seconds(simulationTime));

    // 2. 配置并安装生产者应用 (MyProducer)
    Ptr<MyProducer> producerApp = CreateObject<MyProducer>();
    std::vector<Address> sinkAddresses;
    sinkAddresses.push_back(InetSocketAddress(interfaces.GetAddress(1), port));
    producerApp->Setup(sinkAddresses, lambda, 256 * 1024, 1024, simStep);
    producerNodes.Get(0)->AddApplication(producerApp);
    producerApp->SetStartTime(Seconds(0.1));
    producerApp->SetStopTime(Seconds(simulationTime));

    // --- 3. 连接 Trace Source  ---
    // 将 "TaskCompleted" Trace 连接到我们的回调函数
    sinkApp->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&TaskCompletedCallback));
    
    // 将 "TaskSent" Trace 连接到我们的回调函数
    producerApp->TraceConnectWithoutContext("TaskSent", MakeCallback(&TaskSentCallback));

    // --- 运行仿真 ---
    NS_LOG_INFO("开始运行仿真...");
    Simulator::Stop(Seconds(simulationTime + 0.5));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("仿真结束。");
    return 0;
}