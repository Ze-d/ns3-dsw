/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * scratch/topo_figure_flowmon_cfg.cc
 *
 * 功能概览：
 * - 从 CSV 读取节点：nodes.csv => id[,x,y[,name]]
 * - 从 CSV 读取链路：links.csv => a,b,rate,delay
 * - 支持“按距离自动计算链路时延”：delay = (欧氏距离*meterPerUnit / propSpeed) * delayFactor
 *   可用 --delayByDist=1 开启（默认 1），否则使用 CSV 的 delay
 * - 为每条链路独立设置带宽/时延，按 a->b 的原始方向分配 IP
 * - 安装 UDP Echo（自动选择一条链路的 b 端为 server；client 为最小 ID 节点）
 * - 启用 FlowMonitor（XML + 可选 CSV）
 * - NetAnim 可视化（server=红，client=绿，其余=蓝）
 * - Graphviz 导出 .dot（带链路标签：速率/时延；按 CSV 坐标固定布局）
 *
 * 构建：
 *   ./ns3 build
 * 运行示例：
 *   ./ns3 run "scratch/topo_figure_flowmon_cfg \
 *     --nodes=scratch/nodes.csv --links=scratch/links.csv \
 *     --delayByDist=1 --meterPerUnit=50000 --propSpeed=2e8 --delayFactor=1.0 \
 *     --stop=25 --anim=1 --pcap=0 --log=info \
 *     --statsCsv=flowstats.csv --dot=scratch/topo.dot --dotScale=80"
 * 生成图片（需 graphviz）：
 *   dot -Tpng scratch/topo.dot -o scratch/topo.png
 */

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <cmath> // hypot

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/log.h"
#include "ns3/names.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TopoFigureFlowmonCfg");

// ----------------------------- 工具函数 -----------------------------
static inline std::string Trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(s[e-1]))) --e;
  return s.substr(b, e-b);
}
static inline std::pair<uint32_t,uint32_t> Key(uint32_t a, uint32_t b) {
  return std::minmax(a,b); // 无向键
}
static inline std::string FormatTime(double sec) {
  std::ostringstream os; os.setf(std::ios::fixed);
  if (sec < 1e-6)      { os << std::setprecision(3) << (sec*1e9)  << "ns"; }
  else if (sec < 1e-3) { os << std::setprecision(3) << (sec*1e6)  << "us"; }
  else if (sec < 1.0)  { os << std::setprecision(3) << (sec*1e3)  << "ms"; }
  else                 { os << std::setprecision(3) <<  sec       << "s";  }
  return os.str();
}

// ----------------------------- 配置结构 -----------------------------
struct NodeSpec {
  uint32_t id = 0;
  bool hasPos = false;
  double x = 0.0, y = 0.0;
  std::string name; // 可空
};

struct LinkSpec {
  uint32_t a = 0, b = 0;     // 原始方向（用于确定 IP 顺序）
  std::string rate;          // e.g., "100Mbps"
  std::string delay;         // e.g., "2ms"（当 delayByDist=1 时被忽略）
};

// nodes.csv: id[,x,y[,name]]
static std::vector<NodeSpec> LoadCsvNodes(const std::string& path) {
  std::vector<NodeSpec> out;
  std::ifstream fin(path.c_str());
  if (!fin.is_open()) { NS_FATAL_ERROR("Cannot open nodes file: " << path); }
  std::string line; uint32_t ln = 0;
  while (std::getline(fin, line)) {
    ++ln; std::string s = Trim(line);
    if (s.empty() || s[0]=='#') continue;

    std::stringstream ss(s);
    std::string fid, fx, fy, fname;
    std::getline(ss, fid, ',');
    std::getline(ss, fx,  ',');
    std::getline(ss, fy,  ',');
    std::getline(ss, fname); // 可能为空

    fid = Trim(fid); fx = Trim(fx); fy = Trim(fy); fname = Trim(fname);

    if (!std::all_of(fid.begin(), fid.end(), ::isdigit)) {
      if (ln==1) { NS_LOG_WARN("Skip header in nodes.csv: " << s); continue; }
      NS_LOG_WARN("Skip invalid node line " << ln << ": " << s);
      continue;
    }

    NodeSpec ns; ns.id = static_cast<uint32_t>(std::stoul(fid));
    if (!fx.empty() && !fy.empty()) { ns.hasPos = true; ns.x = std::stod(fx); ns.y = std::stod(fy); }
    ns.name = fname;
    if (ns.id == 0) { NS_LOG_WARN("Node id 0 is reserved. Skip line " << ln); continue; }
    out.push_back(ns);
  }
  return out;
}

