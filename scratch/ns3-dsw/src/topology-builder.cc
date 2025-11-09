#include "topology-builder.h"
#include "dswutils.h"

#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/names.h"
#include "ns3/point-to-point-module.h"
#include "ns3/string.h"
#include "ns3/traffic-control-module.h"

#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>

NS_LOG_COMPONENT_DEFINE("TopologyBuilder");

namespace ns3 {

TopologyBuilder::BuildResult
TopologyBuilder::BuildTopology(const std::vector<NodeSpec>& nodeSpecs,
                              const std::vector<LinkSpec>& linkSpecs,
                              uint32_t maxId,
                              bool delayByDist,
                              double meterPerUnit,
                              double propSpeed,
                              double delayFactor,
                              bool enablePcap)
{
    BuildResult result;

    // 创建节点ID集合
    for (const auto& n : nodeSpecs)
    {
        result.nodeIds.insert(n.id);
    }

    // 创建节点：索引 0..maxId（0 占位）
    NodeContainer nodes;
    nodes.Create(maxId + 1);
    result.nodes = nodes;

    // 名称与坐标
    std::vector<bool> hasPos(maxId + 1, false);
    std::vector<Vector> pos(maxId + 1, Vector(0, 0, 0));
    for (const auto& n : nodeSpecs)
    {
        if (!n.name.empty())
        {
            Names::Add(n.name, nodes.Get(n.id));
            NS_LOG_INFO("Name node " << n.id << " as '" << n.name << "'");
        }
        if (n.hasPos)
        {
            hasPos[n.id] = true;
            pos[n.id] = Vector(n.x, n.y, 0.0);
            NS_LOG_INFO("Preset position for node " << n.id << ": (" << n.x << "," << n.y << ")");
        }
    }
    result.positions = pos;

    // 自动布局未给坐标的节点
    const double dx = 2.0, dy = 2.0;
    uint32_t col = 0, row = 0;
    for (uint32_t id = 1; id <= maxId; ++id)
    {
        if (result.nodeIds.count(id) == 0)
            continue;
        if (!hasPos[id])
        {
            pos[id] = Vector(col * dx, row * dy, 0.0);
            hasPos[id] = true;
            ++col;
            if (col >= 8)
            {
                col = 0;
                ++row;
            }
            NS_LOG_DEBUG("Auto position for node " << id << ": (" << pos[id].x << "," << pos[id].y
                                                   << ")");
        }
    }

    // 协议栈
    InternetStackHelper internet;
    internet.Install(nodes);

    // 安装 Mobility（为索引 0..maxId 都放置一个坐标，未使用的放远处）
    MobilityHelper mob;
    Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
    allocator->Add(Vector(-10, -10, 0)); // 0 占位
    for (uint32_t id = 1; id <= maxId; ++id)
    {
        if (result.nodeIds.count(id) == 0)
        {
            allocator->Add(Vector(-50, -50, 0));
            continue;
        }
        allocator->Add(pos[id]);
    }
    mob.SetPositionAllocator(allocator);
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mob.Install(nodes);

    // 每条链路一个
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    // 链路安装（保留原始方向 a->b）
    std::set<std::pair<uint32_t, uint32_t>> seen; // 去重（无向）
    std::map<std::pair<uint32_t, uint32_t>, IfRecord> ifMap;
    NetDeviceContainer allP2pDevices; // 收集所有 P2P 设备, 用于 TCH

    for (const auto& l : linkSpecs)
    {
        auto undirected = DswUtils::Key(l.a, l.b);
        if (seen.count(undirected))
        {
            NS_LOG_WARN("Duplicate link spec " << l.a << "<->" << l.b << " ignored");
            continue;
        }
        seen.insert(undirected);

        if (result.nodeIds.count(l.a) == 0 || result.nodeIds.count(l.b) == 0)
        {
            NS_LOG_WARN("Link " << l.a << "<->" << l.b << " references undefined node id; skip");
            continue;
        }

        // 计算距离与时延（若启用）
        double du =
            std::hypot(pos[l.a].x - pos[l.b].x, pos[l.a].y - pos[l.b].y); // 坐标距离（单位）
        double meters = du * meterPerUnit;
        double delaySecComputed = (meters / propSpeed) * delayFactor;

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue(l.rate));
        if (delayByDist)
        {
            p2p.SetChannelAttribute("Delay", TimeValue(Seconds(delaySecComputed)));
        }
        else
        {
            // delay CSV column removed — use a sensible default when not computing by distance
            p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
        }

        // 原始方向：a 在前、b 在后 -> IP 地址 index 0 属于 a，index 1 属于 b
        NetDeviceContainer dev = p2p.Install(nodes.Get(l.a), nodes.Get(l.b));
        allP2pDevices.Add(dev); // 将设备添加到容器

        Ipv4InterfaceContainer ifc = address.Assign(dev);
        address.NewNetwork();

        std::string delayLabel =
            delayByDist ? DswUtils::FormatTime(delaySecComputed) : std::string("1ms");
        std::cout << "[link] " << l.a << "<->" << l.b << "  id=" << l.id << "  rate=" << l.rate
                  << "  delay=" << delayLabel;
        if (delayByDist)
        {
            std::cout << "  dist=" << std::fixed << std::setprecision(3) << du << " units ("
                      << std::setprecision(1) << meters << " m)";
        }
        std::cout << "  " << ifc.GetAddress(0) << " <-> " << ifc.GetAddress(1) << std::endl;

        if (enablePcap)
        {
            std::ostringstream os;
            os << "pcap-" << l.a << "-" << l.b;
            p2p.EnablePcapAll(os.str(), true);
        }

        IfRecord rec;
        rec.a = l.a;
        rec.b = l.b;
        rec.ifc = ifc;
        rec.rate = l.rate;
        rec.delay = delayLabel;
        rec.distanceUnits = du;
        rec.distanceMeters = meters;
        rec.id = l.id;
        ifMap[undirected] = rec;
    }

    // --- 在安装 FqCoDel 之前，删除 P2PHelper 自动安装的默认 QueueDisc ---
    NS_LOG_INFO("Deleting default QueueDiscs installed by P2P helper...");
    for (uint32_t i = 0; i < allP2pDevices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = allP2pDevices.Get(i);
        Ptr<Node> node = dev->GetNode();
        // 从节点获取 TC-Layer
        Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
        if (!tc)
        {
            NS_LOG_WARN("Node has no TrafficControlLayer");
            continue;
        }
        Ptr<NetDevice> nd = node->GetDevice(dev->GetIfIndex());
        if (!nd)
        {
            NS_LOG_WARN("Cannot get NetDevice from device container");
            continue;
        }
        // 找到对应的 NetDevice (可能是 ChannelStart 接口)
        uint32_t nDevices = node->GetNDevices();
        for (uint32_t j = 0; j < nDevices; ++j)
        {
            Ptr<NetDevice> nd2 = node->GetDevice(j);
            if (nd2 == nd)
            {
                tc->DeleteRootQueueDiscOnDevice(nd);
            }
        }
    }

    // 安装 FqCoDel 队列
    NS_LOG_INFO("Installing FqCoDel queue disc on all P2P devices for Pacing...");
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    tch.Install(allP2pDevices);

    // 注册链路到 LinkUtilizationMonitor
    // 注意：这里没有直接注册，需要调用者在外部处理

    // 完成，返回结果
    result.ifMap = ifMap;
    result.p2pDevices = allP2pDevices;
    result.positions = pos;

    return result;
}

} // namespace ns3
