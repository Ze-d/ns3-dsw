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

#include "ns3/config.h" // 用于 Config::SetDefault
#include "ns3/string.h" // 用于 StringValue
#include "ns3/pro-sink-app.h" 

#include "dswutils.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TopoFigureFlowmonCfg");

static std::ofstream g_xmlFile;


// ----------------------------- 配置结构 -----------------------------
enum class NodeType {
    UNKNOWN,
    PRODUCER, // 生产者 (edge)
    CONSUMER  // 消费者 (core)
};

struct NodeSpec {
  uint32_t id = 0;
  bool hasPos = false;
  double x = 0.0, y = 0.0;
  std::string name;
  NodeType type = NodeType::UNKNOWN; //Producer or Consumer
  double appRate = 0.0;     //lambda or consumerRate
};

struct LinkSpec {
  uint32_t a = 0, b = 0;     // 原始方向（用于确定 IP 顺序）
  std::string rate;          // e.g., "100Mbps"
  uint32_t id = 0;           // 标识符（可从 CSV 读取，若缺省则自动分配）
};

// nodes.csv: id,x,y,name,rate
static std::vector<NodeSpec> LoadCsvNodes(const std::string& path) {
  std::vector<NodeSpec> out;
  std::ifstream fin(path.c_str());
  if (!fin.is_open()) { NS_FATAL_ERROR("Cannot open nodes file: " << path); }
  std::string line; uint32_t ln = 0;
  while (std::getline(fin, line)) {
    ++ln; std::string s = DswUtils::Trim(line);
    if (s.empty() || s[0]=='#') continue;

    std::stringstream ss(s);
    std::string fid, fx, fy, fname, frate;
    std::getline(ss, fid, ',');
    std::getline(ss, fx,  ',');
    std::getline(ss, fy,  ',');
    std::getline(ss, fname, ','); 
    std::getline(ss, frate);

    fid = DswUtils::Trim(fid); fx = DswUtils::Trim(fx); fy = DswUtils::Trim(fy);
    fname = DswUtils::Trim(fname); frate = DswUtils::Trim(frate);

    if (!std::all_of(fid.begin(), fid.end(), ::isdigit)) {
      if (ln==1) { NS_LOG_WARN("Skip header in nodes.csv: " << s); continue; }
      NS_LOG_WARN("Skip invalid node line " << ln << ": " << s);
      continue;
    }

    NodeSpec ns; ns.id = static_cast<uint32_t>(std::stoul(fid));
    if (!fx.empty() && !fy.empty()) { ns.hasPos = true; ns.x = std::stod(fx); ns.y = std::stod(fy); }
    ns.name = fname;

    //解析类型和速率
    try {
      if (fname.rfind("edge-", 0) == 0) {
        ns.type = NodeType::PRODUCER;
      } else if (fname.rfind("core-", 0) == 0) {
        ns.type = NodeType::CONSUMER;
      } else {
        NS_LOG_WARN("Skip node line " << ln << ": Invalid name '" << fname << "'. Must start with 'edge-' or 'core-'.");
        continue;
      }

      if (frate.empty()) {
        NS_LOG_WARN("Skip node line " << ln << ": Rate column is empty for node " << fid);
        continue;
      }
      ns.appRate = std::stod(frate); // 解析速率
      if (ns.appRate <= 0.0) {
        NS_LOG_WARN("Skip node line " << ln << ": Rate must be positive, got " << ns.appRate);
        continue;
      }
    } catch (const std::exception& e) {
      NS_LOG_WARN("Skip node line " << ln << ": Invalid rate value '" << frate << "' (" << e.what() << ")");
      continue;
    }

    if (ns.id == 0) { NS_LOG_WARN("Node id 0 is reserved. Skip line " << ln); continue; }
    out.push_back(ns);
  }
  return out;
}

