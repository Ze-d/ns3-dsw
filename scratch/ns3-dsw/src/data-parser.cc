#include "data-parser.h"
#include "dswutils.h"

#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("DataParser");

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ns3 {

std::vector<double>
DataParser::LoadCsvPrices(const std::string& path)
{
    std::vector<double> prices;
    std::ifstream fin(path.c_str());
    if (!fin.is_open())
    {
        NS_FATAL_ERROR("Cannot open price file: " << path);
    }
    std::string line;
    uint32_t ln = 0;
    while (std::getline(fin, line))
    {
        ++ln;
        std::string s = DswUtils::Trim(line);
        if (s.empty() || s[0] == '#')
            continue;

        std::stringstream ss(s);
        std::string hour, minute, priceStr;

        // 1. 解析列
        std::getline(ss, hour, ',');
        std::getline(ss, minute, ',');
        std::getline(ss, priceStr);

        hour = DswUtils::Trim(hour);
        priceStr = DswUtils::Trim(priceStr);

        if (priceStr.empty())
        {
             NS_LOG_WARN("Skip price line " << ln << ": Price column is empty. Line: " << s);
             continue;
        }

        try
        {
            // 2. 转换第三列
            prices.push_back(std::stod(priceStr));
        }
        catch (const std::exception& e)
        {
            // 3. 检查是否为表头
            if (ln == 1 && (hour == "hour" || priceStr == "price"))
            {
                NS_LOG_WARN("Skip header in price.csv: " << s);
                continue;
            }
            NS_LOG_WARN("Skip invalid price line " << ln << ": " << s << " (" << e.what() << ")");
        }
    }
    NS_LOG_INFO("Loaded " << prices.size() << " price points from " << path);
    return prices;
}

// nodes.csv: id,x,y,name,rate[,baseP,fullP,phase]
std::vector<NodeSpec>
DataParser::LoadCsvNodes(const std::string& path)
{
    std::vector<NodeSpec> out;
    std::ifstream fin(path.c_str());
    if (!fin.is_open())
    {
        NS_FATAL_ERROR("Cannot open nodes file: " << path);
    }
    std::string line;
    uint32_t ln = 0;
    while (std::getline(fin, line))
    {
        ++ln;
        std::string s = DswUtils::Trim(line);
        if (s.empty() || s[0] == '#')
            continue;

        std::stringstream ss(s);
        std::string fid, fx, fy, fname, frate, fbaseP, ffullP, fphase;

        std::getline(ss, fid, ',');
        std::getline(ss, fx, ',');
        std::getline(ss, fy, ',');
        std::getline(ss, fname, ',');
        std::getline(ss, frate, ',');
        std::getline(ss, fbaseP, ',');
        std::getline(ss, ffullP, ',');
        std::getline(ss, fphase);


        fid = DswUtils::Trim(fid);
        fx = DswUtils::Trim(fx);
        fy = DswUtils::Trim(fy);
        fname = DswUtils::Trim(fname);
        frate = DswUtils::Trim(frate);
        fbaseP = DswUtils::Trim(fbaseP);
        ffullP = DswUtils::Trim(ffullP);
        fphase = DswUtils::Trim(fphase);

        if (!std::all_of(fid.begin(), fid.end(), ::isdigit))
        {
            if (ln == 1)
            {
                NS_LOG_WARN("Skip header in nodes.csv: " << s);
                continue;
            }
            NS_LOG_WARN("Skip invalid node line " << ln << ": " << s);
            continue;
        }

        NodeSpec ns;
        ns.id = static_cast<uint32_t>(std::stoul(fid));
        if (!fx.empty() && !fy.empty())
        {
            ns.hasPos = true;
            ns.x = std::stod(fx);
            ns.y = std::stod(fy);
        }
        ns.name = fname;

        // 解析类型和速率
        try
        {
            if (fname.rfind("edge-", 0) == 0)
            {
                ns.type = NodeType::PRODUCER;
            }
            else if (fname.rfind("core-", 0) == 0)
            {
                ns.type = NodeType::CONSUMER;
            }
            else
            {
                NS_LOG_WARN("Skip node line " << ln << ": Invalid name '" << fname
                                              << "'. Must start with 'edge-' or 'core-'.");
                continue;
            }

            if (frate.empty())
            {
                NS_LOG_WARN("Skip node line " << ln << ": Rate column is empty for node " << fid);
                continue;
            }
            ns.appRate = std::stod(frate); // 解析速率
            if (ns.appRate <= 0.0)
            {
                NS_LOG_WARN("Skip node line " << ln << ": Rate must be positive, got "
                                              << ns.appRate);
                continue;
            }

            if (ns.type == NodeType::CONSUMER)
            {
                if (!fbaseP.empty()) ns.basePower = std::stod(fbaseP);
                if (!ffullP.empty()) ns.fullPower = std::stod(ffullP);
                if (!fphase.empty()) ns.phaseOffset = std::stod(fphase);
            }

        }
        catch (const std::exception& e)
        {
            NS_LOG_WARN("Skip node line " << ln << ": Invalid numeric value in '" << s << "' ("
                                          << e.what() << ")");
            continue;
        }

        if (ns.id == 0)
        {
            NS_LOG_WARN("Node id 0 is reserved. Skip line " << ln);
            continue;
        }
        out.push_back(ns);
    }
    return out;
}

// links.csv: a,b,rate[,id]
std::vector<LinkSpec>
DataParser::LoadCsvLinks(const std::string& path)
{
    std::vector<LinkSpec> links;
    std::ifstream fin(path.c_str());
    if (!fin.is_open())
    {
        NS_FATAL_ERROR("Cannot open links file: " << path);
    }
    std::string line;
    uint32_t ln = 0;
    while (std::getline(fin, line))
    {
        ++ln;
        std::string s = DswUtils::Trim(line);
        if (s.empty() || s[0] == '#')
            continue;

        std::stringstream ss(s);
        std::vector<std::string> cols;
        std::string tok;
        while (std::getline(ss, tok, ','))
        {
            cols.push_back(DswUtils::Trim(tok));
        }
        if (cols.size() < 3)
        {
            NS_LOG_WARN("Skip invalid link line " << ln << ": " << s);
            continue;
        }

        std::string fa = cols[0], fb = cols[1], fr = cols[2];

        if (!std::all_of(fa.begin(), fa.end(), ::isdigit) ||
            !std::all_of(fb.begin(), fb.end(), ::isdigit))
        {
            if (ln == 1)
            {
                NS_LOG_WARN("Skip header in links.csv: " << s);
                continue;
            }
            NS_LOG_WARN("Skip invalid link line " << ln << ": " << s);
            continue;
        }

        LinkSpec ls;
        ls.a = static_cast<uint32_t>(std::stoul(fa));
        ls.b = static_cast<uint32_t>(std::stoul(fb));
        ls.rate = fr;

        // optional id column (now at index 3)
        if (cols.size() >= 4 && !cols[3].empty() &&
            std::all_of(cols[3].begin(), cols[3].end(), ::isdigit))
        {
            ls.id = static_cast<uint32_t>(std::stoul(cols[3]));
        }
        else
        {
            ls.id = static_cast<uint32_t>(links.size() + 1);
        }

        if (ls.a == 0 || ls.b == 0 || ls.a == ls.b)
        {
            NS_LOG_WARN("Skip invalid/self-loop link at line " << ln << ": " << s);
            continue;
        }
        links.push_back(ls);
    }
    return links;
}

} // namespace ns3
