#!/usr/bin/env bash

# 解析命令行参数
ENABLE_PRICE_SCHEDULING=0
QUEUE_PENALTY_FACTOR=0.1

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
        *)
            shift
            ;;
    esac
done

./ns3 build

# 构建命令参数
CMD="topo_figure_flowmon_cfg_integrated \
--nodes=scratch/ns3-dsw/data/nodes.csv \
--links=scratch/ns3-dsw/data/links.csv \
--warmupTime=6 \
--simDuration=24 \
--pcap=0 \
--anim=1 \
--log=info \
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
--queuePenaltyFactor=$QUEUE_PENALTY_FACTOR"

echo "Running simulation with Price-Aware Scheduling: $ENABLE_PRICE_SCHEDULING"
echo "Queue Penalty Factor: $QUEUE_PENALTY_FACTOR"
echo ""

./ns3 run "$CMD"