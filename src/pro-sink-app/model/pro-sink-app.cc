#include "pro-sink-app.h"
#include "price-aware-scheduler.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/buffer.h" // 包含 Buffer
#include "ns3/tcp-socket-factory.h" // 包含 TCP
#include <cmath>
#include <iomanip>

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
        .AddTraceSource("Utilization",
                         "Trace triggered periodically with the compute utilization (0.0 to 1.0).",
                         MakeTraceSourceAccessor(&MySink::m_utilizationTrace),
                         "ns3::TracedCallback<uint32_t, double>") // <NodeId, Utilization>
        .AddTraceSource("QueueLength",
                         "Trace triggered periodically with the queue length (number of pending tasks).",
                         MakeTraceSourceAccessor(&MySink::m_queueLengthTrace),
                         "ns3::TracedCallback<uint32_t, uint32_t>") // <NodeId, QueueLength>
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
      m_currentRxBytesPerTask(),
      m_initialized(false),
      m_nodeId(0),
      m_powerCostXmlPath(),
      m_simulationStep(MilliSeconds(1)),
      m_tasksCompleted(0),
      m_tasksPerSecond(1000.0),
      m_processingCredit(0.0),
      m_running(false),
      m_updateInterval(Seconds(0.25)), // 默认更新间隔为 0.25 秒
      m_totalTimeInInterval(Time(0)),
      m_idleTimeInInterval(Time(0)),
      m_powerCouplingEnabled(false),
      m_warmupTime(0.0),
      m_basePower(0.0),
      m_fullPower(0.0),
      m_pricePhaseOffsetHours(0.0),
      m_totalAccumulatedCost(0.0)
{
}

MySink::~MySink()
{
    m_listenSocket = nullptr;
    m_socketBuffers.clear();
    m_acceptedSockets.clear();
    
    if (m_powerCouplingEnabled && m_powerCostXmlFile.is_open())
    {
        m_powerCostXmlFile << "</PowerCostStats>" << std::endl;
        m_powerCostXmlFile.close();
    }
}

void
MySink::Setup(uint32_t nodeId, double tasksPerSecond, Time simulationStep, Time updateInterval)
{
    m_nodeId = nodeId; // 保存nodeId，但不要立即使用GetNode()
    m_tasksPerSecond = tasksPerSecond;
    m_simulationStep = simulationStep;
    m_updateInterval = updateInterval;
    m_powerCouplingEnabled = false; // 显式关闭
}

void
MySink::Setup(uint32_t nodeId,
              double tasksPerSecond,
              Time simulationStep,
              Time updateInterval,
              double warmupTime,
              double basePower,
              double fullPower,
              double phaseOffset,
              const std::vector<double>& priceProfile,
              const std::string& powerCostXmlPath)
{
    m_nodeId = nodeId; // 保存nodeId，但不要立即使用GetNode()

    // 1. Set basic params
    m_tasksPerSecond = tasksPerSecond;
    m_simulationStep = simulationStep;
    m_updateInterval = updateInterval;

    // 2. Set power params
    m_warmupTime = warmupTime;
    m_basePower = basePower;
    m_fullPower = fullPower;
    m_pricePhaseOffsetHours = phaseOffset;
    m_priceProfile = priceProfile;
    m_powerCouplingEnabled = true; // 显式开启

    // 3. Open XML file (延迟到StartApplication中，因为此时还没有GetNode())
    // 注意：我们现在不打开文件，而是在StartApplication中打开
    m_powerCostXmlPath = powerCostXmlPath;
}

