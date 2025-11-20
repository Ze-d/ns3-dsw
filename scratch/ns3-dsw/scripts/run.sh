#!/usr/bin/env bash

# ==========================================
# 配置区域
# ==========================================
AUTHOR_NAME="Default"  # 你可以在这里修改作者名字
BASE_OUT_DIR="scratch/ns3-dsw/out"
DATA_DIR="scratch/ns3-dsw/data"

# 获取当前时间戳 (格式: YYYYMMDD_HHMMSS)
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
# 创建本次运行的专属输出目录
CURRENT_OUT_DIR="${BASE_OUT_DIR}/${TIMESTAMP}"

# ==========================================
# 参数解析
# ==========================================
ENABLE_PRICE_SCHEDULING=1
LOAD_DECAY_FACTOR=0.5
MAX_CONGESTION_PENALTY=0.5
CONGESTION_SENSITIVITY=2.0
MAX_PRODUCER_PENALTY=3.0
PRODUCER_SENSITIVITY=100.0
LOG_LEVEL="off"

while [[ $# -gt 0 ]]; do
    case $1 in
        --enablePriceAwareScheduling)
            ENABLE_PRICE_SCHEDULING="$2"
            shift 2
            ;;
        --loadDecayFactor)
            LOAD_DECAY_FACTOR="$2"
            shift 2
            ;;
        --maxCongestionPenalty)
            MAX_CONGESTION_PENALTY="$2"
            shift 2
            ;;
        --congestionSensitivity)
            CONGESTION_SENSITIVITY="$2"
            shift 2
            ;;
        --maxProducerPenalty)
            MAX_PRODUCER_PENALTY="$2"
            shift 2
            ;;
        --producerSensitivity)
            PRODUCER_SENSITIVITY="$2"
            shift 2
            ;;
        --logLevel)
            LOG_LEVEL="$2"
            shift 2
            ;;
        --disableLogs)
            LOG_LEVEL="off"
            shift
            ;;
        *)
            shift
            ;;
    esac
done

# ==========================================
# 准备环境
# ==========================================

# 确保输出目录存在
mkdir -p "$CURRENT_OUT_DIR"

# 编译 (如果有改动)
./ns3 build

# 构建命令参数
# 注意：所有的输出路径现在都指向 $CURRENT_OUT_DIR
CMD="topo_figure_flowmon_cfg_integrated \
--nodes=${DATA_DIR}/nodes.csv \
--links=${DATA_DIR}/links.csv \
--warmupTime=6 \
--simDuration=24 \
--pcap=0 \
--anim=1 \
--flowXml=${CURRENT_OUT_DIR}/flowmon.xml \
--statsCsv=${CURRENT_OUT_DIR}/flowstats.csv \
--animXml=${CURRENT_OUT_DIR}/topo_figure.xml \
--dot=${CURRENT_OUT_DIR}/topo.dot \
--dotScale=80 \
--delayByDist=1 \
--meterPerUnit=50000 \
--propSpeed=2e8 \
--delayFactor=1.0 \
--simulationStep=1.0 \
--proAppUpdateInterval=0.1 \
--proSinkXml=${CURRENT_OUT_DIR}/pro_sink_stats.xml \
--nodeUtilXml=${CURRENT_OUT_DIR}/node_util.xml \
--enableLinkUtil=1 \
--linkUtilInterval=0.1 \
--linkUtilXml=${CURRENT_OUT_DIR}/link_util.xml \
--enablePowerCoupling=1 \
--priceCsv=${DATA_DIR}/daily_price.csv \
--powerCostXmlBase=${CURRENT_OUT_DIR}/power_cost \
--enablePriceAwareScheduling=$ENABLE_PRICE_SCHEDULING \
--loadDecayFactor=$LOAD_DECAY_FACTOR \
--maxCongestionPenalty=$MAX_CONGESTION_PENALTY \
--congestionSensitivity=$CONGESTION_SENSITIVITY \
--maxProducerPenalty=$MAX_PRODUCER_PENALTY \
--producerSensitivity=$PRODUCER_SENSITIVITY \
--schedulerLogPath=${CURRENT_OUT_DIR}/scheduler_events.xml \
--log=$LOG_LEVEL"

# ==========================================
# 生成运行信息报告 (Run Info Report)
# ==========================================
INFO_FILE="${CURRENT_OUT_DIR}/run_info.txt"

echo "Generating run info at: $INFO_FILE"

cat <<EOF > "$INFO_FILE"
==========================================================
NS-3 Simulation Run Report
==========================================================
Run ID (Timestamp) : $TIMESTAMP
Run Date           : $(date)
Output Directory   : $CURRENT_OUT_DIR
Author             : $AUTHOR_NAME
System Info        : $(uname -sr)

----------------------------------------------------------
Simulation Parameters
----------------------------------------------------------
Price-Aware Scheduling : $ENABLE_PRICE_SCHEDULING
Load Decay Factor      : $LOAD_DECAY_FACTOR
Max Congestion Penalty : $MAX_CONGESTION_PENALTY
Congestion Sensitivity : $CONGESTION_SENSITIVITY (s)
Max Producer Penalty   : $MAX_PRODUCER_PENALTY
Producer Sensitivity   : $PRODUCER_SENSITIVITY (tasks)
Log Level              : $LOG_LEVEL

----------------------------------------------------------
Input Data Sources
----------------------------------------------------------
Nodes File : ${DATA_DIR}/nodes.csv
Links File : ${DATA_DIR}/links.csv
Price File : ${DATA_DIR}/daily_price.csv

----------------------------------------------------------
Full Execution Command
----------------------------------------------------------
./ns3 run "$CMD"

EOF

# ==========================================
# 终端输出与执行
# ==========================================
echo "================================================"
echo "动态成本感知调度仿真 (Dynamic Cost-Aware Scheduling)"
echo "================================================"
echo "Output Folder: $CURRENT_OUT_DIR"
echo "Run Info File: $INFO_FILE"
echo "Log Level:     $LOG_LEVEL"

if [ "$LOG_LEVEL" != "off" ]; then
    echo ""
    echo "提示：终端日志也将被保存到: ${CURRENT_OUT_DIR}/console_output.log"
fi
echo "================================================"
echo ""

# 定义日志文件路径
LOG_FILE="${CURRENT_OUT_DIR}/console_output.log"

# 执行仿真
# 使用 tee 命令：既在屏幕显示，又写入文件
# 2>&1 确保错误输出也能被捕获

if [ "$LOG_LEVEL" = "off" ]; then
    # 静默模式：只记录到文件，或者完全丢弃（此处选择记录到文件以便排查错误，但不输出到屏幕）
    NS_LOG_DISABLE=all ./ns3 run "$CMD" > "$LOG_FILE" 2>&1
    echo "仿真完成。日志已静默写入 $LOG_FILE"

elif [ "$LOG_LEVEL" = "all" ]; then
    # 启用所有日志
    NS_LOG_ENABLE=all ./ns3 run "$CMD" 2>&1 | tee "$LOG_FILE"

else
    # 指定日志级别
    NS_LOG_COMPONENTS="$LOG_LEVEL" ./ns3 run "$CMD" 2>&1 | tee "$LOG_FILE"
fi

echo ""
echo "================================================"
echo "Run Finished."
echo "Results saved in: $CURRENT_OUT_DIR"
echo "================================================"