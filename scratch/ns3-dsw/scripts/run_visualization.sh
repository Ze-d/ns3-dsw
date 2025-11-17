#!/usr/bin/env bash
# ns3-dsw 可视化生成脚本
# 用于快速生成所有仿真结果图表
# 使用方法: bash scripts/run_visualization.sh

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 显示标题
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} ns3-dsw 仿真结果可视化生成器${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查Python是否可用
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}错误: 未找到 python3，请先安装 Python 3${NC}"
    exit 1
fi

# 切换到脚本目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

echo -e "${YELLOW}当前目录: $SCRIPT_DIR${NC}"
echo ""

# 检查数据文件
echo -e "${YELLOW}正在检查数据文件...${NC}"
DATA_DIR="$SCRIPT_DIR/../out"
REQUIRED_FILES=(
    "$DATA_DIR/node_util.xml"
    "$DATA_DIR/power_cost_node2.xml"
    "$DATA_DIR/power_cost_node6.xml"
    "$DATA_DIR/power_cost_node9.xml"
    "$DATA_DIR/link_util.xml"
    "$DATA_DIR/pro_sink_stats.xml"
)

MISSING_FILES=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}  ❌ 缺少: $file${NC}"
        MISSING_FILES=$((MISSING_FILES + 1))
    fi
done

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "\n${RED}错误: 缺少 $MISSING_FILES 个数据文件${NC}"
    echo -e "${YELLOW}请先运行仿真生成数据文件:${NC}"
    echo "  cd $SCRIPT_DIR/.."
    echo "  sh scripts/run.sh"
    exit 1
fi

echo -e "${GREEN}✅ 所有数据文件都存在${NC}"
echo ""

# 运行可视化脚本
echo -e "${YELLOW}开始生成可视化图表...${NC}"
echo ""

# 切换到项目根目录，然后运行可视化脚本
cd "$SCRIPT_DIR/.." || exit 1
if python3 analization/generate_all_visualizations.py; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✅ 可视化生成完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${BLUE}生成的图表:${NC}"
    ls -lh "$SCRIPT_DIR/../out/visualization/"*.png 2>/dev/null | awk '{print "  📊 " $9 " (" $5 ")"}'
    echo ""
    echo -e "${BLUE}图表位置:${NC} $SCRIPT_DIR/../out/visualization/"
    echo ""
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}❌ 可视化生成失败${NC}"
    echo -e "${RED}========================================${NC}"
    exit 1
fi
