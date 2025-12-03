#include "visualization-config.h"

#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/netanim-module.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

NS_LOG_COMPONENT_DEFINE("VisualizationConfig");

namespace ns3 {

void
VisualizationConfig::SetupLogging(const std::string& levelStr)
{
    std::string s = levelStr;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "off")
        return;
    LogLevel lv = LOG_LEVEL_INFO;
    if (s == "warn")
        lv = LOG_LEVEL_WARN;
    if (s == "info")
        lv = LOG_LEVEL_INFO;
    if (s == "debug")
        lv = LOG_LEVEL_DEBUG;
    if (s == "all")
        lv = LOG_LEVEL_ALL;
    LogComponentEnable("TopoFigureFlowmonCfg", lv);
    LogComponentEnable("ProSinkApp", lv); // 启用新 App 的日志
    LogComponentEnable("LinkUtilizationMonitor", lv); // 启用链路监控日志
    LogComponentEnable("PriceAwareScheduler", lv);
    NS_LOG_INFO("Logging level set to: " << s);
}

void
VisualizationConfig::WriteGraphvizDot(const std::string& path,
                                     const std::set<uint32_t>& nodeIds,
                                     const std::vector<Vector>& pos,
                                     const std::map<std::pair<uint32_t, uint32_t>, IfRecord>& ifMap,
                                     double scale)
{
    std::ofstream dot(path.c_str());
    if (!dot.is_open())
    {
        NS_LOG_WARN("Cannot open dot path for write: " << path);
        return;
    }
    dot << "graph topo {\n";
    dot << "  layout=neato;\n  overlap=false;\n  splines=true;\n";
    dot << "  node [shape=circle, style=filled, fontname=\"Helvetica\"];\n\n";

    for (uint32_t id : nodeIds)
    {
        double X = pos[id].x * scale;
        double Y = pos[id].y * scale;
        std::ostringstream label;
        label << id;
        std::string color = "#1f77b4"; // 默认蓝
        dot << "  n" << id << " [label=\"" << label.str() << "\", pos=\"" << X << "," << Y
            << "!\", pin=true, fillcolor=\"" << color << "\"];\n";
    }
    dot << "\n";
    for (const auto& kv : ifMap)
    {
        const auto& undirected = kv.first;
        const auto& rec = kv.second;
        dot << "  n" << undirected.first << " -- n" << undirected.second << " [label=\"" << rec.rate
            << " / " << rec.delay << "\", id=\"link" << rec.id << "\", penwidth=2];\n";
    }
    dot << "}\n";
    dot.close();
    std::cout << "[viz] Graphviz .dot written: " << path << std::endl;
}

void
VisualizationConfig::SetupNetAnim(bool enableAnim,
                                 const std::string& animXml,
                                 const std::set<uint32_t>& nodeIds,
                                 const NodeContainer& nodes,
                                 const std::map<uint32_t, NodeSpec>& nodeSpecMap)
{
    if (enableAnim)
    {
        AnimationInterface anim(animXml);
        for (uint32_t id : nodeIds)
        {
            auto n = nodes.Get(id);
            std::ostringstream label;
            auto nm = Names::FindName(n);
            if (nm.empty())
                label << id;
            else
                label << id << ":" << nm;
            anim.UpdateNodeDescription(n, label.str());

            // 根据新类型为节点着色
            if (nodeSpecMap.count(id))
            {
                if (nodeSpecMap.at(id).type == NodeType::PRODUCER)
                {
                    anim.UpdateNodeColor(n, 255, 0, 0); // 红色 (Producer)
                }
                else if (nodeSpecMap.at(id).type == NodeType::CONSUMER)
                {
                    anim.UpdateNodeColor(n, 0, 0, 255); // 蓝色 (Consumer)
                }
                else
                {
                    anim.UpdateNodeColor(n, 100, 100, 100); // 灰色 (Router)
                }
            }
        }
        NS_LOG_INFO("NetAnim written: " << animXml);
    }
}

} // namespace ns3