void
MySink::StartApplication()
{
    // 标记为已初始化，现在可以安全使用GetNode()
    m_initialized = true;

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

    // 现在可以安全地打开XML文件和使用GetNode()
    if (m_powerCouplingEnabled)
    {
        // 打开XML文件
        m_powerCostXmlFile.open(m_powerCostXmlPath.c_str());
        if (m_powerCostXmlFile.is_open())
        {
            m_powerCostXmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
            m_powerCostXmlFile << "<PowerCostStats nodeId=\"" << m_nodeId
                               << "\" basePowerMW=\"" << m_basePower
                               << "\" fullPowerMW=\"" << m_fullPower
                               << "\" phaseOffsetHours=\"" << m_pricePhaseOffsetHours
                               << "\" updateIntervalS=\"" << m_updateInterval.GetSeconds()
                               << "\">" << std::endl;
            m_powerCostXmlFile << std::fixed << std::setprecision(6);
        }
        else
        {
            NS_LOG_ERROR("MySink (Node " << m_nodeId << "): Failed to open power cost XML file: " << m_powerCostXmlPath);
            m_powerCouplingEnabled = false; // Open failed, disable
        }

        NS_LOG_INFO("MySink (Node " << m_nodeId << "): Power coupling ENABLED.");
        if (m_priceProfile.empty())
        {
             NS_FATAL_ERROR("MySink (Node " << m_nodeId << "): Power coupling enabled but price profile is empty. Check main setup.");
        }
    }
    else
    {
        NS_LOG_INFO("MySink (Node " << m_nodeId << "): Power coupling DISABLED.");
    }

    m_running = true;
    Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
    Simulator::Schedule(m_updateInterval, &MySink::ReportUtilization, this);
}

void
MySink::StopApplication()
{
    m_running = false;

    // 关闭监听套字
    if (m_listenSocket != nullptr)
    {
        m_listenSocket->SetAcceptCallback(Callback<bool, Ptr<Socket>, const Address&>(),
                                          Callback<void, Ptr<Socket>, const Address&>());
        m_listenSocket->Close();
    }

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId
                  << "]: 停止处理新任务。总共处理任务数: " << m_tasksCompleted
                  << ". 队列中剩余任务数: " << m_taskQueue.size());

    if (m_powerCouplingEnabled)
    {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId
                      << "]: 累积总成本: $" << std::fixed << std::setprecision(2) << m_totalAccumulatedCost);

        if (m_powerCostXmlFile.is_open())
        {
            m_powerCostXmlFile << "</PowerCostStats>" << std::endl;
            m_powerCostXmlFile.close();
        }
    }
}

bool
MySink::HandleAccept(Ptr<Socket> socket, const Address& from)
{
    //NS_LOG_INFO("Node " << GetNode()->GetId() << " accepting connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    return true;
}

void
MySink::HandleNewConnection(Ptr<Socket> socket, const Address& from)
{
    //NS_LOG_INFO("Node " << GetNode()->GetId() << " handling new connection from " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    
    m_acceptedSockets.push_back(socket);
    m_socketBuffers[socket] = Buffer(); // 为新连接创建缓冲区

    socket->SetRecvCallback(MakeCallback(&MySink::HandleRead, this));
    socket->SetCloseCallbacks(MakeCallback(&MySink::HandleNormalClose, this),
                              MakeCallback(&MySink::HandleErrorClose, this));
}

void 
MySink::HandleNormalClose(Ptr<Socket> socket)
{
    //NS_LOG_INFO("Node " << GetNode()->GetId() << ": Peer closed connection normally.");
    CleanupConnection(socket);
}

void 
MySink::HandleErrorClose(Ptr<Socket> socket)
{
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "Node " << GetNode()->GetId() << ": Connection closed with error.");
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
    while (socket->GetRxAvailable() > 0)
    {
        Ptr<Packet> packet = socket->Recv(socket->GetRxAvailable(), 0);
        uint32_t packetSize = packet->GetSize();
        if (packetSize == 0)
        {
            break; // 实际上 GetRxAvailable > 0 保证了 packet size > 0
        }

        uint8_t* tempBuffer = new uint8_t[packetSize];
        packet->CopyData(tempBuffer, packetSize);

        Buffer& appBuffer = m_socketBuffers[socket];
        appBuffer.AddAtEnd(packetSize); // 预留空间
        Buffer::Iterator it = appBuffer.End();
        it.Prev(packetSize); // 移动到预留空间的开头
        it.Write(tempBuffer, packetSize); // 写入数据

        delete[] tempBuffer;
    }
    
    // 处理缓冲区中的字节流
    ProcessSocketBuffer(socket);
}

