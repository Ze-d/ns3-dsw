#pragma once

#include "dsw-structures.h"

#include <string>

#include "ns3/address.h"
#include "ns3/ptr.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

namespace ns3 {

// 前向声明
class MonitorConfig;

class MonitorConfig
{
public:
    MonitorConfig();

    // 当 Sink 完成一个任务时（Trace 回调）
    // nodeId: 消费者的节点 ID (用于 "Core-Id")
    // producerId: 任务来源的生产者 ID (用于 "Edge-Id")
    // taskId: 任务的 ID (由生产者分配，用于 "Task-Id")
    // totalCompleted: 该消费者节点累计完成的总任务数
    void OnSinkTaskCompleted(uint32_t nodeId,
                            uint32_t producerId,
                            uint32_t taskId,
                            uint32_t totalCompleted);

    // 当 Producer 发送一个新任务时（Trace 回调）
    // nodeId: 生产者的节点 ID (用于 "Edge-Id")
    // taskId: 任务的 ID (在该生产者上是唯一的, 用于 "Task-Id")
    // target: 任务发送的目标地址 (用于 "TargetIp")
    void OnProducerTaskSent(uint32_t nodeId, uint32_t taskId, Address target);

    // 当 Sink 报告算力利用率时 (Trace 回调)
    // nodeId: 消费者的节点 ID (用于 "Core-Id")
    // utilization: 利用率 (0.0 到 1.0)
    void OnSinkUtilization(uint32_t nodeId, double utilization);

    // 配置 FlowMonitor
    // 返回安装的 FlowMonitor 指针
    static Ptr<FlowMonitor> SetupFlowMonitor();

    // 处理和输出 FlowMonitor 统计结果
    // monitor: FlowMonitor 指针
    // fmh: FlowMonitorHelper 引用
    // statsCsv: CSV 输出文件路径（可选）
    // flowmonXml: FlowMonitor XML 输出文件路径
    static void ProcessFlowStats(Ptr<FlowMonitor> monitor,
                                FlowMonitorHelper& fmh,
                                const std::string& statsCsv,
                                const std::string& flowmonXml);

    // 打开 XML 输出文件
    // xmlFile: 主 XML 文件
    // utilXmlFile: 利用率 XML 文件
    // proSinkXmlFile: Pro-Sink XML 文件
    void OpenXmlFiles(std::ofstream& xmlFile,
                     std::ofstream& utilXmlFile,
                     const std::string& proSinkXmlFile);

    // 关闭 XML 输出文件
    // xmlFile: 主 XML 文件
    // utilXmlFile: 利用率 XML 文件
    void CloseXmlFiles(std::ofstream& xmlFile, std::ofstream& utilXmlFile);
};

} // namespace ns3
