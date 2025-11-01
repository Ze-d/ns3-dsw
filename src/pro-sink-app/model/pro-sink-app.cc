#include "pro-sink-app.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/buffer.h" // 包含 Buffer
#include "ns3/tcp-socket-factory.h" // 包含 TCP
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ProSinkApp"); 

// --- 0. TaskHeader 实现 (与之前相同) ---

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


// --- 1. MySink 实现 (TCP 版本) ---

TypeId MySink::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::MySink")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<MySink>()
        .AddTraceSource("TaskCompleted",
                        "Trace triggered when a task is completed.",
                        MakeTraceSourceAccessor(&MySink::m_taskCompletedTrace),
                        "ns3::TracedCallback<uint32_t, uint32_t, uint32_t, uint32_t>")
        .AddAttribute("Port",
                      "Port on which to listen for connections.",
                      UintegerValue(8080),
                      MakeUintegerAccessor(&MySink::m_port),
                      MakeUintegerChecker<uint16_t>())
        .AddAttribute("TaskSize",
                      "The size of a task in bytes.",
                      UintegerValue(256 * 1024),
                      MakeUintegerAccessor(&MySink::m_taskSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("PacketSize",
                      "The size of payload packets (excluding header).",
                      UintegerValue(1024),
                      MakeUintegerAccessor(&MySink::m_packetSize),
                      MakeUintegerChecker<uint32_t>());
    return tid;
}

MySink::MySink()
    : m_listenSocket(nullptr),
      m_port(8080),
      m_taskSize(256 * 1024),
      m_packetSize(1024), // 必须与 Producer 匹配
      m_simulationStep(MilliSeconds(1)),
      m_tasksCompleted(0),
      m_tasksPerSecond(1000.0),
      m_processingCredit(0.0),
      m_running(false)
{
}

MySink::~MySink()
{
    m_listenSocket = nullptr;
    m_socketBuffers.clear();
    m_acceptedSockets.clear();
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
    if (m_listenSocket == nullptr)
    {
        // 使用 TcpSocketFactory
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_listenSocket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_listenSocket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        if (m_listenSocket->Listen() == -1)
        {
            NS_FATAL_ERROR("Failed to listen on socket");
        }
    }
    
    // 设置接受连接的回调
    m_listenSocket->SetAcceptCallback(MakeCallback(&MySink::HandleAccept, this),
                                    MakeCallback(&MySink::HandleNewConnection, this));
    
    m_running = true;
    Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
}

void
MySink::StopApplication()
{
    m_running = false;
    
    // 关闭所有接受的连接
    for (auto const& socket : m_acceptedSockets)
    {
        if (socket != nullptr)
        {
            socket->Close();
        }
    }
    m_acceptedSockets.clear();
    m_socketBuffers.clear();

    // 关闭监听套接字
    if (m_listenSocket != nullptr)
    {
        m_listenSocket->SetAcceptCallback(Callback<bool, Ptr<Socket>, const Address&>(),
                                          Callback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->Close();
    }
    NS_LOG_UNCOND("消费者应用停止。节点 " << GetNode()->GetId() << " 总共处理任务数: " << m_tasksCompleted << ". 队列中剩余任务数: " << m_taskQueue.size());
}

bool
MySink::HandleAccept(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << " accepting connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    return true;
}

void
MySink::HandleNewConnection(Ptr<Socket> socket, const Address& from)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << " handling new connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    
    m_acceptedSockets.push_back(socket);
    m_socketBuffers[socket] = Buffer(); // 严格遵守教训 3 和 5：为新连接创建缓冲区

    // 严格遵守教训 1 和 2：使用正确签名的回调
    socket->SetRecvCallback(MakeCallback(&MySink::HandleRead, this));
    socket->SetCloseCallbacks(MakeCallback(&MySink::HandleNormalClose, this),
                              MakeCallback(&MySink::HandleErrorClose, this));
}

void 
MySink::HandleNormalClose(Ptr<Socket> socket)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Peer closed connection normally.");
    CleanupConnection(socket);
}

void 
MySink::HandleErrorClose(Ptr<Socket> socket)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Connection closed with error.");
    CleanupConnection(socket);
}

void 
MySink::CleanupConnection(Ptr<Socket> socket)
{
    m_acceptedSockets.remove(socket);
    m_socketBuffers.erase(socket);
}


