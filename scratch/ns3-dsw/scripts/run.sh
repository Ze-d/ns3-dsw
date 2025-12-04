#!/usr/bin/env bash

# ==========================================
# 配置区域
# ==========================================
AUTHOR_NAME="ns3"  # 你可以在这里修改作者名字
BASE_OUT_DIR="scratch/ns3-dsw/out"
DATA_DIR="scratch/ns3-dsw/data"

# ==========================================
# 参数解析
# ==========================================
ENABLE_PRICE_SCHEDULING=1
LOAD_DECAY_FACTOR=0.5
MAX_CONGESTION_PENALTY=0.5
CONGESTION_SENSITIVITY=2.0
MAX_PRODUCER_PENALTY=3.0
PRODUCER_SENSITIVITY=100.0
LOG_LEVEL="all"

# 新增：用于存储用户自定义的子文件夹名
CUSTOM_SUBDIR=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --outputDir)
            CUSTOM_SUBDIR="$2"
            shift 2
            ;;
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
            LOG_LEVEL="$2"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            shift
            ;;
    esac
done

# ==========================================
# 确定输出目录 (核心修改逻辑)
# ==========================================

# 如果用户没有指定自定义文件夹，则使用时间戳 (默认行为)
if [ -z "$CUSTOM_SUBDIR" ]; then
    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    # 为了防止时间戳重名，可以加个随机数后缀（可选）
    CURRENT_OUT_DIR="${BASE_OUT_DIR}/${TIMESTAMP}"
else
    # 如果用户指定了文件夹，直接使用
    # 注意：这里支持多级目录，例如 grid_results/test1
    CURRENT_OUT_DIR="${BASE_OUT_DIR}/${CUSTOM_SUBDIR}"
fi

# ==========================================
# 准备环境
# ==========================================

# 1. 定义关键变量
SIM_DURATION=24
INFO_FILE="${CURRENT_OUT_DIR}/run_info.txt"
LOG_FILE="${CURRENT_OUT_DIR}/console_output.log"

# 2. 确保输出目录存在
if [ ! -d "$CURRENT_OUT_DIR" ]; then
    echo "Creating output directory: $CURRENT_OUT_DIR"
    mkdir -p "$CURRENT_OUT_DIR"
    
    # 双重检查
    if [ ! -d "$CURRENT_OUT_DIR" ]; then
        echo "Error: Failed to create directory $CURRENT_OUT_DIR. Aborting."
        exit 1
    fi
fi

# 3. 编译
./ns3 build

# 4. 构建命令参数
CMD="topo_figure_flowmon_cfg_integrated \
--nodes=${DATA_DIR}/nodes.csv \
--links=${DATA_DIR}/links.csv \
--warmupTime=0 \
--simDuration=${SIM_DURATION} \
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
--burstEventsXmlBase=${CURRENT_OUT_DIR}/burst_events \
--enablePriceAwareScheduling=$ENABLE_PRICE_SCHEDULING \
--loadDecayFactor=$LOAD_DECAY_FACTOR \
--maxCongestionPenalty=$MAX_CONGESTION_PENALTY \
--congestionSensitivity=$CONGESTION_SENSITIVITY \
--maxProducerPenalty=$MAX_PRODUCER_PENALTY \
--producerSensitivity=$PRODUCER_SENSITIVITY \
--schedulerLogPath=${CURRENT_OUT_DIR}/scheduler_events.xml \
--log=$LOG_LEVEL"

# ==========================================
# 生成运行信息报告
# ==========================================
echo "Generating run info at: $INFO_FILE"

cat <<EOF > "$INFO_FILE"
==========================================================
NS-3 Simulation Run Report
==========================================================
Run ID (Timestamp) : ${TIMESTAMP:-Custom_Dir}
Run Date           : $(date)
Output Directory   : $CURRENT_OUT_DIR
Author             : $AUTHOR_NAME
System Info        : $(uname -sr)
Target Duration    : $SIM_DURATION seconds
----------------------------------------------------------
Simulation Parameters
----------------------------------------------------------
Price-Aware Scheduling : $ENABLE_PRICE_SCHEDULING
Load Decay Factor      : $LOAD_DECAY_FACTOR
Max Congestion Penalty : $MAX_CONGESTION_PENALTY
Congestion Sensitivity : $CONGESTION_SENSITIVITY
Max Producer Penalty   : $MAX_PRODUCER_PENALTY
Producer Sensitivity   : $PRODUCER_SENSITIVITY
Log Level              : $LOG_LEVEL
----------------------------------------------------------
CMD : ./ns3 run $CMD
EOF

