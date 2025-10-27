#ifndef DSWUTILS_H
#define DSWUTILS_H

#include "ns3/address.h"
#include "ns3/core-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/network-module.h"
#include "ns3/ptr.h"

#include <map>
#include <set>
#include <string>
#include <vector>
using namespace ns3;
class DswUtils
{
  public:
    // 工具函数
    static std::string Trim(const std::string& s);
    static std::pair<uint32_t, uint32_t> Key(uint32_t a, uint32_t b);
    static std::string FormatTime(double sec);
    static Ipv4Address GetPrimaryIpv4Address(Ptr<Node> node);
    static void PrettyInetTarget(const Address& target, std::string& ip, uint16_t& port);

}; // namespace ns3

#endif // DSWUTILS_H
