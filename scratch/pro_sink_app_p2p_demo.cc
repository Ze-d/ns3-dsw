// 使用封装好的应用，在p2p拓扑下的一个demo.

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

int
main(int argc, char* argv[])
{
    // --- 参数定义 ---
    double lambda = 60.0;//平均每秒生成任务数
    double simulationTime = 0.4;
    double simulationStepMs = 20.0;//tick长度
    double consumerRatePerSecond = 40.0;//每秒消纳任务数

    CommandLine cmd(__FILE__);
    cmd.AddValue("lambda", "生产者平均每秒生成的任务数", lambda);
    cmd.AddValue("simulationTime", "模拟总时长 (秒)", simulationTime);
    cmd.AddValue("step", "模拟步长 (毫秒)", simulationStepMs);
    cmd.AddValue("consumerRatePerSecond", "消费者每秒处理的任务数", consumerRatePerSecond);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("P2PTaskSimulationExample", LOG_LEVEL_INFO);
    LogComponentEnable("ProSinkApp", LOG_LEVEL_INFO); // 开启日志

    // --- 网络拓扑设置 ---
    NodeContainer producerNodes, consumerNodes;
    producerNodes.Create(1);
    consumerNodes.Create(1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer producerDevices = pointToPoint.Install(producerNodes.Get(0), consumerNodes.Get(0));

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

    // --- 运行仿真 ---
    NS_LOG_INFO("开始运行仿真...");
    Simulator::Stop(Seconds(simulationTime + 0.5));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("仿真结束。");
    return 0;
}