// links.csv: a,b,rate,delay
static std::vector<LinkSpec> LoadCsvLinks(const std::string& path) {
  std::vector<LinkSpec> links;
  std::ifstream fin(path.c_str());
  if (!fin.is_open()) { NS_FATAL_ERROR("Cannot open links file: " << path); }
  std::string line; uint32_t ln = 0;
  while (std::getline(fin, line)) {
    ++ln; std::string s = Trim(line);
    if (s.empty() || s[0]=='#') continue;

    std::stringstream ss(s);
    std::string fa, fb, fr, fd;
    if (!std::getline(ss, fa, ',')) continue;
    if (!std::getline(ss, fb, ',')) continue;
    if (!std::getline(ss, fr, ',')) continue;
    if (!std::getline(ss, fd, ',')) continue;

    fa = Trim(fa); fb = Trim(fb); fr = Trim(fr); fd = Trim(fd);

    if (!std::all_of(fa.begin(), fa.end(), ::isdigit) ||
        !std::all_of(fb.begin(), fb.end(), ::isdigit)) {
      if (ln==1) { NS_LOG_WARN("Skip header in links.csv: " << s); continue; }
      NS_LOG_WARN("Skip invalid link line " << ln << ": " << s);
      continue;
    }

    LinkSpec ls;
    ls.a = (uint32_t)std::stoul(fa);
    ls.b = (uint32_t)std::stoul(fb);
    ls.rate  = fr;
    ls.delay = fd; // 可能被忽略（按距离计算时）
    if (ls.a==0 || ls.b==0 || ls.a==ls.b) {
      NS_LOG_WARN("Skip invalid/self-loop link at line " << ln << ": " << s);
      continue;
    }
    links.push_back(ls);
  }
  return links;
}

static void SetupLogging(const std::string& levelStr) {
  std::string s = levelStr; std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  if (s=="off")  return;
  LogLevel lv = LOG_LEVEL_INFO;
  if (s=="warn")  lv = LOG_LEVEL_WARN;
  if (s=="info")  lv = LOG_LEVEL_INFO;
  if (s=="debug") lv = LOG_LEVEL_DEBUG;
  if (s=="all")   lv = LOG_LEVEL_ALL;
  LogComponentEnable("TopoFigureFlowmonCfg", lv);
  NS_LOG_INFO("Logging level set to: " << s);
}

// 记录每条链路的接口与“原始方向 a->b”

struct IfRecord {
  uint32_t a = 0;
  uint32_t b = 0; // 与 CSV 中一致的原始次序
  std::string rate;
  std::string delay;  // 标签展示（可能是计算值）
  double distanceUnits = 0.0;
  double distanceMeters = 0.0;
  Ipv4InterfaceContainer ifc;
};

