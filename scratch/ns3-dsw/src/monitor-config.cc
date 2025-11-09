#include "monitor-config.h"

#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/simulator.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

NS_LOG_COMPONENT_DEFINE("MonitorConfig");

namespace ns3 {

// 外部全局文件流定义（与主文件保持一致）
static std::ofstream g_xmlFile;
static std::ofstream g_utilXmlFile;

MonitorConfig::MonitorConfig()
{
}

void
MonitorConfig::OnSinkTaskCompleted(uint32_t nodeId,
                                   uint32_t producerId,
                                   uint32_t taskId,
                                   uint32_t totalCompleted)
{
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "  <Event type=\"CoreComp\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Core-Id=\"Core-" << nodeId << "\""
                  << " Edge-Id=\"Edge-" << producerId << "\""
                  << " Task-Id=\"" << producerId << "-" << taskId << "\""
                  << " TotalCompleted=\"" << totalCompleted << "\"/>"
                  << std::endl;
    }
}

void
MonitorConfig::OnProducerTaskSent(uint32_t nodeId, uint32_t taskId, Address target)
{
    if (g_xmlFile.is_open())
    {
        g_xmlFile << "  <Event type=\"EdgeSend\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Edge-Id=\"Edge-" << nodeId << "\""
                  << " Task-Id=\"" << nodeId << "-" << taskId << "\""
                  << " TargetIp=\"" << InetSocketAddress::ConvertFrom(target).GetIpv4() << "\"/>"
                  << std::endl;
    }
}

void
MonitorConfig::OnSinkUtilization(uint32_t nodeId, double utilization)
{
    if (g_utilXmlFile.is_open())
    {
        g_utilXmlFile << "  <Event type=\"CoreUtil\""
                  << " Time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " Core-Id=\"Core-" << nodeId << "\""
                  << " Utilization=\"" << utilization << "\"/>"
                  << std::endl;
    }
}

Ptr<FlowMonitor>
MonitorConfig::SetupFlowMonitor()
{
    FlowMonitorHelper fmh;
    Ptr<FlowMonitor> monitor = fmh.InstallAll();
    return monitor;
}

void
MonitorConfig::ProcessFlowStats(Ptr<FlowMonitor> monitor,
                               FlowMonitorHelper& fmh,
                               const std::string& statsCsv,
                               const std::string& flowmonXml)
{
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
    auto stats = monitor->GetFlowStats();

    std::ofstream csv;
    if (!statsCsv.empty())
    {
        csv.open(statsCsv);
        csv << "flowId,src,dst,proto,sPort,dPort,tx,rx,lost,throughput_Mbps,avgDelay_ms,avgJitter_"
               "ms\n";
    }

    std::cout << "\n========== FlowMonitor per-flow statistics ==========\n";
    double sumThr = 0.0, sumDelay = 0.0, sumJit = 0.0;
    uint32_t rxFlows = 0;

    for (const auto& kv : stats)
    {
        FlowId id = kv.first;
        const auto& st = kv.second;
        auto t = classifier->FindFlow(id);

        double dur = st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds();
        if (dur <= 0.0)
            dur = 1e-9;
        double thr = st.rxBytes * 8.0 / dur / 1e6;
        double dly = (st.rxPackets > 0) ? (st.delaySum.GetSeconds() / st.rxPackets * 1000.0) : 0.0;
        double jit =
            (st.rxPackets > 1) ? (st.jitterSum.GetSeconds() / (st.rxPackets - 1) * 1000.0) : 0.0;

        std::cout << "Flow " << id << "  " << t.sourceAddress << " -> " << t.destinationAddress
                  << "  proto=" << (uint32_t)t.protocol << " sPort=" << t.sourcePort
                  << " dPort=" << t.destinationPort << "\n"
                  << "  TxPkts=" << st.txPackets << " RxPkts=" << st.rxPackets
                  << " Lost=" << st.lostPackets << "  Throughput=" << std::fixed
                  << std::setprecision(3) << thr << " Mbps"
                  << "  AvgDelay=" << std::setprecision(3) << dly << " ms"
                  << "  AvgJitter=" << std::setprecision(3) << jit << " ms\n";

        if (!statsCsv.empty())
        {
            csv << id << "," << t.sourceAddress << "," << t.destinationAddress << ","
                << (uint32_t)t.protocol << "," << t.sourcePort << "," << t.destinationPort << ","
                << st.txPackets << "," << st.rxPackets << "," << st.lostPackets << "," << std::fixed
                << std::setprecision(6) << thr << "," << dly << "," << jit << "\n";
        }
        if (st.rxPackets > 0)
        {
            ++rxFlows;
            sumThr += thr;
            sumDelay += dly;
            sumJit += jit;
        }
    }
    if (!statsCsv.empty())
        csv.close();

    if (rxFlows > 0)
    {
        std::cout << "\n--- Aggregated over " << rxFlows << " received flows ---  "
                  << "MeanThr=" << (sumThr / rxFlows) << " Mbps  "
                  << "MeanDelay=" << (sumDelay / rxFlows) << " ms  "
                  << "MeanJitter=" << (sumJit / rxFlows) << " ms\n";
    }

    monitor->SerializeToXmlFile(flowmonXml, true, true);
    NS_LOG_INFO("FlowMonitor XML written: " << flowmonXml);
}

void
MonitorConfig::OpenXmlFiles(std::ofstream& xmlFile,
                            std::ofstream& utilXmlFile,
                            const std::string& proSinkXmlFile)
{
    // 打开主 XML 文件
    xmlFile.open(proSinkXmlFile);
    if (!xmlFile.is_open())
    {
        NS_LOG_ERROR("Failed to open " << proSinkXmlFile << " for writing.");
    }
    else
    {
        xmlFile << "<ProSinkStats>\n";
        NS_LOG_INFO("Opened ProSink XML: " << proSinkXmlFile);
    }

    // 打开利用率 XML 文件（如果需要）
    // 注意：利用率文件的打开由调用者管理
}

void
MonitorConfig::CloseXmlFiles(std::ofstream& xmlFile, std::ofstream& utilXmlFile)
{
    if (xmlFile.is_open())
    {
        xmlFile << "</ProSinkStats>" << std::endl;
        xmlFile.close();
        NS_LOG_INFO("Closed ProSink XML file.");
    }

    if (utilXmlFile.is_open())
    {
        utilXmlFile.close();
        NS_LOG_INFO("Closed utilization XML file.");
    }
}

} // namespace ns3
