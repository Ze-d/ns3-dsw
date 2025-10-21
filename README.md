| 日期/版本        | 更新内容   | 更新人 |
| ---------------- | ---------- | ------ |
| 2025.10.17  V0.0 | 网络层仿真 | zjy    |
|                  |            |        |
|                  |            |        |

### 环境构建

### 编译构建

```bash
# 完整重构时，或者构建出现问题时【可选】
./ns3 clean
# 构建
./ns3 build
# 1.1 距离决定时延（毫秒级）+ 动画 + Graphviz + FlowMonitor CSV
./ns3 run "scratch/topo_figure_flowmon_cfg \
  --nodes=scratch/nodes.csv \
  --links=scratch/links.csv \
  --delayByDist=1 \
  --meterPerUnit=50000 \
  --propSpeed=2e8 \
  --delayFactor=1.0 \
  --stop=25 \
  --anim=1 \
  --animXml=scratch/topo_figure.xml \
  --dot=scratch/topo.dot \
  --dotScale=80 \
  --statsCsv=scratch/flowstats.csv \
  --flowXml=scratch/flowmon.xml \
  --pcap=0 \
  --log=info"
# 1.2 渲染图片
dot -Tpng scratch/topo.dot -o scratch/topo.png
# 1.3 动态图片【todo】
NetAnim 载入文件：scratch/topo_figure.xml
# 2.1 使用csv自带的delay
./ns3 run "scratch/topo_figure_flowmon_cfg \
  --nodes=scratch/nodes.csv \
  --links=scratch/links.csv \
  --delayByDist=0 \
  --stop=25 \
  --anim=1 \
  --animXml=scratch/topo_figure.xml \
  --dot=scratch/topo.dot \
  --statsCsv=scratch/flowstats.csv \
  --flowXml=scratch/flowmon.xml \
  --log=debug"


```

