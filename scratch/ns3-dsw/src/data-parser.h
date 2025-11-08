#pragma once

#include "dsw-structures.h"

#include <string>
#include <vector>

namespace ns3 {

class DataParser
{
public:
    // 加载电价数据 CSV 文件
    // 格式: hour,minute,price
    // 返回包含 288 个价格点的向量
    static std::vector<double> LoadCsvPrices(const std::string& path);

    // 加载节点配置 CSV 文件
    // 格式: id,x,y,name,rate[,baseP,fullP,phase]
    // 返回节点规范向量
    static std::vector<NodeSpec> LoadCsvNodes(const std::string& path);

    // 加载链路配置 CSV 文件
    // 格式: a,b,rate[,id]
    // 返回链路规范向量
    static std::vector<LinkSpec> LoadCsvLinks(const std::string& path);
};

} // namespace ns3
