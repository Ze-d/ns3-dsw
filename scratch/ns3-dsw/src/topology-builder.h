#pragma once

#include "dsw-structures.h"

#include <string>
#include <set>
#include <map>

#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/point-to-point-helper.h"

namespace ns3 {

class TopologyBuilder
{
public:
    // 拓扑构建结果
    struct BuildResult
    {
        NodeContainer nodes;                                   // 所有节点
        std::map<std::pair<uint32_t, uint32_t>, IfRecord> ifMap; // 接口映射
        std::vector<Vector> positions;                          // 节点位置
        std::set<uint32_t> nodeIds;                            // 节点ID集合
        NetDeviceContainer p2pDevices;                         // P2P设备容器
    };

    // 构建拓扑
    // nodeSpecs: 节点规范列表
    // linkSpecs: 链路规范列表
    // maxId: 最大节点ID
    // delayByDist: 是否按距离计算时延
    // meterPerUnit: 每单位距离的米数
    // propSpeed: 传播速度
    // delayFactor: 时延因子
    // enablePcap: 是否启用 pcap
    static BuildResult BuildTopology(const std::vector<NodeSpec>& nodeSpecs,
                                    const std::vector<LinkSpec>& linkSpecs,
                                    uint32_t maxId,
                                    bool delayByDist = true,
                                    double meterPerUnit = 50000.0,
                                    double propSpeed = 2e8,
                                    double delayFactor = 1.0,
                                    bool enablePcap = false);
};

} // namespace ns3