// ----------------------------- Graphviz 导出 -----------------------------
static void WriteGraphvizDot(const std::string& path,
                             const std::set<uint32_t>& nodeIds,
                             const std::vector<Vector>& pos,
                             const std::map<std::pair<uint32_t,uint32_t>, IfRecord>& ifMap,
                             double scale,
                             uint32_t clientId,
                             uint32_t serverId)
{
  std::ofstream dot(path.c_str());
  if (!dot.is_open()) {
    NS_LOG_WARN("Cannot open dot path for write: " << path);
    return;
  }
  dot << "graph topo {\n";
  dot << "  layout=neato;\n  overlap=false;\n  splines=true;\n";
  dot << "  node [shape=circle, style=filled, fontname=\"Helvetica\"];\n\n";

  for (uint32_t id : nodeIds) {
    double X = pos[id].x * scale;
    double Y = pos[id].y * scale;
    std::ostringstream label; label << id;
    std::string color = "#1f77b4"; // 默认蓝
    if (id == serverId) color = "#d62728";      // 红
    else if (id == clientId) color = "#2ca02c"; // 绿
    dot << "  n" << id << " [label=\"" << label.str()
        << "\", pos=\"" << X << "," << Y << "!\", pin=true, fillcolor=\"" << color << "\"];\n";
  }
  dot << "\n";
  for (const auto& kv : ifMap) {
    const auto& undirected = kv.first;
    const auto& rec = kv.second;
    dot << "  n" << undirected.first << " -- n" << undirected.second
        << " [label=\"" << rec.rate << " / " << rec.delay << "\", penwidth=2];\n";
  }
  dot << "}\n";
  dot.close();
  std::cout << "[viz] Graphviz .dot written: " << path << std::endl;
}

