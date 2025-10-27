#!/usr/bin/env bash
./ns3 build

./ns3 run "topo_figure_flowmon_cfg_integrated \
  --nodes=scratch/ns3-dsw/data/nodes.csv \
  --links=scratch/ns3-dsw/data/links.csv \
  --delayByDist=1 \
  --meterPerUnit=50000 \
  --propSpeed=2e8 \
  --delayFactor=1.0 \
  --stop=25 \
  --anim=1 \
  --animXml=scratch/ns3-dsw/out/topo_figure.xml \
  --dot=scratch/ns3-dsw/out/topo.dot \
  --dotScale=80 \
  --statsCsv=scratch/ns3-dsw/out/flowstats.csv \
  --flowXml=scratch/ns3-dsw/out/flowmon.xml \
  --pcap=0 \
  --log=info \
  --consumers=2,6,9 \
  --simulationStep=1.0 \
  --proAppDuration=0.5 \
  --proSinkXml=scratch/ns3-dsw/out/pro_sink_stats.xml"