void
MySink::ProcessSocketBuffer(Ptr<Socket> socket)
{
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

        // 2. 从缓冲区移除包头
        buffer.RemoveAtStart(headerSize);

        // 3. 识别任务 (我们知道 payload 大小，所以只需累加)
        uint32_t producerId = header.GetProducerId();
        uint32_t taskId = header.GetTaskId();
        std::pair<uint32_t, uint32_t> taskKey = {producerId, taskId};

        m_currentRxBytesPerTask[taskKey] += payloadSize;

        // 4. 从缓冲区移除 Payload
        buffer.RemoveAtStart(payloadSize);

        // 5. 检查任务是否完整接收
        if (m_currentRxBytesPerTask[taskKey] >= m_taskSize)
        {
            m_taskQueue.push(taskKey);
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId << "]: 任务 "
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

     // --- 1. 算力利用率跟踪 (高精度轮询) ---
     // 累加总时间 (以 simulationStep 为单位)
     m_totalTimeInInterval += m_simulationStep;

     // 检查队列状态
     if (m_taskQueue.empty())
     {
         // 队列为空：累加空闲时间，不累积处理信用点
         m_idleTimeInInterval += m_simulationStep;

         //在队列为空时清除任何剩余的 < 1.0 的信用点：
         m_processingCredit = 0.0;
     }
     else
     {
         // 队列非空：不累加空闲时间，但累积处理信用点
         m_processingCredit += m_tasksPerSecond * m_simulationStep.GetSeconds();
     }

     // --- 2. 现有任务处理逻辑 ---
    uint32_t tasksToProcess = floor(m_processingCredit);
    for (uint32_t i = 0; i < tasksToProcess && !m_taskQueue.empty(); ++i)
    {
        std::pair<uint32_t, uint32_t> taskKey = m_taskQueue.front();
        m_taskQueue.pop();
        m_tasksCompleted++;
        m_processingCredit -= 1.0;

        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId << "]: 任务 "
                      << taskKey.first << "-" << taskKey.second << " 处理完成，队列共有"
                      << m_taskQueue.size() << "个任务等待处理。消费者 "
                      << m_nodeId << " 处理总数 " << m_tasksCompleted << ".");

        m_taskCompletedTrace(m_nodeId, taskKey.first, taskKey.second, m_tasksCompleted);
    }
    if (m_running)
    {
        Simulator::Schedule(m_simulationStep, &MySink::ProcessTasks, this);
    }
}

uint32_t
MySink::GetCurrentPriceIndex() const
{
    // 电价曲线覆盖 24 小时
    const double priceProfileDuration = 24.0; // 小时
    const double numPricePoints = m_priceProfile.size(); 
    if (numPricePoints == 0)
    {
        NS_LOG_WARN("Price profile is empty, returning index 0.");
        return 0;
    }

    double nowSeconds = Simulator::Now().GetSeconds();
    
    // 在预热期间，我们可能使用默认电价（或索引0）
    if (nowSeconds < m_warmupTime)
    {
        return 0; 
    }

    // 计算实际仿真时间（小时）
    double realTimeHours = (nowSeconds - m_warmupTime);

    // 应用相位偏移并确保时间在 [0, 24) 范围内
    double effectiveTimeHours = std::fmod(realTimeHours + m_pricePhaseOffsetHours, 
                                          priceProfileDuration);
    
    // 确保 fmod 结果为正
    if (effectiveTimeHours < 0)
    {
        effectiveTimeHours += priceProfileDuration;
    }

    // 计算每个电价点代表的时间（小时）
    double priceStepHours = priceProfileDuration / numPricePoints;
    
    // 计算浮点索引
    // (添加一个很小的值以处理浮点精度问题)
    double floatingIndex = (effectiveTimeHours + 1e-9) / priceStepHours;

    // 向下取整得到离散索引
    uint32_t index = static_cast<uint32_t>(std::floor(floatingIndex));
    
    // 确保索引在有效范围内
    return std::min(index, static_cast<uint32_t>(numPricePoints - 1));
}

