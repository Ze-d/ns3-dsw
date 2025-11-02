#include "link-utilization-monitor.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h" // 包含 StringValue
#include "ns3/double.h" // 包含 DoubleValue
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("LinkUtilizationMonitor");

TypeId
LinkUtilizationMonitor::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::LinkUtilizationMonitor")
            .SetParent<Object>()
            .SetGroupName("TrafficControl")
            .AddConstructor<LinkUtilizationMonitor>();
    return tid;
}

LinkUtilizationMonitor::LinkUtilizationMonitor()
    : m_interval(Seconds(0.25)),
      m_running(false),
      m_xmlHeaderWritten(false),
      m_xmlFilePath("scratch/ns3-dsw/out/link_util.xml") // 默认路径
{
}

LinkUtilizationMonitor::~LinkUtilizationMonitor()
{
    // DoDispose will handle file closing
}

void
LinkUtilizationMonitor::DoDispose()
{
    m_running = false; // 确保停止
    if (m_xmlFile.is_open())
    {
        if (m_xmlHeaderWritten)
        {
            WriteXmlFooter();
        }
        m_xmlFile.close();
    }
    Object::DoDispose();
}

void
LinkUtilizationMonitor::SetPollInterval(Time interval)
{
    m_interval = interval;
}

void
LinkUtilizationMonitor::SetXmlOutput(std::string path)
{
    m_xmlFilePath = path;
}

Ptr<TrafficControlLayer>
LinkUtilizationMonitor::GetTcLayer(Ptr<NetDevice> dev)
{
    if (!dev)
    {
        return nullptr;
    }
    Ptr<Node> node = dev->GetNode(); //
    if (!node)
    {
        return nullptr;
    }
    return node->GetObject<TrafficControlLayer>(); //
}

void
LinkUtilizationMonitor::RegisterLink(uint32_t linkId,
                                     uint32_t nodeAId,
                                     uint32_t nodeBId,
                                     Ptr<NetDevice> devA,
                                     Ptr<NetDevice> devB,
                                     DataRate rate)
{
    Ptr<TrafficControlLayer> tcA = GetTcLayer(devA);
    Ptr<TrafficControlLayer> tcB = GetTcLayer(devB);

    if (!tcA || !tcB)
    {
        NS_LOG_WARN("Could not find TrafficControlLayer for Link " << linkId << ". Link monitoring disabled.");
        return;
    }

    Ptr<QueueDisc> qdA = tcA->GetRootQueueDiscOnDevice(devA); //
    Ptr<QueueDisc> qdB = tcB->GetRootQueueDiscOnDevice(devB); //

    if (!qdA || !qdB)
    {
        NS_LOG_WARN("Could not find Root QueueDisc for Link " << linkId
                   << " (Ensure TrafficControlHelper ran first). Link monitoring disabled.");
        return;
    }

    LinkRecord rec;
    rec.linkId = linkId;
    rec.nodeAId = nodeAId;
    rec.nodeBId = nodeBId;
    rec.rate = rate;
    rec.qd_A_to_B = qdA;
    rec.qd_B_to_A = qdB;
    rec.prevTxBytes_A_to_B = 0;
    rec.prevTxBytes_B_to_A = 0;

    // 缓存速率的字符串形式
    std::stringstream ss;
    ss << rate;
    rec.rateStr = ss.str();

    m_links.push_back(rec);
    NS_LOG_INFO("Monitoring Link " << linkId << " (" << nodeAId << "<->" << nodeBId
                                   << ") Rate: " << rec.rateStr);
}

void
LinkUtilizationMonitor::Start()
{
    //在启动时，按 linkId 对 m_links 向量进行排序
    std::sort(m_links.begin(),
              m_links.end(),
              [](const LinkRecord& a, const LinkRecord& b) { return a.linkId < b.linkId; });
    m_running = true;
    WriteXmlHeader();
    Simulator::ScheduleNow(&LinkUtilizationMonitor::PollStats, this);
}

void
LinkUtilizationMonitor::Stop()
{
    m_running = false;
}

void
LinkUtilizationMonitor::WriteXmlHeader()
{
    m_xmlFile.open(m_xmlFilePath.c_str());
    if (!m_xmlFile.is_open())
    {
        NS_LOG_ERROR("Failed to open XML file for writing: " << m_xmlFilePath);
        return;
    }
    m_xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    m_xmlFile << "<LinkUtilizationStats interval=\"" << m_interval.GetSeconds() << "\">"
              << std::endl;
    m_xmlHeaderWritten = true;
}

void
LinkUtilizationMonitor::WriteXmlFooter()
{
    if (!m_xmlFile.is_open() || !m_xmlHeaderWritten)
    {
        return;
    }
    m_xmlFile << "</LinkUtilizationStats>" << std::endl;
    m_xmlHeaderWritten = false;
}

