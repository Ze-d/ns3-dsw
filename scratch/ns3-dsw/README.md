run script
sh scratch/ns3-dsw/scripts/run.sh 
# 1.2 渲染图片
dot -Tpng scratch/ns3-dsw/out/topo.dot -o scratch/ns3-dsw/out/topo.png
# 1.3 动态图片【todo】
NetAnim 载入文件：scratch/topo_figure.xml