void
MySink::HandleRead(Ptr<Socket> socket)
{
    // 严格遵守教训 3：从套接字读取所有可用数据到应用缓冲区
    while (socket->GetRxAvailable() > 0)
    {
        Ptr<Packet> packet = socket->Recv(socket->GetRxAvailable(), 0);
        uint32_t packetSize = packet->GetSize();
        if (packetSize == 0)
        {
            break; // 实际上 GetRxAvailable > 0 保证了 packet size > 0
        }

        // --- BEGIN FIX ---
        // 严格遵守教训 5：
        // 1. Packet::CopyData 到一个临时 C-style 缓冲区
        uint8_t* tempBuffer = new uint8_t[packetSize];
        packet->CopyData(tempBuffer, packetSize);

        // 2. Buffer::AddAtEnd(size) + Iterator::Write
        Buffer& appBuffer = m_socketBuffers[socket];
        appBuffer.AddAtEnd(packetSize); // 预留空间
        Buffer::Iterator it = appBuffer.End();
        it.Prev(packetSize); // 移动到预留空间的开头
        it.Write(tempBuffer, packetSize); // 写入数据

        delete[] tempBuffer;
        // --- END FIX ---
    }
    
    // 处理我们缓冲区中的字节流
    ProcessSocketBuffer(socket);
}

void
MySink::ProcessSocketBuffer(Ptr<Socket> socket)
{
    // 严格遵守教训 5：正确处理 Buffer API
    Buffer& buffer = m_socketBuffers[socket];

    uint32_t headerSize = TaskHeader().GetSerializedSize();
    uint32_t payloadSize = m_packetSize; // 假设 Producer 发送固定大小的 payload
    uint32_t fullPacketSize = headerSize + payloadSize;

    // 循环处理缓冲区，直到数据不足以解析一个完整的 (Header + Payload)
    while (buffer.GetSize() >= fullPacketSize)
    {
        // 1. 读取包头 (不从缓冲区移除)
        Buffer::Iterator it = buffer.Begin();
        TaskHeader header;
        header.Deserialize(it);

        // 2. 从缓冲区移除包头 (严格遵守教训 5)
        buffer.RemoveAtStart(headerSize);

        // 3. 识别任务 (我们知道 payload 大小，所以只需累加)
        uint32_t producerId = header.GetProducerId();
        uint32_t taskId = header.GetTaskId();
        std::pair<uint32_t, uint32_t> taskKey = {producerId, taskId};

        m_currentRxBytesPerTask[taskKey] += payloadSize;

        // 4. 从缓冲区移除 Payload (严格遵守教训 5)
        buffer.RemoveAtStart(payloadSize);

        // 5. 检查任务是否完整接收 (与 UDP 逻辑相同)
        if (m_currentRxBytesPerTask[taskKey] >= m_taskSize)
        {
            m_taskQueue.push(taskKey);
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " 
                          << taskKey.first << "-" << taskKey.second << " 完整接收并入列，队列共有" 
                          << m_taskQueue.size() << "个任务等待处理。");

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
        
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << GetNode()->GetId() << "]: 任务 " 
                      << taskKey.first << "-" << taskKey.second << " 处理完成，队列共有" 
                      << m_taskQueue.size() << "个任务等待处理。消费者 " 
                      << GetNode()->GetId() << " 处理总数 " << m_tasksCompleted << "。");
        
        m_taskCompletedTrace(GetNode()->GetId(), taskKey.first, taskKey.second, m_tasksCompleted);
    }
    if (m_running)
    {
        Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
    }
}

// --- 2. MyProducer 实现 (TCP 版本) ---

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
        .AddAttribute("TaskSize",
                      "The size of a task in bytes.",
                      UintegerValue(256 * 1024),
                      MakeUintegerAccessor(&MyProducer::m_taskSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("PacketSize",
                      "The size of payload packets (excluding header).",
                      UintegerValue(1024),
                      MakeUintegerAccessor(&MyProducer::m_packetSize),
                      MakeUintegerChecker<uint32_t>());
    return tid;
}

MyProducer::MyProducer()
    : m_socket(nullptr),
      m_taskSize(256 * 1024),
      m_packetSize(1024),
      m_totalBytesSentForCurrentTask(0),
      m_totalTasksSent(0),
      m_isSending(false),
      m_currentSendingProducerId(0),
      m_currentSendingTaskId(0),
      m_simulationStep(MilliSeconds(1)),
      m_lambda(0.0),
      m_interTaskTimeGenerator(nullptr),
      m_running(false),
      m_connected(false) // 严格遵守教训 4：使用布尔标志管理状态
{
}

MyProducer::~MyProducer()
{
    m_socket = nullptr;
}

void
MyProducer::Setup(Address sinkAddress, double lambda, uint32_t taskSize, uint32_t packetSize, Time simulationStep)
{
    m_peerAddress = sinkAddress; // TCP 连接到单个对端
    m_taskSize = taskSize;
    m_packetSize = packetSize;
    m_simulationStep = simulationStep;
    m_lambda = lambda;
    m_interTaskTimeGenerator = CreateObject<ExponentialRandomVariable>();
    m_interTaskTimeGenerator->SetAttribute("Mean", DoubleValue(1.0 / m_lambda));
}

void
MyProducer::StartApplication()
{
    // 使用 TcpSocketFactory
    // 注意：Pacing 必须在运行此文件的脚本中通过 Config::SetDefault 启用
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    
    // 严格遵守教训 1, 2, 4：设置所有状态管理回调
    m_socket->SetConnectCallback(MakeCallback(&MyProducer::ConnectionSucceeded, this),
                                 MakeCallback(&MyProducer::ConnectionFailed, this));
    m_socket->SetSendCallback(MakeCallback(&MyProducer::HandleSend, this));
    m_socket->SetCloseCallbacks(MakeCallback(&MyProducer::NormalClose, this),
                                MakeCallback(&MyProducer::ErrorClose, this));

    m_running = true;
    m_socket->Connect(m_peerAddress); // 异步连接

    Simulator::Schedule(m_simulationStep, &MyProducer::GenerateTasks, this);
}

void
MyProducer::StopApplication()
{
    m_running = false;
    if (m_socket != nullptr)
    {
        m_socket->Close();
        m_connected = false; // 严格遵守教训 4
    }
    NS_LOG_UNCOND("生产者应用停止。节点 " << GetNode()->GetId() << " 总共发送任务数: " << m_totalTasksSent << ". 队列中剩余任务数: " << m_taskQueue.size());
}

// --- TCP 回调实现 ---

void 
MyProducer::ConnectionSucceeded(Ptr<Socket> socket)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << " connected to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4());
    m_connected = true; // 严格遵守教训 4
    SendNextTask(); // 尝试发送队列中的任务
}

