#include "pro-sink-app.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/buffer.h" // 包含 Buffer
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ProSinkApp"); 

// --- 0. TaskHeader 实现 ---

TypeId TaskHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::TaskHeader")
        .SetParent<Header>()
        .SetGroupName("Applications")
        .AddConstructor<TaskHeader>()
    ;
    return tid;
}

TypeId TaskHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

uint32_t TaskHeader::GetSerializedSize(void) const
{
    // producerId (4 bytes) + taskId (4 bytes)
    return sizeof(uint32_t) * 2;
}

void TaskHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteHtonU32(m_producerId);
    start.WriteHtonU32(m_taskId);
}

uint32_t TaskHeader::Deserialize(Buffer::Iterator start)
{
    m_producerId = start.ReadNtohU32();
    m_taskId = start.ReadNtohU32();
    return GetSerializedSize();
}

void TaskHeader::Print(std::ostream &os) const
{
    os << "ProducerId=" << m_producerId << " TaskId=" << m_taskId;
}

TaskHeader::TaskHeader()
    : m_producerId(0), m_taskId(0)
{
}

TaskHeader::~TaskHeader()
{
}

void TaskHeader::SetData(uint32_t producerId, uint32_t taskId)
{
    m_producerId = producerId;
    m_taskId = taskId;
}

uint32_t TaskHeader::GetProducerId(void) const
{
    return m_producerId;
}

uint32_t TaskHeader::GetTaskId(void) const
{
    return m_taskId;
}


// --- 1. MySink 实现 ---

TypeId MySink::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MySink")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<MySink>()
        // 修改 TraceSource 签名
        .AddTraceSource("TaskCompleted",
                        "Trace triggered when a task is completed.",
                        MakeTraceSourceAccessor(&MySink::m_taskCompletedTrace),
                        "ns3::TracedCallback<uint32_t, uint32_t, uint32_t, uint32_t>")
    ;
    return tid;
}

MySink::MySink()
    : m_socket(nullptr),
      m_port(8080),
      m_taskSize(256 * 1024),
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
        // 1. 读取包头
        TaskHeader header;
        if (packet->RemoveHeader(header) == 0)
        {
            NS_LOG_WARN("收到了没有 TaskHeader 的包，丢弃。");
            continue;
        }

        uint32_t producerId = header.GetProducerId();
        uint32_t taskId = header.GetTaskId();
        std::pair<uint32_t, uint32_t> taskKey = {producerId, taskId};

        // 2. 累加字节数
        m_currentRxBytesPerTask[taskKey] += packet->GetSize();

        // 3. 检查任务是否完整接收
        if (m_currentRxBytesPerTask[taskKey] >= m_taskSize)
        {
            m_taskQueue.push(taskKey);
            // 打印入列日志
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " 
                          << taskKey.first << "-" << taskKey.second << " 入列，队列共有" 
                          << m_taskQueue.size() << "个任务等待处理。");

            // 清理map
            m_currentRxBytesPerTask.erase(taskKey);
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
        std::pair<uint32_t, uint32_t> taskKey = m_taskQueue.front();
        m_taskQueue.pop();
        m_tasksCompleted++;
        m_processingCredit -= 1.0;
        
        // 打印处理完成日志
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " 
                      << taskKey.first << "-" << taskKey.second << " 处理完成，队列共有" 
                      << m_taskQueue.size() << "个任务等待处理。消费者 " 
                      << GetNode()->GetId() << " 处理总数 " << m_tasksCompleted << "。");
        
        // 触发 Trace (nodeId, producerId, taskId, totalCompleted)
        m_taskCompletedTrace(GetNode()->GetId(), taskKey.first, taskKey.second, m_tasksCompleted);
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
      m_currentSendingProducerId(0),
      m_currentSendingTaskId(0),
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

    // 存储当前任务信息，用于添加到包头
    m_currentSendingProducerId = GetNode()->GetId();
    m_currentSendingTaskId = m_totalTasksSent;

    // 修改日志格式
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [生产者 " << GetNode()->GetId() << "]: 开始发送任务 " 
                  << m_currentSendingProducerId << "-" << m_currentSendingTaskId 
                  << " 到 " << InetSocketAddress::ConvertFrom(m_currentTarget).GetIpv4());

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
    
    // 1. 创建包头
    TaskHeader header;
    header.SetData(m_currentSendingProducerId, m_currentSendingTaskId);

    // 2. 创建包并添加包头
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    packet->AddHeader(header);
    
    // 3. 发送
    m_socket->SendTo(packet, 0, m_currentTarget);
    m_packetsSentForCurrentTask++;
    if (m_isSending)
    {
        Simulator::ScheduleNow(&MyProducer::SendPacket, this);
    }
}

} // namespace ns3