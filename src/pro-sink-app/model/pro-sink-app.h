#ifndef PRO_SINK_APP_H
#define PRO_SINK_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "ns3/header.h" // 包含 Header
#include "ns3/buffer.h" // 包含 Buffer

#include <queue>
#include <vector>
#include <map>     // 包含 map
#include <list>    // 包含 list
#include <utility> // 包含 pair

namespace ns3 {

// --- 0. 自定义包头 (TaskHeader) 声明 (与之前相同) ---
class TaskHeader : public Header
{
public:
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(Buffer::Iterator start) const;
    virtual uint32_t Deserialize(Buffer::Iterator start);
    virtual void Print(std::ostream &os) const;

    TaskHeader();
    ~TaskHeader();

    void SetData(uint32_t producerId, uint32_t taskId);
    uint32_t GetProducerId(void) const;
    uint32_t GetTaskId(void) const;

private:
    uint32_t m_producerId;
    uint32_t m_taskId;
};


// --- 1. 自定义消费者应用 (MySink) 声明 (TCP 版本) ---
class MySink : public Application
{
public:
    static TypeId GetTypeId(void);
    MySink();
    virtual ~MySink();

    void Setup(double tasksPerSecond, Time simulationStep);

    TracedCallback<uint32_t, uint32_t, uint32_t, uint32_t> m_taskCompletedTrace;

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // --- TCP 监听和连接管理 ---
    bool HandleAccept(Ptr<Socket> socket, const Address& from);
    void HandleNewConnection(Ptr<Socket> socket, const Address& from);
    void CleanupConnection(Ptr<Socket> socket);
    
    // --- TCP 回调 (严格遵守教训 1, 2) ---
    void HandleRead(Ptr<Socket> socket);
    void HandleNormalClose(Ptr<Socket> socket);
    void HandleErrorClose(Ptr<Socket> socket);
    
    // --- TCP 流处理 (严格遵守教训 3, 5) ---
    void ProcessSocketBuffer(Ptr<Socket> socket);

    // --- 任务处理 (同之前) ---
    void ProcessTasks();

    Ptr<Socket> m_listenSocket; // TCP 监听套接字
    std::list<Ptr<Socket>> m_acceptedSockets; // 跟踪所有活跃连接
    
    // 严格遵守教训 3, 5：为每个连接维护一个应用层缓冲区
    std::map<Ptr<Socket>, Buffer> m_socketBuffers;

    uint16_t    m_port;
    uint32_t    m_taskSize;
    uint32_t    m_packetSize; // 用于解析流

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> m_currentRxBytesPerTask;

    Time        m_simulationStep;
    uint32_t    m_tasksCompleted;
    std::queue<std::pair<uint32_t, uint32_t>> m_taskQueue;

    double      m_tasksPerSecond;
    double      m_processingCredit;
    bool        m_running;
};


// --- 2. 自定义生产者应用 (MyProducer) 声明 (TCP 版本) ---
class MyProducer : public Application
{
public:
    static TypeId GetTypeId(void);
    MyProducer();
    virtual ~MyProducer();

    // Setup 已修改：现在只接受一个目标地址
    void Setup(Address sinkAddress, double lambda, uint32_t taskSize, uint32_t packetSize, Time simulationStep);

    TracedCallback<uint32_t, uint32_t, Address> m_taskSentTrace;

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // --- TCP 回调 (严格遵守教训 1, 2) ---
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void HandleSend(Ptr<Socket> socket, uint32_t txSpace); // 流量控制
    void NormalClose(Ptr<Socket> socket);
    void ErrorClose(Ptr<Socket> socket);

    // --- 发送和生成逻辑 (已修改) ---
    void SendPacket();
    void SendNextTask();
    void GenerateTasks();

    Ptr<Socket> m_socket;
    Address     m_peerAddress; // 单个对端
    
    uint32_t    m_taskSize;
    uint32_t    m_packetSize;
    uint64_t    m_totalBytesSentForCurrentTask; // 使用字节数跟踪 TCP 流
    uint32_t    m_totalTasksSent;
    bool        m_isSending;

    uint32_t    m_currentSendingProducerId;
    uint32_t    m_currentSendingTaskId;

    Time m_simulationStep;
    double m_lambda;
    Ptr<ExponentialRandomVariable> m_interTaskTimeGenerator;
    std::queue<bool> m_taskQueue;
    
    bool m_running;
    bool m_connected; // 严格遵守教训 4：连接状态标志
};

} // namespace ns3

#endif // PRO_SINK_APP_H
