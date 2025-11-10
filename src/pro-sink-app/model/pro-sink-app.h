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

#include <fstream>
#include <string>

// 前向声明
namespace ns3 {
class PriceAwareScheduler;
};

namespace ns3 {

// --- 0. 自定义包头 (TaskHeader) 声明 ---
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

    /**
     * @brief (Simple Setup) 仅配置基础任务处理功能。
     */
    void Setup(uint32_t nodeId,
               double tasksPerSecond, 
               Time simulationStep, 
               Time updateInterval);

    /**
     * @brief (Extended Setup) 配置任务处理 + 电力耦合功能。
     */
    void Setup(uint32_t nodeId,
               double tasksPerSecond, 
               Time simulationStep, 
               Time updateInterval,
               double warmupTime,
               double basePower,
               double fullPower,
               double phaseOffset,
               const std::vector<double>& priceProfile,
               const std::string& powerCostXmlPath);

    TracedCallback<uint32_t, uint32_t, uint32_t, uint32_t> m_taskCompletedTrace;
    TracedCallback<uint32_t, double> m_utilizationTrace; // <NodeId, Utilization>
    TracedCallback<uint32_t, uint32_t> m_queueLengthTrace; // <NodeId, QueueLength>

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // --- TCP ---
    bool HandleAccept(Ptr<Socket> socket, const Address& from);
    void HandleNewConnection(Ptr<Socket> socket, const Address& from);
    void CleanupConnection(Ptr<Socket> socket);
    void HandleRead(Ptr<Socket> socket);
    void HandleNormalClose(Ptr<Socket> socket);
    void HandleErrorClose(Ptr<Socket> socket);
    void ProcessSocketBuffer(Ptr<Socket> socket);

    // --- 任务处理 ---
    void ProcessTasks();
    
    // --- 算力统计 & 功率/成本计算 ---
    void ReportUtilization();
    uint32_t GetCurrentPriceIndex() const;


    Ptr<Socket> m_listenSocket; 
    std::list<Ptr<Socket>> m_acceptedSockets; 
    std::map<Ptr<Socket>, Buffer> m_socketBuffers;

    uint16_t    m_port;
    uint32_t    m_taskSize;
    uint32_t    m_packetSize;

    std::map<std::pair<uint32_t, uint32_t>, uint32_t> m_currentRxBytesPerTask;

    // --- 初始化状态跟踪 ---
    bool        m_initialized;   // 是否已初始化（已调用StartApplication）
    uint32_t    m_nodeId;        // 节点ID（延迟获取以避免空指针）
    std::string m_powerCostXmlPath; // 功率成本XML文件路径（延迟打开）

    Time        m_simulationStep;
    uint32_t    m_tasksCompleted;
    std::queue<std::pair<uint32_t, uint32_t>> m_taskQueue;

    double      m_tasksPerSecond;
    double      m_processingCredit;
    bool        m_running;

    // --- 算力统计成员 ---
    Time        m_updateInterval;       
    Time        m_totalTimeInInterval;  
    Time        m_idleTimeInInterval;   
        
    // --- Power and Cost members ---
    bool                m_powerCouplingEnabled;   // 开关
    double              m_warmupTime;             // 仿真预热时间 (s)
    double              m_basePower;              // 基础功率 (MW)
    double              m_fullPower;              // 满载功率 (MW)
    double              m_pricePhaseOffsetHours;  // 电价相位偏移 (h)
    std::vector<double> m_priceProfile;           // 电价曲线 (由 main 传入)
    std::ofstream       m_powerCostXmlFile;       // 功率成本 XML 输出流
    double              m_totalAccumulatedCost;   // 累积的总成本
};


// --- 2. 自定义生产者应用 (MyProducer) ---
class MyProducer : public Application
{
public:
    static TypeId GetTypeId(void);
    MyProducer();
    virtual ~MyProducer();

    void Setup(Address sinkAddress, double lambda, uint32_t taskSize, uint32_t packetSize, Time simulationStep);

    /**
     * @brief 启用价格感知调度
     * @param enable 是否启用
     */
    void EnablePriceAwareScheduling(bool enable = true);

    /**
     * @brief 设置调度器
     * @param scheduler 调度器实例
     */
    void SetScheduler(Ptr<PriceAwareScheduler> scheduler);

    TracedCallback<uint32_t, uint32_t, Address> m_taskSentTrace;

private:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

    // --- TCP 回调 ---
    void ConnectionSucceeded(Ptr<Socket> socket);
    void ConnectionFailed(Ptr<Socket> socket);
    void HandleSend(Ptr<Socket> socket, uint32_t txSpace); // 流量控制
    void NormalClose(Ptr<Socket> socket);
    void ErrorClose(Ptr<Socket> socket);

    // --- 发送和生成逻辑 ---
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

    // --- 价格感知调度 ---
    bool m_enablePriceAwareScheduling;  // 是否启用价格感知调度
    Ptr<PriceAwareScheduler> m_scheduler;  // 调度器实例

    bool m_running;
    bool m_connected;
};

} // namespace ns3

#endif // PRO_SINK_APP_H