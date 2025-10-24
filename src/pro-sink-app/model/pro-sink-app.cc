#include "pro-sink-app.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/uinteger.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ProSinkApp"); 

// --- 1. MySink 实现 ---

TypeId MySink::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MySink")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<MySink>()
        // 添加 TraceSource
        .AddTraceSource("TaskCompleted",
                        "Trace triggered when a task is completed.",
                        MakeTraceSourceAccessor(&MySink::m_taskCompletedTrace),
                        "ns3::TracedCallback<uint32_t, uint32_t, uint32_t>")
    ;
    return tid;
}

MySink::MySink()
    : m_socket(nullptr),
      m_port(8080),
      m_taskSize(256 * 1024),
      m_currentRxBytes(0),
      m_nextTaskId(1),
      m_simulationStep(MilliSeconds(1)),
      m_tasksCompleted(0),
      m_tasksPerSecond(1000.0),
      m_processingCredit(0.0),
      m_running(false)
{
}

MySink::~MySink()
{
    m_socket = nullptr;
}

void
MySink::Setup(double tasksPerSecond, Time simulationStep)
{
    m_tasksPerSecond = tasksPerSecond;
    m_simulationStep = simulationStep;
}

void
MySink::StartApplication()
{
    if (m_socket == nullptr)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }
    m_socket->SetRecvCallback(MakeCallback(&MySink::HandleRead, this));
    m_running = true;
    Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
}

void
MySink::StopApplication()
{
    m_running = false;
    if (m_socket != nullptr)
    {
        m_socket->SetRecvCallback(Callback<void, Ptr<Socket>>());
    }
    NS_LOG_UNCOND("消费者应用停止。节点 " << GetNode()->GetId() << " 总共处理任务数: " << m_tasksCompleted << ". 队列中剩余任务数: " << m_taskQueue.size());
}

void
MySink::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        m_currentRxBytes += packet->GetSize();
        if (m_currentRxBytes >= m_taskSize)
        {
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " << m_nextTaskId << " 已完整接收并进入队列。");
            m_taskQueue.push(m_nextTaskId);
            m_currentRxBytes = 0;
            m_nextTaskId++;
        }
    }
}

void
MySink::ProcessTasks()
{
    if (!m_running) return;
    m_processingCredit += m_tasksPerSecond * m_simulationStep.GetSeconds();
    uint32_t tasksToProcess = floor(m_processingCredit);
    for (uint32_t i = 0; i < tasksToProcess && !m_taskQueue.empty(); ++i)
    {
        uint32_t taskId = m_taskQueue.front();
        m_taskQueue.pop();
        m_tasksCompleted++;
        m_processingCredit -= 1.0;
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " << taskId << " 处理完成。已完成总数: " << m_tasksCompleted);
        
        // 触发 Trace (nodeId, taskId, totalCompleted)
        m_taskCompletedTrace(GetNode()->GetId(), taskId, m_tasksCompleted);
    }
    if (m_running)
    {
        Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
    }
}

// --- 2. MyProducer 实现 ---

TypeId MyProducer::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MyProducer")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<MyProducer>()
        // 添加 TraceSource
        .AddTraceSource("TaskSent",
                        "Trace triggered when a new task starts sending.",
                        MakeTraceSourceAccessor(&MyProducer::m_taskSentTrace),
                        "ns3::TracedCallback<uint32_t, uint32_t, ns3::Address>")
    ;
    return tid;
}

MyProducer::MyProducer()
    : m_socket(nullptr),
      m_taskSize(0),
      m_packetSize(0),
      m_packetsSentForCurrentTask(0),
      m_totalTasksSent(0),
      m_isSending(false),
      m_simulationStep(MilliSeconds(1)),
      m_lambda(0.0),
      m_interTaskTimeGenerator(nullptr),
      m_sinkSelector(nullptr),
      m_running(false)
{
}

MyProducer::~MyProducer()
{
    m_socket = nullptr;
}

void
MyProducer::Setup(const std::vector<Address>& sinkAddresses, double lambda, uint32_t taskSize, uint32_t packetSize, Time simulationStep)
{
    m_sinkAddresses = sinkAddresses;
    m_taskSize = taskSize;
    m_packetSize = packetSize;
    m_simulationStep = simulationStep;
    m_lambda = lambda;
    m_interTaskTimeGenerator = CreateObject<ExponentialRandomVariable>();
    m_interTaskTimeGenerator->SetAttribute("Mean", DoubleValue(1.0 / m_lambda));
    m_sinkSelector = CreateObject<UniformRandomVariable>();
    m_sinkSelector->SetAttribute("Min", DoubleValue(0));
    m_sinkSelector->SetAttribute("Max", DoubleValue(m_sinkAddresses.size() > 0 ? m_sinkAddresses.size() - 1 : 0));
}

void
MyProducer::StartApplication()
{
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_running = true;
    Simulator::Schedule(m_simulationStep, &MyProducer::GenerateTasks, this);
}

void
MyProducer::StopApplication()
{
    m_running = false;
    if (m_socket != nullptr)
    {
        m_socket->Close();
    }
    NS_LOG_UNCOND("生产者应用停止。节点 " << GetNode()->GetId() << " 总共发送任务数: " << m_totalTasksSent << ". 队列中剩余任务数: " << m_taskQueue.size());
}

void
MyProducer::GenerateTasks()
{
    if (!m_running) return;
    uint32_t numTasksToGenerate = 0;
    double timeElapsedInStep = 0.0;
    while (true)
    {
        double nextInterval = m_interTaskTimeGenerator->GetValue();
        if (timeElapsedInStep + nextInterval > m_simulationStep.GetSeconds())
        {
            break; 
        }
        timeElapsedInStep += nextInterval;
        numTasksToGenerate++;
    }
    if (numTasksToGenerate > 0)
    {
         NS_LOG_INFO(Simulator::Now().GetSeconds() << "s: [生产者 " << GetNode()->GetId() << "]: 生成了 " << numTasksToGenerate << " 个新任务。");
        for (uint32_t i = 0; i < numTasksToGenerate; ++i)
        {
            m_taskQueue.push(true);
        }
        if (!m_isSending)
        {
            SendNextTask();
        }
    }
    if (m_running)
    {
        Simulator::Schedule(m_simulationStep, &MyProducer::GenerateTasks, this);
    }
}

void
MyProducer::SendNextTask()
{
    if (!m_running || m_taskQueue.empty())
    {
        m_isSending = false;
        return;
    }
    m_isSending = true;
    m_taskQueue.pop();
    m_totalTasksSent++;
    int sink_idx = m_sinkSelector->GetInteger();
    m_currentTarget = m_sinkAddresses[sink_idx];
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [生产者 " << GetNode()->GetId() << "]: 开始发送任务 " << m_totalTasksSent << " 到 " << InetSocketAddress::ConvertFrom(m_currentTarget).GetIpv4());

    // 触发 Trace (nodeId, taskId (totalSent), targetAddress)
    m_taskSentTrace(GetNode()->GetId(), m_totalTasksSent, m_currentTarget);

    m_packetsSentForCurrentTask = 0;
    SendPacket();
}

void
MyProducer::SendPacket()
{
    if (!m_running)
    {
        m_isSending = false;
        return;
    }
    if (m_packetsSentForCurrentTask * m_packetSize >= m_taskSize)
    {
        SendNextTask();
        return;
    }
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->SendTo(packet, 0, m_currentTarget);
    m_packetsSentForCurrentTask++;
    if (m_isSending)
    {
        Simulator::ScheduleNow(&MyProducer::SendPacket, this);
    }
}

} // namespace ns3
