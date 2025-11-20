#!/bin/bash
# ns3-dsw 仿真结果分析与可视化脚本
# 支持指定文件夹进行分析
# 使用方法: bash scripts/run_analization.sh [文件夹名称或路径]

# ==========================================
# 颜色定义
# ==========================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ==========================================
# 环境准备
# ==========================================

# 显示标题
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} ns3-dsw 仿真结果分析与可视化${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 检查Python是否可用
if ! command -v python3 &> /dev/null && [ ! -f /usr/bin/python3 ]; then
    echo -e "${RED}错误: 未找到 python3，请先安装 Python 3${NC}"
    exit 1
fi

# 切换到脚本目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")" # 项目根目录

# ==========================================
# 核心逻辑：智能路径解析
# ==========================================
DEFAULT_BASE_DIR="$PROJECT_ROOT/out"
INPUT_TARGET="$1"

# 如果没有提供参数，默认使用 ../out (兼容旧行为)
if [ -z "$INPUT_TARGET" ]; then
    echo -e "${YELLOW}未指定目标文件夹，默认分析根目录 out/${NC}"
    DATA_DIR="$DEFAULT_BASE_DIR"
else
    # 1. 检查是否为直接路径 (绝对路径或当前目录下的相对路径)
    if [ -d "$INPUT_TARGET" ]; then
        DATA_DIR=$(realpath "$INPUT_TARGET")
    # 2. 检查是否为 out 目录下的子文件夹名称
    elif [ -d "${DEFAULT_BASE_DIR}/${INPUT_TARGET}" ]; then
        DATA_DIR=$(realpath "${DEFAULT_BASE_DIR}/${INPUT_TARGET}")
    else
        echo -e "${RED}❌ 错误: 找不到目录: $INPUT_TARGET${NC}"
        echo -e "   请检查路径是否正确，或该文件夹是否存在于 $DEFAULT_BASE_DIR 下"
        exit 1
    fi
fi

echo -e "${YELLOW}📂 分析目标目录: $DATA_DIR${NC}"
echo ""

# ==========================================
# 文件完整性检查
# ==========================================
echo -e "${YELLOW}正在检查数据文件...${NC}"

REQUIRED_FILES=(
    "$DATA_DIR/node_util.xml"
    "$DATA_DIR/power_cost_node2.xml"
    "$DATA_DIR/power_cost_node6.xml"
    "$DATA_DIR/power_cost_node9.xml"
    "$DATA_DIR/link_util.xml"
    "$DATA_DIR/pro_sink_stats.xml"
    "$DATA_DIR/flowstats.csv"
)

MISSING_FILES=0
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}  ❌ 缺少: $(basename "$file")${NC}"
        MISSING_FILES=$((MISSING_FILES + 1))
    fi
done

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "\n${RED}错误: 缺少 $MISSING_FILES 个数据文件${NC}"
    echo -e "${YELLOW}目录 '$DATA_DIR' 数据不完整。${NC}"
    exit 1
fi

echo -e "${GREEN}✅ 所有数据文件都存在${NC}"
echo ""

# 为了方便 Python 脚本读取，我们这里设置一个环境变量 (作为双重保险)
export TARGET_ANALYSIS_DIR="$DATA_DIR"

# ========================================================================
# 第一步：提取任务延迟数据
# ========================================================================
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} 第一步：提取任务延迟数据${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 切换到项目根目录运行 Python
cd "$PROJECT_ROOT" || exit 1

# ⚠️ 注意：我们在命令最后传入了 "$DATA_DIR" 参数
if python3 analization/extract_task_latency.py "$DATA_DIR"; then
    echo ""
    echo -e "${GREEN}✅ 任务延迟数据提取完成！${NC}"
    echo ""
else
    echo -e "${RED}❌ 任务延迟数据提取失败${NC}"
    exit 1
fi

# ========================================================================
# 第二步：生成可视化图表
# ========================================================================
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} 第二步：生成可视化图表${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 确保可视化输出目录存在 (在目标数据文件夹内新建 visualization 文件夹)
mkdir -p "$DATA_DIR/visualization"

if python3 analization/generate_all_visualizations.py "$DATA_DIR"; then
    echo ""
    echo -e "${GREEN}✅ 可视化生成完成！${NC}"
    echo ""
    echo -e "${BLUE}生成的图表:${NC}"
    # 这里的 ls 路径也动态化了
    ls -lh "$DATA_DIR/visualization/"*.png 2>/dev/null | awk '{print "  📊 " $9 " (" $5 ")"}'
    echo ""
    echo -e "${BLUE}图表位置:${NC} $DATA_DIR/visualization/"
    echo ""
else
    echo -e "${RED}❌ 可视化生成失败${NC}"
    exit 1
fi

# ========================================================================
# 第三步：计算和显示KPI统计量
# ========================================================================
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE} 第三步：计算KPI统计量${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

echo -e "${YELLOW}正在计算基础KPI统计量...${NC}"
# 这里通过环境变量 OUT_DIR 传递 (兼容你的旧脚本逻辑)，同时也作为参数传递
OUT_DIR="$DATA_DIR" python3 analization/calculate_kpi.py "$DATA_DIR"

echo -e "\n${YELLOW}正在计算任务延迟时间序列...${NC}"
python3 analization/task_latency_timeseries.py "$DATA_DIR"

echo -e "\n${YELLOW}正在计算任务延迟KPI统计...${NC}"
python3 analization/calculate_task_latency_kpi.py "$DATA_DIR"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✅ 分析完成！${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${BLUE}总结:${NC}"
echo "  1. 数据来源: $DATA_DIR"
echo "  2. 可视化图表: $DATA_DIR/visualization/"
echo "  3. KPI统计量已计算完成"
echo ""