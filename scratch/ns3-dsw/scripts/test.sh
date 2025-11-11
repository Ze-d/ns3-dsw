#!/bin/bash

# 这是一个占位符测试脚本
echo "Running tests... (Placeholder - Success)"

# -----------------------------------------------------------------
# 运行构建，如果成功 (&&)，则运行模拟
/home/hurun/project/ns3-dsw/ns3 build && \
/home/hurun/project/ns3-dsw/ns3 run "scratch/ns3-dsw/src/topo_figure_flowmon_cfg_integrated \
--nodes=scratch/ns3-dsw/data/nodes.csv \
--links=scratch/ns3-dsw/data/links.csv \
--warmupTime=0 \
--simDuration=0.1"

# 捕获上面命令链的最终退出状态
exit_status=$?

# -----------------------------------------------------------------

# 检查退出状态
if [ $exit_status -ne 0 ]; then
    # 如果任何一个命令失败（退出状态非 0），则报告失败并退出
    echo "Tests failed!"
    # 尝试删除标志文件，以防它存在
    rm -f /tmp/ns3-dsw-tests-pass
    exit 1
fi

# 只有当所有命令都成功（exit_status 为 0）时，才会执行到这里
echo "All tests passed!"

touch /tmp/ns3-dsw-tests-pass

exit 0