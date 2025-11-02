#include "link-utilization-monitor.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h" // 包含 StringValue
#include "ns3/double.h" // 包含 DoubleValue
#include <list>         // [修改]
#include <atomic>       // [修改]

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
      m_xmlFilePath("scratch/ns3-dsw/out/link_util.xml"), // 默认路径
      m_lastPollTime(Seconds(0)) // [修改] 初始化上次轮询时间
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

    // [修改] 使用 emplace_back() 在 list 末尾就地构造一个默认的 LinkRecord
    // 这会调用 LinkRecord 的默认构造函数，并将 atomic 成员零初始化
    m_links.emplace_back(); 
    
    // [修改] 获取刚创建的、位于 list 中的对象的稳定指针
    LinkRecord* recPtr = &m_links.back();

    // [修改] 通过指针填充就地构造的对象
    recPtr->linkId = linkId;
    recPtr->nodeAId = nodeAId;
    recPtr->nodeBId = nodeBId;
    recPtr->rate = rate;
    recPtr->qd_A_to_B = qdA;
    recPtr->qd_B_to_A = qdB;
    
    // [修改] 显式初始化 atomic (虽然默认构造已将其置零，但这样更清晰)
    recPtr->intervalBytes_A_to_B = 0;
    recPtr->intervalBytes_B_to_A = 0;
    
    // 缓存速率的字符串形式
    std::stringstream ss;
    ss << rate;
    recPtr->rateStr = ss.str();


    // [修改] 连接 QueueDisc 的 Dequeue 跟踪到我们的静态回调函数
    // 使用 MakeBoundCallback 绑定额外的参数 (recPtr 和 方向)
    qdA->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&LinkUtilizationMonitor::StaticOnDequeue, recPtr, true)); // true = A->B
    
    qdB->TraceConnectWithoutContext(
        "Dequeue",
        MakeBoundCallback(&LinkUtilizationMonitor::StaticOnDequeue, recPtr, false)); // false = B->A


    NS_LOG_INFO("Monitoring Link " << linkId << " (" << nodeAId << "<->" << nodeBId
                                   << ") Rate: " << recPtr->rateStr);
}

void
LinkUtilizationMonitor::Start()
{
    //在启动时，按 linkId 对 m_links 列表进行排序
    // [修改] 使用 std::list::sort
    m_links.sort(
        [](const LinkRecord& a, const LinkRecord& b) { return a.linkId < b.linkId; });
    
    m_running = true;
    WriteXmlHeader();
    
    // [修改] 设置启动轮询的基准时间
    m_lastPollTime = Simulator::Now(); 
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

    // [关键修复] 计算自上次轮询以来的 *实际* 时间差
    Time now = Simulator::Now();
    double dt = (now - m_lastPollTime).GetSeconds();
    m_lastPollTime = now; // [关键修复] 更新上次轮询时间

    if (dt == 0.0)
    {
        NS_LOG_WARN("LinkUtilizationMonitor: Poll interval 'dt' is zero, skipping sample.");
        // [修改] 重新调度并返回，避免除以零
        Simulator::Schedule(m_interval, &LinkUtilizationMonitor::PollStats, this);
        return; 
    }

    // --- 2. 打印控制台表头 ---
    std::cout << "\n--- Link Utilization Report (" << std::fixed << std::setprecision(2)
              << now.GetSeconds() << "s, Interval: " << std::setprecision(4) << dt << "s) ---" << std::endl; // [修改] 打印真实 dt

    // --- 3. 写入 XML 采样头 ---
    if (m_xmlFile.is_open())
    {
        m_xmlFile << "  <Sample time=\"" << std::fixed << std::setprecision(6) << now.GetSeconds()
                  << "\" dt=\"" << dt << "\">" << std::endl; // [修改] 记录真实 dt
    }

    double totalUtilPct = 0.0;
    uint32_t linkCount = m_links.size();

    // --- 4. 遍历所有注册的链路 ---
    for (auto& link : m_links)
    {
        // 4.1 [修改] 从回调累加器中原子地读取并重置字节数
        // exchange(0) 会返回当前值，并将计数器设置为0
        uint64_t bytesA = link.intervalBytes_A_to_B.exchange(0);
        uint64_t bytesB = link.intervalBytes_B_to_A.exchange(0);

        // 4.2 计算速率 (R = (Bytes_in_interval) * 8 / dt_actual)
        double rateA_bps = (bytesA * 8.0) / dt;
        double rateB_bps = (bytesB * 8.0) / dt;
        double rateA_Mbps = rateA_bps / 1e6;
        double rateB_Mbps = rateB_bps / 1e6;

        // 4.3 计算利用率 (U = R / C * 100)
        double capacity_bps = link.rate.GetBitRate();
        double capacity_Mbps = capacity_bps / 1e6;
        double utilA_pct = (capacity_bps > 0) ? (rateA_bps / capacity_bps) * 100.0 : 0.0;
        double utilB_pct = (capacity_bps > 0) ? (rateB_bps / capacity_bps) * 100.0 : 0.0;

        totalUtilPct += utilA_pct + utilB_pct;

        // 4.4 打印到控制台 (使用 std::setw 进行对齐)
        std::cout << "  Link " << std::setw(3) << link.linkId << " (Rate: " 
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
                      << "\" utilPct=\"" << utilA_pct << "\" bytes=\"" << bytesA << "\" />\n" // [修改] 增加 bytes
                      << "      <BtoA rateBps=\"" << rateB_bps << "\" rateMbps=\"" << rateB_Mbps
                      << "\" utilPct=\"" << utilB_pct << "\" bytes=\"" << bytesB << "\" />\n" // [修改] 增加 bytes
                      << "    </Link>\n";
        }

        // 4.6 [修改] 移除对 prevTxBytes 的更新
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


// [新增] 静态回调函数的实现
void
LinkUtilizationMonitor::StaticOnDequeue(LinkRecord* rec, bool isAtoB, Ptr<const QueueDiscItem> item)
{
    // 这是一个静态函数，它通过 MakeBoundCallback 获得了 'rec' 指针
    if (!rec || !item)
    {
        return;
    }

    Ptr<const Packet> packet = item->GetPacket();  // ✅ 从 QueueDiscItem 中取出 Packet
    if (!packet)
    {
        return;
    }

    uint32_t packetSize = packet->GetSize();

    if (isAtoB)
    {
        // 原子地增加 A->B 的字节计数器
        rec->intervalBytes_A_to_B.fetch_add(packetSize, std::memory_order_relaxed);
    }
    else
    {
        // 原子地增加 B->A 的字节计数器
        rec->intervalBytes_B_to_A.fetch_add(packetSize, std::memory_order_relaxed);
    }
}


} // namespace ns3