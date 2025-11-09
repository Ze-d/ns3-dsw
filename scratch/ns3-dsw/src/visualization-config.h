#pragma once

#include "dsw-structures.h"
#include "ns3/node-container.h"

#include <string>
#include <set>
#include <map>

namespace ns3 {

class VisualizationConfig
{
public:
    // 配置日志级别
    // levelStr: off|warn|info|debug|all
    static void SetupLogging(const std::string& levelStr);

    // 导出 Graphviz .dot 文件
    // path: 输出文件路径
    // nodeIds: 节点ID集合
    // pos: 节点位置向量
    // ifMap: 接口映射表
    // scale: 坐标缩放因子
    static void WriteGraphvizDot(const std::string& path,
                                const std::set<uint32_t>& nodeIds,
                                const std::vector<Vector>& pos,
                                const std::map<std::pair<uint32_t, uint32_t>, IfRecord>& ifMap,
                                double scale);

    // 配置 NetAnim 可视化
    // enableAnim: 是否启用 NetAnim
    // animXml: NetAnim XML 输出文件路径
    // nodeIds: 节点ID集合
    // nodes: 节点容器
    // nodeSpecMap: 节点规范映射表
    static void SetupNetAnim(bool enableAnim,
                            const std::string& animXml,
                            const std::set<uint32_t>& nodeIds,
                            const NodeContainer& nodes,
                            const std::map<uint32_t, NodeSpec>& nodeSpecMap);
};

} // namespace ns3
