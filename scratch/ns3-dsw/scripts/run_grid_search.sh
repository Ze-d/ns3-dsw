#!/bin/bash

# ============================================================
# 动态成本感知调度 - 全网格参数扫描与自动分析脚本 (修复版)
# ============================================================

# --- 颜色定义 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# --- 1. 参数配置区域 ---

# [修复]：将 "key=value" 格式改为独立的元素 "key" "value"
# run.sh 使用 shift 2 解析，必须用空格(数组独立元素)分隔键和值
FIXED_PARAMS=(
    "--enablePriceAwareScheduling" "1" 
    "--maxProducerPenalty" "0.1" 
    "--producerSensitivity" "1000.0" 
    "--logLevel" "off"
)

# 扫描参数空间
decay_list=(0.0 0.05 0.1 0.15 0.2 0.3 0.5)     # 负载衰减
penalty_list=(0.05 0.1 0.2 0.4 0.6 0.8)        # 拥塞惩罚上限
sensitivity_list=(2.0 5.0 10.0)                # 拥塞敏感度

# 脚本与路径定义
PROJECT_BASE="scratch/ns3-dsw"
SCRIPTS_DIR="${PROJECT_BASE}/scripts"
RUN_SCRIPT="${SCRIPTS_DIR}/run.sh"
ANALYSIS_SCRIPT="${SCRIPTS_DIR}/run_analization.sh"
MASTER_LOG="grid_search_progress.log"

# --- 2. 初始化环境 ---

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}    全网格扫描任务启动 (Grid Search)    ${NC}"
echo -e "${BLUE}========================================${NC}"

# 检查 run.sh 是否存在
if [ ! -f "$RUN_SCRIPT" ]; then
    echo -e "${RED}[错误] 找不到执行脚本: $RUN_SCRIPT${NC}"
    echo "请确保您在 ns-3 根目录下运行此脚本。"
    exit 1
fi

# 确保 grid 父目录存在
mkdir -p "${PROJECT_BASE}/out/grid"

# 计算任务总量
total_tasks=$(( ${#decay_list[@]} * ${#penalty_list[@]} * ${#sensitivity_list[@]} ))
current_task=0
start_timestamp=$(date +%s)

echo "开始时间: $(date)" | tee $MASTER_LOG
echo "总任务数: $total_tasks" | tee -a $MASTER_LOG
echo "运行脚本: $RUN_SCRIPT" | tee -a $MASTER_LOG
echo ""

# --- 3. 循环扫描 ---

for sens in "${sensitivity_list[@]}"; do
    for pen in "${penalty_list[@]}"; do
        for decay in "${decay_list[@]}"; do
            
            # --- 计时与进度计算 ---
            current_task=$((current_task+1))
            loop_start_time=$(date +%s)
            
            # 估算剩余时间
            if [ $current_task -gt 1 ]; then
                avg_time_per   _task=$(( (loop_start_time - start_timestamp) / (current_task - 1) ))
                tasks_left=$((total_tasks - current_task + 1))
                secs_left=$((tasks_left * avg_time_per_task))
                mins_left=$((secs_left / 60))
                eta_str="${mins_left}m"
            else
                eta_str="计算中..."
            fi

            # --- 构建标识符 ---
            RUN_ID="D${decay}_P${pen}_S${sens}"
            TARGET_SUBDIR="grid/${RUN_ID}"
            FULL_TARGET_PATH="${PROJECT_BASE}/out/${TARGET_SUBDIR}"

            # --- 打印进度条 ---
            echo -e "${CYAN}------------------------------------------------------------${NC}"
            echo -e "${YELLOW}[进度 ${current_task}/${total_tasks}]${NC} 正在处理: ${GREEN}${RUN_ID}${NC} (ETA: $eta_str)"
            echo -e "参数: Decay=${decay}, MaxPenalty=${pen}, Sensitivity=${sens}"
            
            echo "[${current_task}/${total_tasks}] Start: ${RUN_ID} at $(date)" >> $MASTER_LOG

            # ==========================================
            # 步骤 A: 运行仿真 (run.sh)
            # ==========================================
            # [修复]：确保所有变量都加了引号，且数组正确展开
            bash "$RUN_SCRIPT" "${FIXED_PARAMS[@]}" \
                --outputDir "$TARGET_SUBDIR" \
                --loadDecayFactor "$decay" \
                --maxCongestionPenalty "$pen" \
                --congestionSensitivity "$sens" > /dev/null
            
            EXIT_CODE=$?

            # 检查仿真是否成功（不仅检查文件，也检查退出码）
            if [ $EXIT_CODE -ne 0 ] || [ ! -f "${FULL_TARGET_PATH}/flowstats.csv" ]; then
                echo -e "${RED}[错误] 仿真失败或未生成数据 (Exit Code: $EXIT_CODE)。跳过分析。${NC}" | tee -a $MASTER_LOG
                continue
            fi

            # ==========================================
            # 步骤 B: 运行自动化分析 (run_analization.sh)
            # ==========================================
            if [ -f "$ANALYSIS_SCRIPT" ]; then
                # 调用分析脚本
                bash "$ANALYSIS_SCRIPT" "$TARGET_SUBDIR" > "${FULL_TARGET_PATH}/analysis_log.txt" 2>&1
                
                if [ $? -eq 0 ]; then
                    echo -e "${GREEN}   -> 分析完成! 图表已保存至: ${TARGET_SUBDIR}/visualization${NC}"
                else
                    echo -e "${RED}   -> [警告] 分析脚本返回错误，请检查 ${TARGET_SUBDIR}/analysis_log.txt${NC}"
                fi
            else
                echo -e "${YELLOW}   -> [跳过] 未找到分析脚本: $ANALYSIS_SCRIPT${NC}"
            fi

        done
    done
done

# --- 4. 结束汇总 ---
end_timestamp=$(date +%s)
duration=$((end_timestamp - start_timestamp))
hours=$((duration / 3600))
minutes=$(( (duration % 3600) / 60 ))

echo "" | tee -a $MASTER_LOG
echo -e "${BLUE}========================================${NC}" | tee -a $MASTER_LOG
echo -e "${GREEN}所有扫描任务已完成！${NC}" | tee -a $MASTER_LOG
echo -e "总耗时: ${hours}小时 ${minutes}分钟" | tee -a $MASTER_LOG
echo -e "结果位于: ${PROJECT_BASE}/out/grid/" | tee -a $MASTER_LOG
echo -e "${BLUE}========================================${NC}" | tee -a $MASTER_LOG