// links.csv: a,b,rate[,id]
static std::vector<LinkSpec> LoadCsvLinks(const std::string& path) {
  std::vector<LinkSpec> links;
  std::ifstream fin(path.c_str());
  if (!fin.is_open()) { NS_FATAL_ERROR("Cannot open links file: " << path); }
  std::string line; uint32_t ln = 0;
  while (std::getline(fin, line)) {
    ++ln; std::string s = DswUtils::Trim(line);
    if (s.empty() || s[0]=='#') continue;

    std::stringstream ss(s);
    std::vector<std::string> cols;
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      cols.push_back(DswUtils::Trim(tok));
    }
    if (cols.size() < 3) {
      NS_LOG_WARN("Skip invalid link line " << ln << ": " << s);
      continue;
    }

    std::string fa = cols[0], fb = cols[1], fr = cols[2];

    if (!std::all_of(fa.begin(), fa.end(), ::isdigit) ||
        !std::all_of(fb.begin(), fb.end(), ::isdigit)) {
      if (ln==1) { NS_LOG_WARN("Skip header in links.csv: " << s); continue; }
      NS_LOG_WARN("Skip invalid link line " << ln << ": " << s);
      continue;
    }

    LinkSpec ls;
    ls.a = static_cast<uint32_t>(std::stoul(fa));
    ls.b = static_cast<uint32_t>(std::stoul(fb));
    ls.rate  = fr;

    // optional id column (now at index 3)
    if (cols.size() >= 4 && !cols[3].empty() &&
        std::all_of(cols[3].begin(), cols[3].end(), ::isdigit)) {
      ls.id = static_cast<uint32_t>(std::stoul(cols[3]));
    } else {
      ls.id = static_cast<uint32_t>(links.size() + 1);
    }

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
  LogComponentEnable("ProSinkApp", lv); // 启用新 App 的日志
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
  uint32_t id = 0;    // link id
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
        << " [label=\"" << rec.rate << " / " << rec.delay << "\", id=\"link" << rec.id << "\", penwidth=2];\n";
  }
  dot << "}\n";
  dot.close();
  std::cout << "[viz] Graphviz .dot written: " << path << std::endl;
}

// ----------------------------- XML Trace 回调 --------------------------------

/**
 * @brief 当 Sink 完成一个任务时（Trace 回调）
 * @param nodeId 节点 ID
 * @param taskId 任务 ID
 * @param totalCompleted 该节点累计完成的任务数
 */
void OnSinkTaskCompleted(uint32_t nodeId, uint32_t taskId, uint32_t totalCompleted)
{
    if (g_xmlFile.is_open()) {
        g_xmlFile << "  <Event type=\"SinkComplete\""
                  << " time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " nodeId=\"" << nodeId << "\""
                  << " taskId=\"" << taskId << "\""
                  << " totalCompleted=\"" << totalCompleted << "\"/>" << std::endl;
    }
}

/**
 * @brief 当 Producer 发送一个新任务时（Trace 回调）
 * @param nodeId 节点 ID
 * @param taskId 任务 ID (在 App 中是 totalTasksSent)
 * @param target 目标地址 (Address)
 */
