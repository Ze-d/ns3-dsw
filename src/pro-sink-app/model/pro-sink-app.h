#ifndef PRO_SINK_APP_H
#define PRO_SINK_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h" // 包含 TracedCallback

#include <queue>
#include <vector>

namespace ns3 {

// --- 1. 自定义消费者应用 (MySink) 声明 ---
class MySink : public Application
{
public:
    static TypeId GetTypeId(void);
    MySink();
    virtual ~MySink();

    void Setup(double tasksPerSecond, Time simulationStep);

    // TracedCallback: nodeId, taskId, totalCompleted
    // 当一个任务处理完成时触发
    TracedCallback<uint32_t, uint32_t, uint32_t> m_taskCompletedTrace;

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void HandleRead(Ptr<Socket> socket);
    void ProcessTasks();

    Ptr<Socket> m_socket;
    uint16_t    m_port;
    uint32_t    m_taskSize;
    uint32_t    m_currentRxBytes;
    uint32_t    m_nextTaskId;

    Time        m_simulationStep;
    uint32_t    m_tasksCompleted;
    std::queue<uint32_t> m_taskQueue;

    double      m_tasksPerSecond;
    double      m_processingCredit;
    bool        m_running;
};


// --- 2. 自定义生产者应用 (MyProducer) 声明 ---
class MyProducer : public Application
{
public:
    static TypeId GetTypeId(void);
    MyProducer();
    virtual ~MyProducer();

    void Setup(const std::vector<Address>& sinkAddresses, double lambda, uint32_t taskSize, uint32_t packetSize, Time simulationStep);

    // TracedCallback: nodeId, taskId (totalSent), targetAddress
    // 当一个新任务开始发送时触发
    TracedCallback<uint32_t, uint32_t, Address> m_taskSentTrace;

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    void SendPacket();
    void SendNextTask();
    void GenerateTasks();

    Ptr<Socket> m_socket;
    std::vector<Address> m_sinkAddresses;
    Address     m_currentTarget;
    uint32_t    m_taskSize;
    uint32_t    m_packetSize;
    uint32_t    m_packetsSentForCurrentTask;
    uint32_t    m_totalTasksSent;
    bool        m_isSending;

    Time m_simulationStep;
    double m_lambda;
    Ptr<ExponentialRandomVariable> m_interTaskTimeGenerator;
    std::queue<bool> m_taskQueue;
    Ptr<UniformRandomVariable> m_sinkSelector;
    bool m_running;
};

} // namespace ns3

#endif // PRO_SINK_APP_H
