#!/usr/bin/env bash

echo "========================================="
echo "价格感知调度器验证脚本"
echo "========================================="
echo ""

# 运行实验
echo "1. 运行对照组（不启用调度）..."
./ns3 run "topo_figure_flowmon_cfg_integrated \
  --nodes=scratch/ns3-dsw/data/nodes.csv \
  --links=scratch/ns3-dsw/data/links.csv \
  --simDuration=5 \
  --enablePriceAwareScheduling=0 \
  --flowXml=scratch/ns3-dsw/out/flowmon.xml \
  --statsCsv=scratch/ns3-dsw/out/flowstats.csv \
  --animXml=scratch/ns3-dsw/out/topo_figure.xml \
  --proSinkXml=scratch/ns3-dsw/out/pro_sink_stats.xml \
  --nodeUtilXml=scratch/ns3-dsw/out/node_util.xml \
  --linkUtilXml=scratch/ns3-dsw/out/link_util.xml \
  --powerCostXmlBase=scratch/ns3-dsw/out/power_cost" 2>&1 | tail -5

# 保存结果
cp scratch/ns3-dsw/out/pro_sink_stats.xml /tmp/baseline.xml

echo ""
echo "2. 运行实验组（启用调度）..."
./ns3 run "topo_figure_flowmon_cfg_integrated \
  --nodes=scratch/ns3-dsw/data/nodes.csv \
  --links=scratch/ns3-dsw/data/links.csv \
  --simDuration=5 \
  --enablePriceAwareScheduling=1 \
  --flowXml=scratch/ns3-dsw/out/flowmon.xml \
  --statsCsv=scratch/ns3-dsw/out/flowstats.csv \
  --animXml=scratch/ns3-dsw/out/topo_figure.xml \
  --proSinkXml=scratch/ns3-dsw/out/pro_sink_stats.xml \
  --nodeUtilXml=scratch/ns3-dsw/out/node_util.xml \
  --linkUtilXml=scratch/ns3-dsw/out/link_util.xml \
  --powerCostXmlBase=scratch/ns3-dsw/out/power_cost" 2>&1 | tail -5

# 保存结果
cp scratch/ns3-dsw/out/pro_sink_stats.xml /tmp/with-scheduler.xml

echo ""
echo "3. 分析结果..."
echo ""

# 检查任务分布
echo "=== 对照组任务分布 ==="
grep "EdgeSend" /tmp/baseline.xml | awk -F'TargetIp="' '{print $2}' | awk -F'"' '{print $1}' | sort | uniq -c

echo ""
echo "=== 实验组任务分布 ==="
grep "EdgeSend" /tmp/with-scheduler.xml | awk -F'TargetIp="' '{print $2}' | awk -F'"' '{print $1}' | sort | uniq -c

echo ""
echo "4. 验证完成！"
echo "   - 对照组结果：/tmp/baseline.xml"
echo "   - 实验组结果：/tmp/with-scheduler.xml"
echo ""
echo "检查队列长度是否更均衡："
echo "  grep 'CoreQueue' /tmp/with-scheduler.xml | tail -20"