// ----------------------------- 主程序 -----------------------------
int main (int argc, char* argv[])
{
  std::string nodesCsv = "scratch/nodes.csv";
  std::string linksCsv = "scratch/links.csv";
  std::string logLevel = "info";             // off|warn|info|debug|all
  std::string flowmonXml = "topo-figure.perlink.flowmon.xml";
  std::string statsCsv = "";                 // 若非空则导出 CSV 指标
  std::string animXml = "topo-figure.xml";
  std::string dotPath = "";                  // 若非空导出 .dot
  double dotScale = 80.0;                    // dot 坐标缩放
  double stopTime = 20.0;
  bool enablePcap = false;
  bool enableAnim = true;

  // 距离->时延控制（默认启用）
  bool   delayByDist = true;     // 1=按距离计算，0=用 CSV delay
  double meterPerUnit = 50000.0; // 1 坐标单位=多少米；默认 50 km，得到毫秒级时延
  double propSpeed    = 2e8;     // 传播速度 m/s（光纤近似）
  double delayFactor  = 1.0;     // 额外缩放系数

  CommandLine cmd;
  cmd.AddValue("nodes",   "CSV of nodes: id[,x,y[,name]]", nodesCsv);
  cmd.AddValue("links",   "CSV of links: a,b,rate,delay", linksCsv);
  cmd.AddValue("stop",    "Simulation stop time (s)", stopTime);
  cmd.AddValue("pcap",    "Enable pcap on all links (0/1)", enablePcap);
  cmd.AddValue("anim",    "Enable NetAnim output (0/1)", enableAnim);
  cmd.AddValue("log",     "Log level: off|warn|info|debug|all", logLevel);
  cmd.AddValue("flowXml", "FlowMonitor XML output", flowmonXml);
  cmd.AddValue("statsCsv","Write per-flow stats to CSV (path)", statsCsv);
  cmd.AddValue("animXml", "NetAnim XML output", animXml);
  cmd.AddValue("dot",     "Write Graphviz .dot to this path (empty to disable)", dotPath);
  cmd.AddValue("dotScale","Scale factor for coordinates in .dot", dotScale);

  // 按距离计算时延的参数
  cmd.AddValue("delayByDist", "If 1, compute link delay from node distance", delayByDist);
  cmd.AddValue("meterPerUnit","Meters per coordinate unit", meterPerUnit);
  cmd.AddValue("propSpeed",   "Propagation speed (m/s)", propSpeed);
  cmd.AddValue("delayFactor", "Extra multiplier for computed delay", delayFactor);

  cmd.Parse(argc, argv);
  SetupLogging(logLevel);

  // 读取配置
  auto nodeSpecs = LoadCsvNodes(nodesCsv);
  auto linkSpecs = LoadCsvLinks(linksCsv);
  if (nodeSpecs.empty()) NS_FATAL_ERROR("No nodes parsed from " << nodesCsv);
  if (linkSpecs.empty()) NS_FATAL_ERROR("No links parsed from " << linksCsv);

  // 节点集合与最大 ID
  std::set<uint32_t> nodeIds;
  uint32_t maxId = 0;
  for (const auto& n : nodeSpecs) { nodeIds.insert(n.id); maxId = std::max(maxId, n.id); }
  for (const auto& l : linkSpecs) { nodeIds.insert(l.a); nodeIds.insert(l.b); maxId = std::max(maxId, std::max(l.a,l.b)); }

  NS_LOG_INFO("Nodes in config: " << nodeIds.size() << " (max id=" << maxId << ")");
  NS_LOG_INFO("Links in config: " << linkSpecs.size());

  // 创建节点：索引 0..maxId（0 占位）
  NodeContainer nodes; nodes.Create(maxId + 1);

  // 名称与坐标
  std::vector<bool> hasPos(maxId + 1, false);
  std::vector<Vector> pos(maxId + 1, Vector(0,0,0));
  for (const auto& n : nodeSpecs) {
    if (!n.name.empty()) { Names::Add (n.name, nodes.Get(n.id)); NS_LOG_INFO("Name node " << n.id << " as '" << n.name << "'"); }
    if (n.hasPos) { hasPos[n.id] = true; pos[n.id] = Vector(n.x, n.y, 0.0); NS_LOG_INFO("Preset position for node " << n.id << ": (" << n.x << "," << n.y << ")"); }
  }
  // 自动布局未给坐标的节点
  const double dx = 2.0, dy = 2.0; uint32_t col = 0, row = 0;
  for (uint32_t id = 1; id <= maxId; ++id) {
    if (nodeIds.count(id)==0) continue;
    if (!hasPos[id]) {
      pos[id] = Vector(col*dx, row*dy, 0.0);
      hasPos[id] = true;
      ++col; if (col >= 8) { col = 0; ++row; }
      NS_LOG_DEBUG("Auto position for node " << id << ": (" << pos[id].x << "," << pos[id].y << ")");
    }
  }

  // 协议栈
  InternetStackHelper internet; internet.Install(nodes);

  // 安装 Mobility（为索引 0..maxId 都放置一个坐标，未使用的放远处）
  MobilityHelper mob;
  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator>();
  allocator->Add(Vector(-10,-10,0)); // 0 占位
  for (uint32_t id = 1; id <= maxId; ++id) {
    if (nodeIds.count(id)==0) { allocator->Add(Vector(-50,-50,0)); continue; }
    allocator->Add(pos[id]);
  }
  mob.SetPositionAllocator(allocator);
  mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mob.Install(nodes);

  // 每条链路一个 /24
  Ipv4AddressHelper address; address.SetBase("10.0.0.0", "255.255.255.0");

  // 链路安装（保留原始方向 a->b）
  std::set<std::pair<uint32_t,uint32_t>> seen; // 去重（无向）
  std::map<std::pair<uint32_t,uint32_t>, IfRecord> ifMap;

  for (const auto& l : linkSpecs) {
    auto undirected = Key(l.a,l.b);
    if (seen.count(undirected)) { NS_LOG_WARN("Duplicate link spec " << l.a << "<->" << l.b << " ignored"); continue; }
    seen.insert(undirected);

    if (nodeIds.count(l.a)==0 || nodeIds.count(l.b)==0) {
      NS_LOG_WARN("Link " << l.a << "<->" << l.b << " references undefined node id; skip");
      continue;
    }

    // 计算距离与时延（若启用）
    double du = std::hypot(pos[l.a].x - pos[l.b].x, pos[l.a].y - pos[l.b].y); // 坐标距离（单位）
    double meters = du * meterPerUnit;
    double delaySecComputed = (meters / propSpeed) * delayFactor;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue(l.rate));
    if (delayByDist) {
      p2p.SetChannelAttribute("Delay", TimeValue(Seconds(delaySecComputed)));
    } else {
      p2p.SetChannelAttribute("Delay", StringValue(l.delay)); // 与旧 CSV 兼容
    }
    p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("100p"));

    // 原始方向：a 在前、b 在后 -> IP 地址 index 0 属于 a，index 1 属于 b
    NetDeviceContainer dev = p2p.Install(nodes.Get(l.a), nodes.Get(l.b));
    Ipv4InterfaceContainer ifc = address.Assign(dev);
    address.NewNetwork();

    std::string delayLabel = delayByDist ? FormatTime(delaySecComputed) : l.delay;
    std::cout << "[link] " << l.a << "<->" << l.b
              << "  rate=" << l.rate
              << "  delay=" << delayLabel;
    if (delayByDist) {
      std::cout << "  dist=" << std::fixed << std::setprecision(3) << du
                << " units (" << std::setprecision(1) << meters << " m)";
    }
    std::cout << "  " << ifc.GetAddress(0) << " <-> " << ifc.GetAddress(1) << std::endl;

    if (enablePcap) {
      std::ostringstream os; os << "pcap-" << l.a << "-" << l.b;
      p2p.EnablePcapAll(os.str(), true);
    }

    IfRecord rec; rec.a = l.a; rec.b = l.b; rec.ifc = ifc;
    rec.rate = l.rate; rec.delay = delayLabel;
    rec.distanceUnits = du; rec.distanceMeters = meters;
    ifMap[undirected] = rec;
  }

  if (ifMap.empty()) { NS_FATAL_ERROR("No valid links created."); }

  // 选择服务器子网：优先 (13,14)，否则回退第一条；保持原始方向
  std::pair<uint32_t,uint32_t> serverKey;
  IfRecord serverRec;

  auto k1314 = Key(13,14);
  if (ifMap.count(k1314)) {
    serverKey = k1314; serverRec = ifMap[k1314];
    NS_LOG_INFO("Using (13,14) subnet for server address.");
  } else {
    serverKey = ifMap.begin()->first; serverRec = ifMap.begin()->second;
    NS_LOG_WARN("Link (13,14) not present; fallback to first link's subnet for server address.");
  }
  NS_LOG_INFO("Server link key = (" << serverKey.first << "," << serverKey.second
                                    << "), oriented as " << serverRec.a << "->" << serverRec.b << ")");

  // 路由
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  NS_LOG_INFO("Global routes populated.");

  // UDP Echo：server 放在“该链路的 b 端”，IP 取 index 1；client 取最小 id，若冲突则用 a 端
  uint32_t serverNodeId = serverRec.b;
  Ipv4Address serverAddr = serverRec.ifc.GetAddress(1);
  uint32_t clientNodeId = *nodeIds.begin();
  if (clientNodeId == serverNodeId) clientNodeId = serverRec.a;

  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  auto serverApps = echoServer.Install(nodes.Get(serverNodeId));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop (Seconds(stopTime - 1.0));

  UdpEchoClientHelper echoClient(serverAddr, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(40));
  echoClient.SetAttribute("Interval",   TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  auto clientApps = echoClient.Install(nodes.Get(clientNodeId));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop (Seconds(stopTime - 2.0));

  NS_LOG_INFO("Echo flow: client node " << clientNodeId
               << " -> server node " << serverNodeId
               << " @ " << serverAddr);

  // NetAnim：高亮 server/client
  if (enableAnim) {
    AnimationInterface anim(animXml);
    for (uint32_t id : nodeIds) {
      auto n = nodes.Get(id);
      std::ostringstream label;
      auto nm = Names::FindName(n);
      if (nm.empty()) label << id;
      else label << id << ":" << nm;
      anim.UpdateNodeDescription(n, label.str());

      if (id == serverNodeId)      anim.UpdateNodeColor(n, 200, 30, 30);   // red
      else if (id == clientNodeId) anim.UpdateNodeColor(n, 30, 180, 80);   // green
      else                         anim.UpdateNodeColor(n, 30, 100, 200);  // blue
    }
    NS_LOG_INFO("NetAnim written: " << animXml);
  }

  // FlowMonitor
  FlowMonitorHelper fmh;
  Ptr<FlowMonitor> monitor = fmh.InstallAll();

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmh.GetClassifier());
  auto stats = monitor->GetFlowStats();

  std::ofstream csv;
  if (!statsCsv.empty()) {
    csv.open(statsCsv);
    csv << "flowId,src,dst,proto,sPort,dPort,tx,rx,lost,throughput_Mbps,avgDelay_ms,avgJitter_ms\n";
  }

  std::cout << "\n========== FlowMonitor per-flow statistics ==========\n";
  double sumThr=0.0,sumDelay=0.0,sumJit=0.0; uint32_t rxFlows=0;

  for (const auto& kv : stats) {
    FlowId id = kv.first; const auto& st = kv.second;
    auto t = classifier->FindFlow(id);

    double dur = st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds();
    if (dur <= 0.0) dur = 1e-9;
    double thr = st.rxBytes * 8.0 / dur / 1e6;
    double dly = (st.rxPackets>0)? (st.delaySum.GetSeconds()/st.rxPackets*1000.0) : 0.0;
    double jit = (st.rxPackets>1)? (st.jitterSum.GetSeconds()/(st.rxPackets-1)*1000.0) : 0.0;

    std::cout << "Flow " << id << "  " << t.sourceAddress << " -> " << t.destinationAddress
              << "  proto=" << (uint32_t)t.protocol
              << " sPort=" << t.sourcePort << " dPort=" << t.destinationPort << "\n"
              << "  TxPkts=" << st.txPackets
              << " RxPkts=" << st.rxPackets
              << " Lost="  << st.lostPackets
              << "  Throughput=" << std::fixed << std::setprecision(3) << thr << " Mbps"
              << "  AvgDelay="    << std::setprecision(3) << dly << " ms"
              << "  AvgJitter="   << std::setprecision(3) << jit << " ms\n";

    if (!statsCsv.empty()) {
      csv << id << "," << t.sourceAddress << "," << t.destinationAddress << ","
          << (uint32_t)t.protocol << "," << t.sourcePort << "," << t.destinationPort << ","
          << st.txPackets << "," << st.rxPackets << "," << st.lostPackets << ","
          << std::fixed << std::setprecision(6) << thr << ","
          << dly << "," << jit << "\n";
    }
    if (st.rxPackets>0) { ++rxFlows; sumThr+=thr; sumDelay+=dly; sumJit+=jit; }
  }
  if (!statsCsv.empty()) csv.close();

  if (rxFlows>0) {
    std::cout << "\n--- Aggregated over " << rxFlows << " received flows ---  "
              << "MeanThr=" << (sumThr/rxFlows) << " Mbps  "
              << "MeanDelay=" << (sumDelay/rxFlows) << " ms  "
              << "MeanJitter=" << (sumJit/rxFlows) << " ms\n";
  }

  monitor->SerializeToXmlFile(flowmonXml, true, true);
  NS_LOG_INFO("FlowMonitor XML written: " << flowmonXml);

  // Graphviz 可视化导出
  if (!dotPath.empty()) {
    WriteGraphvizDot(dotPath, nodeIds, pos, ifMap, dotScale, clientNodeId, serverNodeId);
  }

  Simulator::Destroy();
  std::cout << "\nDone.\n";
  return 0;
}
