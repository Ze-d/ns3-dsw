#!/usr/bin/env bash

# 解析命令行参数
ENABLE_PRICE_SCHEDULING=1
QUEUE_PENALTY_FACTOR=0.1
LOG_LEVEL="all"

while [[ $# -gt 0 ]]; do
    case $1 in
        --enablePriceAwareScheduling)
            ENABLE_PRICE_SCHEDULING="$2"
            shift 2
            ;;
        --queuePenaltyFactor)
            QUEUE_PENALTY_FACTOR="$2"
            shift 2
            ;;
        --logLevel)
            LOG_LEVEL="$2"
            shift 2
            ;;
        --disableLogs)
            LOG_LEVEL="off"
            shift
            ;;
        *)
            shift
            ;;
    esac
done

./ns3 build
mkdir -p scratch/ns3-dsw/out

# 构建命令参数
CMD="topo_figure_flowmon_cfg_integrated \
--nodes=scratch/ns3-dsw/data/nodes.csv \
--links=scratch/ns3-dsw/data/links.csv \
--warmupTime=6 \
--simDuration=24 \
--pcap=0 \
--anim=1 \
--flowXml=scratch/ns3-dsw/out/flowmon.xml \
--statsCsv=scratch/ns3-dsw/out/flowstats.csv \
--animXml=scratch/ns3-dsw/out/topo_figure.xml \
--dot=scratch/ns3-dsw/out/topo.dot \
--dotScale=80 \
--delayByDist=1 \
--meterPerUnit=50000 \
--propSpeed=2e8 \
--delayFactor=1.0 \
--simulationStep=1.0 \
--proAppUpdateInterval=0.1 \
--proSinkXml=scratch/ns3-dsw/out/pro_sink_stats.xml \
--enableLinkUtil=1 \
--linkUtilInterval=0.1 \
--enablePowerCoupling=1 \
--priceCsv=scratch/ns3-dsw/data/daily_price.csv \
--powerCostXmlBase=scratch/ns3-dsw/out/power_cost \
--enablePriceAwareScheduling=$ENABLE_PRICE_SCHEDULING \
--queuePenaltyFactor=$QUEUE_PENALTY_FACTOR \
--log=$LOG_LEVEL"

echo "================================================"
echo "动态成本感知调度仿真 (Dynamic Cost-Aware Scheduling)"
echo "================================================"
echo "Price-Aware Scheduling: $ENABLE_PRICE_SCHEDULING"
echo "Queue Penalty Factor: $QUEUE_PENALTY_FACTOR"
echo "Log Level: $LOG_LEVEL"

if [ "$LOG_LEVEL" != "off" ]; then
    echo ""
    echo "日志输出已启用！你将看到："
    echo "  - TTTT/RTT 测量结果"
    echo "  - 调度决策过程（成本对比）"
    echo "  - 连接切换日志"
    echo "  - 网络探测结果"
    echo ""
    echo "查看调度详细日志："
    echo "  ./ns3 run \"\$CMD\" 2>&1 | grep -E '(Scheduling|Connection Switch|TTTT|RTT)'"
    echo ""
fi

echo "================================================"
echo ""

# 根据日志级别设置环境变量
if [ "$LOG_LEVEL" = "off" ]; then
    # 完全禁用所有日志输出以提高速度
    NS_LOG_DISABLE=all ./ns3 run "$CMD" > /dev/null 2>&1
elif [ "$LOG_LEVEL" = "all" ]; then
    # 启用所有日志，包括我们新增的调度器日志
    NS_LOG_ENABLE=all ./ns3 run "$CMD" 2>&1
else
    # 使用指定的日志级别
    NS_LOG_COMPONENTS="$LOG_LEVEL" ./ns3 run "$CMD" 2>&1
fi