# ==========================================
# 执行仿真
# ==========================================
echo "================================================"
echo "动态成本感知调度仿真 (Dynamic Cost-Aware Scheduling)"
echo "Output Folder: $CURRENT_OUT_DIR"
echo "================================================"

START_TIME=$(date +%s)

if [ "$LOG_LEVEL" = "off" ]; then
    NS_LOG_DISABLE=all ./ns3 run "$CMD" > "$LOG_FILE" 2>&1 &
else
    if [ "$LOG_LEVEL" = "all" ]; then
        export NS_LOG_ENABLE=all
    else
        export NS_LOG="$LOG_LEVEL"
    fi
    ./ns3 run "$CMD" > "$LOG_FILE" 2>&1 &
fi

# 1. 定义清理函数：恢复光标可见，恢复输入回显
cleanup() {
    if [ -n "$SIM_PID" ] && kill -0 $SIM_PID 2>/dev/null; then
        echo -e "\n\n[Cleanup] Detected Ctrl+C (SIGINT). Stopping NS-3 simulation (PID $SIM_PID)..." >&2
        # 发送 SIGINT 信号给 NS-3 进程
        kill $SIM_PID
        wait $SIM_PID 2>/dev/null # 等待其干净退出
        echo "[Cleanup] NS-3 process stopped." >&2
    fi
    tput cnorm   # 恢复光标 (cursor normal)
    stty echo    # 恢复输入回显
}

# 2. 捕捉信号：无论是脚本跑完还是用户按 Ctrl+C，都执行 cleanup
trap cleanup EXIT INT TERM

# 3. 初始化设置：隐藏光标，关闭回显
tput civis       # 隐藏光标 (cursor invisible)
stty -echo       # 关闭输入回显 (no echo)

SIM_PID=$!
BAR_WIDTH=40
echo "" 

while kill -0 $SIM_PID 2>/dev/null; do
    CURRENT_REAL_TIME=$(date +%s)
    ELAPSED_SEC=$((CURRENT_REAL_TIME - START_TIME))
    ELAPSED_MIN=$((ELAPSED_SEC / 60))
    ELAPSED_REM_SEC=$((ELAPSED_SEC % 60))
    
    # 尝试从日志读取进度
    CURRENT_TIME_STR=$(tail -n 20 "$LOG_FILE" 2>/dev/null | grep -oE "^[0-9]+(\.[0-9]+)?s:" | tail -n 1 | tr -d 's:')

    if [ -n "$CURRENT_TIME_STR" ]; then
        PERCENT=$(awk -v cur="$CURRENT_TIME_STR" -v tot="$SIM_DURATION" 'BEGIN { if (tot>0) printf "%.0f", (cur/tot)*100; else print 0 }')
        if [ "$PERCENT" -gt 100 ]; then PERCENT=100; fi

        FILL_LEN=$(awk -v p="$PERCENT" -v w="$BAR_WIDTH" 'BEGIN { printf "%.0f", (p/100)*w }')
        BAR=$(printf "%0.s#" $(seq 1 $FILL_LEN))
        EMPTY=$(printf "%0.s-" $(seq 1 $((BAR_WIDTH - FILL_LEN))))
        
        printf "\r\033[K[%s%s] %3s%% | Sim: %6ss | Real: %02d:%02d" "$BAR" "$EMPTY" "$PERCENT" "$CURRENT_TIME_STR" "$ELAPSED_MIN" "$ELAPSED_REM_SEC"
    # else
    #     printf "\r\033[K[Initializing...] Waiting for output... | Real: %02d:%02d" "$ELAPSED_MIN" "$ELAPSED_REM_SEC"
    fi
    sleep 1
done

wait $SIM_PID
EXIT_CODE=$?

END_TIME=$(date +%s)
TOTAL_SEC=$((END_TIME - START_TIME))
TOTAL_MIN=$((TOTAL_SEC / 60))
TOTAL_REM_SEC=$((TOTAL_SEC % 60))

echo "" 
echo "================================================"

if [ $EXIT_CODE -eq 0 ]; then
    echo "Run Finished Successfully."
else
    echo "Run Failed with exit code $EXIT_CODE."
    echo "Check log for errors: $LOG_FILE"
fi
echo "Results saved in: $CURRENT_OUT_DIR"
echo "================================================"