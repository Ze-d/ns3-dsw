#!/bin/bash

# 这是一个git钩子脚本
echo "Running tests... (Placeholder - Success)"

# --- 1. 定义并创建专门的测试输出目录 ---
TEST_OUT_DIR="/tmp/ns3-dsw-test-output"
echo "Using test output directory: $TEST_OUT_DIR"
# -p 确保目录存在且不会因已存在而报错
mkdir -p "$TEST_OUT_DIR"
# 清理上一次的测试结果
rm -f "$TEST_OUT_DIR"/*


# -----------------------------------------------------------------
# 运行构建，如果成功 (&&)，则运行模拟
echo "Building ns3..."
# 使用POSIX兼容的方式获取脚本目录
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# 脚本在 scratch/ns3-dsw/scripts/，项目根目录需要上三级
PARENT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_ROOT="$(cd "$PARENT_DIR/.." && pwd)"
# 切换到项目根目录
cd "$PROJECT_ROOT" || exit 1
# 使用绝对路径调用ns3
"$PROJECT_ROOT/ns3" build && \
echo "Running ns3 simulation for test..." && \
"$PROJECT_ROOT/ns3" run "scratch/ns3-dsw/src/topo_figure_flowmon_cfg_integrated \
--nodes=scratch/ns3-dsw/data/nodes.csv \
--links=scratch/ns3-dsw/data/links.csv \
--warmupTime=0 \
--simDuration=0.1 \
--pcap=0 \
--anim=0 \
--flowXml=$TEST_OUT_DIR/test.flowmon.xml \
--statsCsv=$TEST_OUT_DIR/test.stats.csv \
--animXml=$TEST_OUT_DIR/test.anim.xml \
--proSinkXml=$TEST_OUT_DIR/test.proSink.xml \
--nodeUtilXml=$TEST_OUT_DIR/test.nodeUtil.xml \
--powerCostXmlBase=$TEST_OUT_DIR/test.powerCost \
--linkUtilXml=$TEST_OUT_DIR/test.linkUtil.xml \
"

# 捕获上面命令链的最终退出状态
# (注意: 我在上面添加了一个 'echo' 和 '&&'，以确保 'ns3 run' 是链条的一部分)
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
echo "Test output files are located in: $TEST_OUT_DIR"

touch /tmp/ns3-dsw-tests-pass

exit 0