void 
MyProducer::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_WARN("Node " << GetNode()->GetId() << " failed to connect to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4());
    m_connected = false;
}

void 
MyProducer::NormalClose(Ptr<Socket> socket)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Connection closed normally.");
    m_connected = false;
    m_isSending = false;
}

void 
MyProducer::ErrorClose(Ptr<Socket> socket)
{
    NS_LOG_INFO("Node " << GetNode()->GetId() << ": Connection closed with error.");
    m_connected = false;
    m_isSending = false;
}

void 
MyProducer::HandleSend(Ptr<Socket> socket, uint32_t txSpace)
{
    // 当 TCP 发送缓冲区有更多空间时调用此函数
    // 我们现在可以继续发送当前任务
    if (m_isSending)
    {
        SendPacket();
    }
}

// --- 任务生成与发送逻辑 ---

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
            SendNextTask(); // 尝试启动发送
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
    // 必须连接上、不在发送中、且队列不为空
    if (!m_running || m_isSending || !m_connected || m_taskQueue.empty())
    {
        return;
    }
    m_isSending = true;
    m_taskQueue.pop();
    m_totalTasksSent++;

    m_currentSendingProducerId = GetNode()->GetId();
    m_currentSendingTaskId = m_totalTasksSent;
    m_totalBytesSentForCurrentTask = 0; // 重置字节计数器

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [生产者 " << GetNode()->GetId() << "]: 开始发送任务 " 
                  << m_currentSendingProducerId << "-" << m_currentSendingTaskId 
                  << " 到 " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4());

    m_taskSentTrace(GetNode()->GetId(), m_totalTasksSent, m_peerAddress);

    SendPacket(); // 开始发送循环
}

void
MyProducer::SendPacket()
{
    if (!m_running || !m_connected || !m_isSending)
    {
        m_isSending = false; // 确保状态一致
        return;
    }

    // 循环，直到任务发送完毕或 TCP 缓冲区已满
    while (m_totalBytesSentForCurrentTask < m_taskSize)
    {
        // 检查 TCP 发送缓冲区是否有空间
        // 我们需要 (header + payload) 的空间，但 Send() 会为我们处理
        // Pacing 会在这里起作用：GetTxAvailable() 会反映 Pacing 速率允许的量
        if (m_socket->GetTxAvailable() < m_packetSize + TaskHeader().GetSerializedSize())
        {
            // 缓冲区已满（或 Pacing 限制），等待 HandleSend 回调
            return;
        }
        
        TaskHeader header;
        header.SetData(m_currentSendingProducerId, m_currentSendingTaskId);

        // 严格遵守教训 3：使用 Ptr<Packet>
        Ptr<Packet> packet = Create<Packet>(m_packetSize); // 只创建 payload
        packet->AddHeader(header); // 添加 header
        
        int bytesSent = m_socket->Send(packet);

        if (bytesSent < 0)
        {
            // 发送失败，可能是连接断开
            NS_LOG_WARN("TCP Send failed with error.");
            // ErrorClose/NormalClose 回调将处理状态清理
            return;
        }
        
        // 假设 Send() 接受了整个包（对于 TCP 流是正确的）
        // 我们按 payload 大小跟踪任务进度
        m_totalBytesSentForCurrentTask += m_packetSize;
    }

    // 任务完成
    NS_LOG_INFO("Node " << GetNode()->GetId() << " finished sending task " << m_currentSendingProducerId << "-" << m_currentSendingTaskId);
    m_isSending = false;
    
    // 尝试立即发送下一个任务
    Simulator::ScheduleNow(&MyProducer::SendNextTask, this);
}

} // namespace ns3

