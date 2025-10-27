#include "dswutils.h"

#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"

#include <algorithm>
#include <cctype>
#include <cmath> // hypot
#include <iomanip>
#include <sstream>
using namespace ns3;

// Trim 字符串前后空格
std::string
DswUtils::Trim(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b])))
        ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])))
        --e;
    return s.substr(b, e - b);
}

// 生成无向键
std::pair<uint32_t, uint32_t>
DswUtils::Key(uint32_t a, uint32_t b)
{
    return std::minmax(a, b);
}

// 格式化时间
std::string
DswUtils::FormatTime(double sec)
{
    std::ostringstream os;
    os.setf(std::ios::fixed);
    if (sec < 1e-6)
    {
        os << std::setprecision(3) << (sec * 1e9) << "ns";
    }
    else if (sec < 1e-3)
    {
        os << std::setprecision(3) << (sec * 1e6) << "us";
    }
    else if (sec < 1.0)
    {
        os << std::setprecision(3) << (sec * 1e3) << "ms";
    }
    else
    {
        os << std::setprecision(3) << sec << "s";
    }
    return os.str();
}

// 获取主用的 IPv4 地址（非 loopback 和 0 地址）
Ipv4Address
DswUtils::GetPrimaryIpv4Address(Ptr<Node> node)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (!ipv4)
        return Ipv4Address::GetZero();
    const uint32_t nIf = ipv4->GetNInterfaces();
    for (uint32_t i = 0; i < nIf; ++i)
    {
        const uint32_t nAddr = ipv4->GetNAddresses(i);
        for (uint32_t j = 0; j < nAddr; ++j)
        {
            Ipv4InterfaceAddress ifa = ipv4->GetAddress(i, j);
            Ipv4Address a = ifa.GetLocal();
            if (a != Ipv4Address("127.0.0.1") && a != Ipv4Address::GetZero())
            {
                return a;
            }
        }
    }
    return Ipv4Address::GetZero();
}

// 打印目标地址（仅支持 InetSocketAddress）
void
DswUtils::PrettyInetTarget(const Address& target, std::string& ip, uint16_t& port)
{
    ip = "unknown";
    port = 0;
    if (InetSocketAddress::IsMatchingType(target))
    {
        auto isa = InetSocketAddress::ConvertFrom(target);
        std::ostringstream os;
        os << isa.GetIpv4();
        ip = os.str();
        port = isa.GetPort();
    }
}