void
MySink::ReportUtilization()
{
    if (!m_running)
    {
        return;
    }

    // 1. & 2. 计算利用率
    Time busyTime = m_totalTimeInInterval - m_idleTimeInInterval;
    double utilization = 0.0;
    if (m_totalTimeInInterval > Time(0))
    {
        // 利用率 = 繁忙时间 / 总时间
        utilization = busyTime.GetSeconds() / m_totalTimeInInterval.GetSeconds();
    }

    // 3. 触发 Utilization Trace
    m_utilizationTrace(m_nodeId, utilization);

    // 3.1 触发队列长度 Trace
    m_queueLengthTrace(m_nodeId, m_taskQueue.size());

    // --- 4. (Guarded) 计算功率、电价和成本 ---
    if (m_powerCouplingEnabled)
    {
        // 功率 (MW) = 基础功率 + (满载功率 - 基础功率) * 利用率
        double avgPower_MW = m_basePower + utilization * (m_fullPower - m_basePower);

        // 获取当前电价
        uint32_t priceIndex = GetCurrentPriceIndex();
        double currentPrice_per_MWh = m_priceProfile[priceIndex];

        // 计算此间隔的成本
        double intervalHours = m_updateInterval.GetSeconds() / 3600.0;
        double intervalEnergy_MWh = avgPower_MW * intervalHours;
        double intervalCost = intervalEnergy_MWh * currentPrice_per_MWh;

        // 累积总成本
        m_totalAccumulatedCost += intervalCost;

        // 5. 打印控制台日志 (扩展)
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId
                      << "]: Util: " << std::fixed << std::setprecision(2) << utilization * 100.0 << "%"
                      << " | AvgPower: " << std::setprecision(3) << avgPower_MW << " MW"
                      << " | Price: $" << std::setprecision(2) << currentPrice_per_MWh << "/MWh"
                      << " | IntervalCost: $" << std::setprecision(4) << intervalCost
                      << " | TotalCost: $" << std::setprecision(2) << m_totalAccumulatedCost);

        // 6. 写入 XML
        if (m_powerCostXmlFile.is_open())
        {
            m_powerCostXmlFile << "  <Sample time=\"" << Simulator::Now().GetSeconds()
                               << "\" util=\"" << utilization
                               << "\" power_MW=\"" << avgPower_MW
                               << "\" price_per_MWh=\"" << currentPrice_per_MWh
                               << "\" interval_cost=\"" << intervalCost
                               << "\" total_cost=\"" << m_totalAccumulatedCost
                               << "\"/>" << std::endl;
        }
    }
    else
    {
        NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [消费者 " << m_nodeId << "]: 算力利用率 (过去 "
                      << m_updateInterval.GetSeconds() << "s): " << std::fixed << std::setprecision(2) << utilization * 100.0 << "%");
    }

    // 7. 重置累加器
    m_totalTimeInInterval = Time(0);
    m_idleTimeInInterval = Time(0);

    // 8. 调度下一次报告
    Simulator::Schedule(m_updateInterval, &MySink::ReportUtilization, this);
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
      m_taskQueue(),
      m_running(false),
      m_connected(false),
      m_enablePriceAwareScheduling(false),
      m_scheduler(nullptr)
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
MyProducer::EnablePriceAwareScheduling(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_enablePriceAwareScheduling = enable;
    NS_LOG_INFO("Price-aware scheduling " << (enable ? "enabled" : "disabled") << " for producer on node " << GetNode()->GetId());
}

void
MyProducer::SetScheduler(Ptr<PriceAwareScheduler> scheduler)
{
    NS_LOG_FUNCTION(this << scheduler);
    m_scheduler = scheduler;
    NS_LOG_INFO("Scheduler set for producer on node " << GetNode()->GetId());
}

void
MyProducer::StartApplication()
{
    // 使用 TcpSocketFactory
    // 注意：Pacing 必须在运行此文件的脚本中通过 Config::SetDefault 启用
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    
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
    m_running = false; // 1. 信号：停止生成新任务和接受新任务

    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: [生产者 " << GetNode()->GetId() << "]: 停止生产新任务, 总共发送任务数: " << m_totalTasksSent 
                  << ". 队列中剩余任务数: " << m_taskQueue.size());
    // 2. 检查是否可以立即停止
    if (!m_isSending)
    {
        // 当前没有任务在发送，可以安全关闭
        //NS_LOG_UNCOND("生产者 " << GetNode()->GetId() << ": 立即停止 (未在发送)。");
        if (m_socket != nullptr && m_connected) // 检查 m_connected 避免在未连接时关闭
        {
            m_socket->Close();
            m_connected = false; 
        }
    }
    // 3. 如果 m_isSending == true，则什么也不做。
    //    SendPacket() 将在完成发送后处理关闭。
}

// --- TCP 回调实现 ---

void 
MyProducer::ConnectionSucceeded(Ptr<Socket> socket)
{
    //NS_LOG_INFO("Node " << GetNode()->GetId() << " connected to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4());
    m_connected = true; 
    SendNextTask(); // 尝试发送队列中的任务
}

