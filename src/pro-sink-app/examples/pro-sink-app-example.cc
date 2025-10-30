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

#include "ns3/drop-tail-queue.h"

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
    // [修改] 将 Trace 日志的任务ID格式修改为 "nodeId-taskId"
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
    // [修改] 更新此函数以匹配新的 TraceSource 签名
    // 现在我们可以打印 "producerId-taskId" 格式了
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [TRACE] Sink Node " << nodeId
                                               << ": Task " << producerId << "-" << taskId << " completed. Total: "
                                               << totalCompleted);
}


int
main(int argc, char* argv[])
{
    // --- 参数定义 ---
    double lambda = 25.0; // 平均每秒生成任务数
    double simulationTime = 1.0;
    double simulationStepMs = 1.0; // tick长度
    double consumerRatePerSecond = 20.0; // 每秒消纳任务数

    CommandLine cmd(__FILE__);
    cmd.AddValue("lambda", "生产者平均每秒生成的任务数", lambda);
    cmd.AddValue("simulationTime", "模拟总时长 (秒)", simulationTime);
    cmd.AddValue("step", "模拟步长 (毫秒)", simulationStepMs);
    cmd.AddValue("consumerRatePerSecond", "消费者每秒处理的任务数", consumerRatePerSecond);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("P2PTaskSimulationExample", LOG_LEVEL_INFO);
    // [重要] 确保 ProSinkApp 的日志级别为 INFO 或更低，以查看 NS_LOG_UNCOND
    LogComponentEnable("ProSinkApp", LOG_LEVEL_INFO); 

    // --- 网络拓扑设置 ---
    NodeContainer producerNodes, consumerNodes;
    producerNodes.Create(1); // 生产者 Node 0
    consumerNodes.Create(1); // 消费者 Node 1

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 手动创建一个 DropTailQueue，设置其属性
    Ptr<DropTailQueue<Packet>> q = CreateObject<DropTailQueue<Packet>>();

    q->SetAttribute("MaxSize", QueueSizeValue(QueueSize("4000p")));

    pointToPoint.SetDeviceAttribute("TxQueue", PointerValue(q));

    // 连接 Node 0 和 Node 1
    NetDeviceContainer p2pDevices;
    p2pDevices.Add(pointToPoint.Install(producerNodes.Get(0), consumerNodes.Get(0)));
    // 生产者总共发送 50 个任务 * 256 包/任务 = 12800 包。

    InternetStackHelper stack;
    stack.Install(producerNodes);
    stack.Install(consumerNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    // p2pDevices.Get(0) 是生产者 (10.1.1.1)
    // p2pDevices.Get(1) 是消费者 (10.1.1.2)
    Ipv4InterfaceContainer interfaces = address.Assign(p2pDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- 应用层设置 ---
    Time simStep = MilliSeconds(simulationStepMs);
    uint16_t port = 8080;

    // 1. 配置并安装消费者应用 (MySink)
    Ptr<MySink> sinkApp = CreateObject<MySink>();
    sinkApp->Setup(consumerRatePerSecond, simStep);
    consumerNodes.Get(0)->AddApplication(sinkApp); // 安装在 Node 1
    sinkApp->SetStartTime(Seconds(0.0));
    sinkApp->SetStopTime(Seconds(simulationTime));

    // 2. 配置并安装生产者应用 (MyProducer)
    Ptr<MyProducer> producerApp = CreateObject<MyProducer>();
    std::vector<Address> sinkAddresses;
    // 目标地址是 Node 1 (消费者) 的 IP
    sinkAddresses.push_back(InetSocketAddress(interfaces.GetAddress(1), port));
    producerApp->Setup(sinkAddresses, lambda, 256 * 1024, 1024, simStep);
    producerNodes.Get(0)->AddApplication(producerApp); // 安装在 Node 0
    producerApp->SetStartTime(Seconds(0.1));
    producerApp->SetStopTime(Seconds(simulationTime));

    // --- 3. 连接 Trace Source  ---
    // [修改] 取消注释以激活 Trace
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