void
LinkUtilizationMonitor::PollStats()
{
    // 1. 检查是否应该停止
    if (!m_running)
    {
        return;
    }

    Time now = Simulator::Now();
    double dt = m_interval.GetSeconds();

    if (dt == 0.0)
    {
        NS_LOG_WARN("LinkUtilizationMonitor: Poll interval is zero, cannot calculate rates.");
        return;
    }

    // --- 2. 打印控制台表头 ---
    std::cout << "\n--- Link Utilization Report (" << std::fixed << std::setprecision(2)
              << now.GetSeconds() << "s, Interval: " << dt << "s) ---" << std::endl;

    // --- 3. 写入 XML 采样头 ---
    if (m_xmlFile.is_open())
    {
        m_xmlFile << "  <Sample time=\"" << std::fixed << std::setprecision(6) << now.GetSeconds()
                  << "\">" << std::endl;
    }

    double totalUtilPct = 0.0;
    uint32_t linkCount = m_links.size();

    // --- 4. 遍历所有注册的链路 ---
    for (auto& link : m_links)
    {
        // 4.1 获取当前的统计数据
        // GetStats() 会返回一个*包含* nTotalSentBytes 的 Stats 结构体
        QueueDisc::Stats statsA = link.qd_A_to_B->GetStats();
        QueueDisc::Stats statsB = link.qd_B_to_A->GetStats();

        uint64_t txNowA = statsA.nTotalSentBytes; //
        uint64_t txNowB = statsB.nTotalSentBytes; //

        // 4.2 计算速率 (R = (Tx_now - Tx_prev) * 8 / dt)
        double rateA_bps = (txNowA - link.prevTxBytes_A_to_B) * 8.0 / dt;
        double rateB_bps = (txNowB - link.prevTxBytes_B_to_A) * 8.0 / dt;
        double rateA_Mbps = rateA_bps / 1e6;
        double rateB_Mbps = rateB_bps / 1e6;

        // 4.3 计算利用率 (U = R / C * 100)
        double capacity_bps = link.rate.GetBitRate();
        double capacity_Mbps = capacity_bps / 1e6; // <-- 新增：将带宽转换为 Mbps
        double utilA_pct = (capacity_bps > 0) ? (rateA_bps / capacity_bps) * 100.0 : 0.0;
        double utilB_pct = (capacity_bps > 0) ? (rateB_bps / capacity_bps) * 100.0 : 0.0;

        totalUtilPct += utilA_pct + utilB_pct;

        // 4.4 打印到控制台 (使用 std::setw 进行对齐)
        std::cout << "  Link " << std::setw(3) << link.linkId << " (Rate: " 
                  << std::fixed << std::setprecision(2) << std::setw(7) 
                  << capacity_Mbps << " Mbps): "
                  << " A(" << std::setw(2) << link.nodeAId << ")->B(" << std::setw(2)
                  << link.nodeBId << "): " << std::fixed << std::setprecision(2) << std::setw(7)
                  << rateA_Mbps << " Mbps (" << std::fixed << std::setprecision(1) << std::setw(6)
                  << utilA_pct << "%)"
                  << " | B(" << std::setw(2) << link.nodeBId << ")->A(" << std::setw(2)
                  << link.nodeAId << "): " << std::fixed << std::setprecision(2)
                  << std::setw(7) << rateB_Mbps << " Mbps (" << std::fixed
                  << std::setprecision(1) << std::setw(6) << utilB_pct << "%)" << std::endl;

        // 4.5 写入 XML
        if (m_xmlFile.is_open())
        {
            m_xmlFile << "    <Link id=\"" << link.linkId << "\" rate=\"" << link.rateStr
                      << "\" nodeA=\"" << link.nodeAId << "\" nodeB=\"" << link.nodeBId << "\">\n"
                      << "      <AtoB rateBps=\"" << rateA_bps << "\" rateMbps=\"" << rateA_Mbps
                      << "\" utilPct=\"" << utilA_pct << "\" />\n"
                      << "      <BtoA rateBps=\"" << rateB_bps << "\" rateMbps=\"" << rateB_Mbps
                      << "\" utilPct=\"" << utilB_pct << "\" />\n"
                      << "    </Link>\n";
        }

        // 4.6 更新上一次的字节数
        link.prevTxBytes_A_to_B = txNowA;
        link.prevTxBytes_B_to_A = txNowB;
    }

    // --- 5. 打印全局平均利用率 ---
    // (总利用率 / (链路数 * 2个方向))
    double avgGlobalUtil = (linkCount > 0) ? (totalUtilPct / (linkCount * 2.0)) : 0.0;
    std::cout << "[Network]: Avg Global Utilization: " << std::fixed << std::setprecision(2)
              << avgGlobalUtil << "%" << std::endl;

    // --- 6. 写入 XML 采样尾 ---
    if (m_xmlFile.is_open())
    {
        m_xmlFile << "  </Sample>" << std::endl;
    }

    // --- 7. 调度下一次轮询 ---
    Simulator::Schedule(m_interval, &LinkUtilizationMonitor::PollStats, this);
}

} // namespace ns3