void 
MyProducer::ConnectionFailed(Ptr<Socket> socket)
{
    NS_LOG_WARN(Simulator::Now().GetSeconds() << "Node " << GetNode()->GetId() << " failed to connect to " << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4());
    m_connected = false;
}

void 
MyProducer::NormalClose(Ptr<Socket> socket)
{
    //NS_LOG_INFO(Simulator::Now().GetSeconds() << "Node " << GetNode()->GetId() << ": Connection closed normally.");
    m_connected = false;
    m_isSending = false;
}

void 
MyProducer::ErrorClose(Ptr<Socket> socket)
{
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "Node " << GetNode()->GetId() << ": Connection closed with error.");
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
    if (!m_running || m_isSending || m_taskQueue.empty())
    {
        return;
    }

    // 如果没有连接，先连接
    if (!m_connected)
    {
        // 动态选择最优目标 (价格感知调度)
        if (m_enablePriceAwareScheduling && m_scheduler)
        {
            double nowSeconds = Simulator::Now().GetSeconds();
            Address newAddress = m_scheduler->ScheduleNextTask(nowSeconds);

            if (newAddress != m_peerAddress)
            {
                NS_LOG_INFO("Switching to new target: " << InetSocketAddress::ConvertFrom(newAddress).GetIpv4());
                m_peerAddress = newAddress;
                if (m_socket)
                {
                    m_socket->Close();
                    m_socket = nullptr;
                }
            }
        }

        // 重新连接
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);

        m_socket->SetConnectCallback(MakeCallback(&MyProducer::ConnectionSucceeded, this),
                                     MakeCallback(&MyProducer::ConnectionFailed, this));
        m_socket->SetSendCallback(MakeCallback(&MyProducer::HandleSend, this));
        m_socket->SetCloseCallbacks(MakeCallback(&MyProducer::NormalClose, this),
                                    MakeCallback(&MyProducer::ErrorClose, this));

        m_socket->Connect(m_peerAddress);
        return; // 等待连接成功
    }

    // 如果已连接，继续发送
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
    // 软停止
    if (!m_connected || !m_isSending)
    {
        m_isSending = false; // 确保状态一致
        return;
    }

    // 循环，直到任务发送完毕或 TCP 缓冲区已满
    while (m_totalBytesSentForCurrentTask < m_taskSize)
    {
        // 检查 TCP 发送缓冲区是否有空间
        if (m_socket->GetTxAvailable() < m_packetSize + TaskHeader().GetSerializedSize())
        {
            // 缓冲区已满（或 Pacing 限制），等待 HandleSend 回调
            return;
        }
        
        TaskHeader header;
        header.SetData(m_currentSendingProducerId, m_currentSendingTaskId);

        // (与之前相同)
        Ptr<Packet> packet = Create<Packet>(m_packetSize); // 只创建 payload
        packet->AddHeader(header); // 添加 header
        
        int bytesSent = m_socket->Send(packet);

        if (bytesSent < 0)
        {
            // (与之前相同)
            NS_LOG_WARN(Simulator::Now().GetSeconds() << "TCP Send failed with error.");
            return;
        }
        
        m_totalBytesSentForCurrentTask += m_packetSize;
    }

    // 任务完成
    //NS_LOG_INFO("Node " << GetNode()->GetId() << " finished sending task " << m_currentSendingProducerId << "-" << m_currentSendingTaskId);
    m_isSending = false;
    
    // 检查是否已请求停止 (m_running == false)
    if (!m_running)
    {
        // 软停止：任务已发送完毕，并且 StopApplication 已被调用。
        // 现在可以安全关闭套接字。
        /*
                NS_LOG_UNCOND("生产者 " << GetNode()->GetId() << ": 软停止 - 已发完最后一个任务 " 
                      << m_currentSendingProducerId << "-" << m_currentSendingTaskId
                      << "，现在关闭套接字。");
        */

        if (m_socket != nullptr && m_connected)
        {
            m_socket->Close();
            m_connected = false;
        }
        // 不再安排 SendNextTask
    }
    else
    {
        // 正常操作：尝试发送队列中的下一个任务
        Simulator::ScheduleNow(&MyProducer::SendNextTask, this);
    }
}

} // namespace ns3