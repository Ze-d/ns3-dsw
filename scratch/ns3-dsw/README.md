run script
sh scratch/ns3-dsw/scripts/run.sh 参数自己看

可视化
/media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/project/ns3-dsw/scratch/ns3-dsw/scripts/run_analization.sh path-to-out

eg. /media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/project/ns3-dsw/scratch/ns3-dsw/scripts/run_analization.sh /media/pw/e97fdd05-9516-4082-826b-eb44c3458a4c/Data/HR/project/ns3-dsw/scratch/ns3-dsw/out/20251120_160404

# 1.2 渲染图片
dot -Tpng scratch/ns3-dsw/out/topo.dot -o scratch/ns3-dsw/out/topo.png
# 1.3 动态图片【todo】
NetAnim 载入文件：scratch/topo_figure.xml