void OnProducerTaskSent(uint32_t nodeId, uint32_t taskId, Address target)
{
    if (g_xmlFile.is_open()) {
        g_xmlFile << "  <Event type=\"ProducerSend\""
                  << " time=\"" << Simulator::Now().GetSeconds() << "\""
                  << " nodeId=\"" << nodeId << "\""
                  << " taskId=\"" << taskId << "\"" // 这是 totalTasksSent
                  << " targetIp=\"" << InetSocketAddress::ConvertFrom(target).GetIpv4() << "\""
                  << " targetPort=\"" << InetSocketAddress::ConvertFrom(target).GetPort() << "\"/>" << std::endl;
    }
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
  double stopTime = 20.0;                    // [修改] 变为 Echo App 的停止时间
  bool enablePcap = false;
  bool enableAnim = true;

  // 距离->时延控制（默认启用）
  bool   delayByDist = true;     // 1=按距离计算，0=用 CSV delay
  double meterPerUnit = 50000.0; // 1 坐标单位=多少米；默认 50 km，得到毫秒级时延
  double propSpeed    = 2e8;     // 传播速度 m/s（光纤近似）
  double delayFactor  = 1.0;     // 额外缩放系数

  // --- Pro-Sink App 参数 ---
  double simulationStepMs = 1.0;     // 默认步长 1ms
  double proAppDuration = 0.5;       // 默认运行 0.5s
  std::string proSinkXmlFile = "scratch/pro_sink_stats.xml"; // 默认 XML 输出文件名

  CommandLine cmd;
  cmd.AddValue("nodes",   "CSV of nodes: id[,x,y[,name]]", nodesCsv);
  cmd.AddValue("links",   "CSV of links: a,b,rate[,id]", linksCsv);
  cmd.AddValue("stop",    "Simulation stop time (s) for Echo Apps", stopTime); // echo
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

  // --- Pro-Sink App 命令行参数 ---
  cmd.AddValue("simulationStep", "Simulation step for Pro-Sink App (ms)", simulationStepMs);
  cmd.AddValue("proAppDuration", "Duration for Pro-Sink App (s)", proAppDuration);
  cmd.AddValue("proSinkXml", "Pro-Sink App XML output file", proSinkXmlFile);

  cmd.Parse(argc, argv);
  SetupLogging(logLevel);

  // 确保 XML 输出在 scratch 目录下
  if (proSinkXmlFile.rfind("scratch/ns3-dsw/out/", 0) != 0) { 
    proSinkXmlFile = "scratch/ns3-dsw/out/" + proSinkXmlFile;
  }

  // 解析消费者列表
  // --- 解析 Pro-Sink 时间参数 ---
  Time simulationStep = MilliSeconds(simulationStepMs);
  NS_LOG_INFO("Pro-Sink simulation step: " << simulationStep);
  NS_LOG_INFO("Pro-Sink duration: " << proAppDuration << "s");

  // 读取配置
  auto nodeSpecs = LoadCsvNodes(nodesCsv);
  auto linkSpecs = LoadCsvLinks(linksCsv);
  if (nodeSpecs.empty()) NS_FATAL_ERROR("No nodes parsed from " << nodesCsv);
  if (linkSpecs.empty()) NS_FATAL_ERROR("No links parsed from " << linksCsv);

  // --- 构建 NodeSpec 映射表 ---
  std::map<uint32_t, NodeSpec> nodeSpecMap;
  for (const auto& ns : nodeSpecs) {
      nodeSpecMap[ns.id] = ns;
  }

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
    auto undirected = DswUtils::Key(l.a,l.b);
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
      // delay CSV column removed — use a sensible default when not computing by distance
      p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    }
    p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("100p"));

    // 原始方向：a 在前、b 在后 -> IP 地址 index 0 属于 a，index 1 属于 b
    NetDeviceContainer dev = p2p.Install(nodes.Get(l.a), nodes.Get(l.b));
    Ipv4InterfaceContainer ifc = address.Assign(dev);
    address.NewNetwork();

    std::string delayLabel = delayByDist ? DswUtils::FormatTime(delaySecComputed) : std::string("1ms");
    std::cout << "[link] " << l.a << "<->" << l.b
              << "  id=" << l.id
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
    rec.id = l.id;
    ifMap[undirected] = rec;
  }

  if (ifMap.empty()) { NS_FATAL_ERROR("No valid links created."); }

  // --- 构建节点 ID 到 IP 的映射，并收集消费者地址 ---
  // Pro-Sink App 需要知道目标 IP 地址
  std::map<uint32_t, Ipv4Address> nodeIpMap;
  for (uint32_t nodeId : nodeIds) {
      Ptr<Node> node = nodes.Get(nodeId);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
      // 接口 0 是 Loopback (127.0.0.1)
      // 我们取接口 1 (假设是第一个 P2P 接口)
      if (ipv4->GetNInterfaces() > 1) { 
          Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal();
          nodeIpMap[nodeId] = addr;
      } else {
          NS_LOG_WARN("Node " << nodeId << " has no P2P interface? Skipping for IP map.");
      }
  }

 uint16_t proPort = 8080; // Pro-Sink App 使用的端口 (必须与 MySink::m_port 匹配)
  std::vector<Address> sinkAddresses; // 存储所有消费者的地址
  bool hasProducers = false; 

  // --- 遍历 nodeSpecs 收集消费者地址 ---
  for (const auto& ns : nodeSpecs) {
      if (ns.type == NodeType::CONSUMER) {
          if (nodeIpMap.count(ns.id)) {
              sinkAddresses.push_back(InetSocketAddress(nodeIpMap[ns.id], proPort));
              NS_LOG_INFO("Consumer " << ns.id << " (core) identified at IP: " << nodeIpMap[ns.id] << " with rate " << ns.appRate << " Tasks/s ");
          } else {
              NS_LOG_WARN("Specified consumer node " << ns.id << " not found or has no IP.");
          }
      } else if (ns.type == NodeType::PRODUCER) {
          hasProducers = true;
      }
  }

  if (sinkAddresses.empty() && hasProducers) {
      NS_FATAL_ERROR("Producers (edge nodes) exist, but no valid consumer (core nodes) addresses were found.");
  }

  // 选择服务器子网：优先 (13,14)，否则回退第一条；保持原始方向
  // (这是为 UDP Echo 准备的)
  std::pair<uint32_t,uint32_t> serverKey;
  IfRecord serverRec;

  auto k1314 = DswUtils::Key(13,14);
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
  serverApps.Stop (Seconds(stopTime - 1.0)); // [修改] 在原 stopTime 之前停止

  UdpEchoClientHelper echoClient(serverAddr, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(40));
  echoClient.SetAttribute("Interval",   TimeValue(Seconds(0.5)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));
  auto clientApps = echoClient.Install(nodes.Get(clientNodeId));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop (Seconds(stopTime - 2.0)); // [修改] 在原 stopTime 之前停止

  NS_LOG_INFO("Echo flow: client node " << clientNodeId
               << " -> server node " << serverNodeId
               << " @ " << serverAddr);

  // --- 安装 Pro-Sink 应用 ---
  double proAppStartTime = stopTime; // 在 Echo App 停止后开始
  double proAppStopTime = proAppStartTime + proAppDuration;

  // Pro-Sink 应用参数 (硬编码)
  uint32_t proTaskSize = 256 * 1024; // (Bytes)
  uint32_t proPacketSize = 1024;     // (Bytes)

  ApplicationContainer proApps;
  std::vector<Ptr<MyProducer>> producers;
  std::vector<Ptr<MySink>> sinks;

  // 遍历所有节点，安装 Producer 或 Sink
  for (uint32_t nodeId : nodeIds) {
      // 检查该节点是否在 nodes.csv 中定义过
      if (nodeSpecMap.count(nodeId) == 0) {
          NS_LOG_DEBUG("Node " << nodeId << " is router-only (in links.csv but not nodes.csv). Skipping Pro-Sink app.");
          continue;
      }
      
      const NodeSpec& ns = nodeSpecMap.at(nodeId);
      Ptr<Node> node = nodes.Get(nodeId);

      if (ns.type == NodeType::CONSUMER) {
          // 这是消费者 (Sink)
          Ptr<MySink> sinkApp = CreateObject<MySink>();
          sinkApp->Setup(ns.appRate, simulationStep); 
          node->AddApplication(sinkApp);
          sinkApp->SetStartTime(Seconds(proAppStartTime));
          sinkApp->SetStopTime(Seconds(proAppStopTime));
          proApps.Add(sinkApp);
          sinks.push_back(sinkApp);
      } else if (ns.type == NodeType::PRODUCER) {
          // 这是生产者 (Producer)
          if (sinkAddresses.empty()) {
              NS_LOG_WARN("Node " << nodeId << " (edge) is a producer, but no sinks are available. Skipping app installation.");
              continue;
          }
          Ptr<MyProducer> producerApp = CreateObject<MyProducer>();
          producerApp->Setup(sinkAddresses, ns.appRate, proTaskSize, proPacketSize, simulationStep); // <--- 使用 CSV 速率
          node->AddApplication(producerApp);
          producerApp->SetStartTime(Seconds(proAppStartTime));
          producerApp->SetStopTime(Seconds(proAppStopTime));
          proApps.Add(producerApp);
          producers.push_back(producerApp);
      }
      // (ns.type == UNKNOWN 的节点会被自动跳过)
  }
  NS_LOG_INFO("Installed " << sinks.size() << " consumers and " << producers.size() << " producers.");
  NS_LOG_INFO("Pro-Sink Apps will run from " << proAppStartTime << "s to " << proAppStopTime << "s.");


  // --- 打开 XML 文件并连接 Traces ---
  g_xmlFile.open(proSinkXmlFile);
  if (!g_xmlFile.is_open()) {
      NS_LOG_ERROR("Failed to open " << proSinkXmlFile << " for writing.");
  } else {
      g_xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
      g_xmlFile << "<ProSinkStats simulationStep=\"" << simulationStep << "\" duration=\"" << proAppDuration << "\">" << std::endl;
      
      // 连接 Sink Traces
      for (auto& sink : sinks) {
          sink->TraceConnectWithoutContext("TaskCompleted", MakeCallback(&OnSinkTaskCompleted));
      }
      // 连接 Producer Traces
      for (auto& producer : producers) {
          producer->TraceConnectWithoutContext("TaskSent", MakeCallback(&OnProducerTaskSent));
      }
  }

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

  // --- 设置总仿真停止时间 ---
  Simulator::Stop(Seconds(proAppStopTime)); // 停止时间取决于 Pro-Sink App
  NS_LOG_INFO("Simulation will stop at " << proAppStopTime << "s.");
  
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

  // --- 关闭 XML 文件 ---
  if (g_xmlFile.is_open()) {
      g_xmlFile << "</ProSinkStats>" << std::endl;
      g_xmlFile.close();
      std::cout << "[stats] Pro-Sink XML written: " << proSinkXmlFile << std::endl;
  }

  Simulator::Destroy();
  std::cout << "\nDone.\n";
  return 0;
}