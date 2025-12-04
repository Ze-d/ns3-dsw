#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
指标计算层 (Metrics Layer)
负责基于 DataLoader 提供的数据计算 KPI。
不包含 print 输出，只返回数据结构。
"""

import numpy as np
import pandas as pd

class MetricsCalculator:
    def __init__(self, loader):
        self.loader = loader
        # 缓存一些重数据，避免重复读取
        self._task_df = None
        self._node_util_df = None
        self._power_data = None

    @property
    def task_df(self):
        if self._task_df is None:
            self._task_df = self.loader.load_task_traces()
        return self._task_df

    def get_global_kpi(self) -> dict:
        """计算 calculate_kpi.py 中的7个关键指标"""
        kpi = {}

        # 1. 总电价
        power_data = self.loader.load_power_costs()
        total_cost = 0.0
        for _, df in power_data.items():
            if not df.empty:
                total_cost += df['total_cost'].iloc[-1]
        kpi['total_power_cost'] = total_cost

        # 2. 整体平均算力利用率
        node_df = self.loader.load_node_utilization()
        if not node_df.empty:
            kpi['avg_core_utilization'] = node_df['Utilization'].mean()
        else:
            kpi['avg_core_utilization'] = 0.0

        # 3. 整体平均延迟 (优先使用 flowstats.csv)
        flow_df = self.loader.load_flow_stats()
        if not flow_df.empty and 'avgDelay_ms' in flow_df.columns:
            kpi['avg_delay_ms'] = flow_df['avgDelay_ms'].mean()
        else:
            kpi['avg_delay_ms'] = 0.0

        # 4 & 5. 链路带宽和利用率
        link_data = self.loader.load_link_utilization_raw()
        if link_data:
            rates = [d['rate'] for d in link_data]
            utils = [d['util'] for d in link_data]
            kpi['avg_link_bandwidth_mbps'] = sum(rates) / len(rates) if rates else 0
            kpi['avg_link_utilization_pct'] = sum(utils) / len(utils) if utils else 0
        else:
            kpi['avg_link_bandwidth_mbps'] = 0.0
            kpi['avg_link_utilization_pct'] = 0.0

        # 6. 完成总任务数 (从 task_df 计算更准确)
        if not self.task_df.empty:
            kpi['total_completed_tasks'] = len(self.task_df)
        else:
            kpi['total_completed_tasks'] = 0

        # 7. 平均每任务电价
        if kpi['total_completed_tasks'] > 0:
            kpi['avg_cost_per_task'] = total_cost / kpi['total_completed_tasks']
        else:
            kpi['avg_cost_per_task'] = 0.0

        return kpi

    def get_latency_stats(self) -> dict:
        """计算任务延迟的详细统计 (Min, Max, P95, P99)"""
        if self.task_df.empty: return {}
        
        latencies = self.task_df['Total_Latency_s']
        return {
            'count': len(latencies),
            'min': latencies.min(),
            'max': latencies.max(),
            'mean': latencies.mean(),
            'std': latencies.std(),
            'p95': latencies.quantile(0.95),
            'p99': latencies.quantile(0.99),
            'throughput': len(latencies) / (self.task_df['End_Time_s'].max() - self.task_df['Start_Time_s'].min()) if len(latencies) > 0 else 0
        }

    def get_latency_timeseries(self, window_size=1.0) -> pd.DataFrame:
        """按时间窗口聚合延迟数据 (用于画图)"""
        df = self.task_df.copy()
        if df.empty: return pd.DataFrame()

        # 按 End_Time 分桶
        df['TimeWindow'] = (df['End_Time_s'] // window_size).astype(int)
        
        # 聚合
        stats = df.groupby('TimeWindow').agg(
            avg_latency=('Total_Latency_s', 'mean'),
            task_count=('Total_Latency_s', 'count')
        ).reset_index()
        
        stats.rename(columns={'TimeWindow': 'time_second'}, inplace=True)
        return stats

    def get_consumer_distribution_timeseries(self, window_size=1.0) -> pd.DataFrame:
        """获取消费者任务分布的时间序列 (用于堆积图)"""
        raw_df = self.loader.load_raw_edge_send_events()
        if raw_df.empty: return pd.DataFrame()

        df = raw_df.copy()
        df['TimeWindow'] = (df['Time'] // window_size) * window_size

        # 统计每个窗口、每个TargetIp的数量
        grouped = df.groupby(['TimeWindow', 'TargetIp']).size().reset_index(name='Count')

        # 转换为透视表
        pivot = grouped.pivot(index='TimeWindow', columns='TargetIp', values='Count').fillna(0)

        # 计算百分比
        totals = pivot.sum(axis=1)
        percents = pivot.div(totals, axis=0) * 100

        return percents

    def analyze_price_driven_scheduling(self) -> dict:
        """
        分析纯电价驱动调度场景

        解析调度事件，分析电价波动与节点切换的关系。
        返回包含分析结果的字典。
        """
        try:
            import xml.etree.ElementTree as ET
            from pathlib import Path
        except ImportError:
            return {"error": "缺少必需的依赖 (xml.etree.ElementTree)"}

        # 获取调度事件文件
        sched_df = self.loader.load_scheduler_events()
        if sched_df.empty:
            return {"error": "无法加载调度事件数据"}

        # 解析XML文件（如果可用）
        data_dir = self.loader.data_dir
        scheduler_file = data_dir / 'scheduler_events.xml'

        if not scheduler_file.exists():
            return {"error": f"调度事件文件不存在: {scheduler_file}"}

        try:
            tree = ET.parse(scheduler_file)
            root = tree.getroot()
        except Exception as e:
            return {"error": f"解析XML文件失败: {e}"}

        events = []
        for event in root.findall('Event'):
            time = float(event.get('time'))
            producer = int(event.get('producerNode'))
            decision = event.get('decision')

            threshold = float(event.find('Threshold').get('value'))

            # 获取CurrentTarget的Cost
            current_target = event.find('CurrentTarget')
            if current_target is not None:
                current_cost_elem = current_target.find('Cost')
                if current_cost_elem is not None:
                    current_cost = float(current_cost_elem.get('total'))
                    current_processing = float(current_cost_elem.get('processingCost'))
                    current_congestion = float(current_cost_elem.get('congestionCost'))
                else:
                    current_cost = current_processing = current_congestion = 0.0
            else:
                current_cost = current_processing = current_congestion = 0.0

            # 获取SwitchTarget的Cost (可能不存在，特别是当decision="STAY")
            switch_target = event.find('SwitchTarget')
            if switch_target is not None:
                switch_cost_elem = switch_target.find('Cost')
                if switch_cost_elem is not None:
                    switch_cost = float(switch_cost_elem.get('total'))
                    switch_processing = float(switch_cost_elem.get('processingCost'))
                    switch_congestion = float(switch_cost_elem.get('congestionCost'))
                else:
                    switch_cost = switch_processing = switch_congestion = 0.0
            else:
                # 如果没有SwitchTarget，设为当前值 (STAY决策)
                switch_cost = current_cost
                switch_processing = current_processing
                switch_congestion = current_congestion

            events.append({
                'time': time,
                'producer': producer,
                'decision': decision,
                'current_cost': current_cost,
                'current_processing': current_processing,
                'current_queue': current_congestion,  # 映射congestionCost到queuePenalty
                'switch_cost': switch_cost,
                'switch_processing': switch_processing,
                'switch_queue': switch_congestion,  # 映射congestionCost到queuePenalty
                'threshold': threshold,
                'cost_diff': current_cost - switch_cost,  # 正值表示切换有收益
                'processing_diff': current_processing - switch_processing,
            })

        df = pd.DataFrame(events)

        # 分析切换决策
        switch_events = df[df['decision'] == 'SWITCH']
        stay_events = df[df['decision'] == 'STAY']

        # 计算统计信息
        analysis = {
            "total_events": len(df),
            "switch_events": len(switch_events),
            "stay_events": len(stay_events),
            "switch_rate": len(switch_events) / len(df) if len(df) > 0 else 0,
            "avg_threshold": df['threshold'].mean(),
            "processing_diff_stats": {
                "mean": df['processing_diff'].mean(),
                "std": df['processing_diff'].std(),
                "max": df['processing_diff'].max(),
                "min": df['processing_diff'].min(),
            },
            "cost_diff_stats": {
                "switch_mean": switch_events['cost_diff'].mean() if len(switch_events) > 0 else 0,
                "stay_mean": stay_events['cost_diff'].mean() if len(stay_events) > 0 else 0,
            },
            "cost_composition": {
                "current_processing_ratio": (df['current_processing'] / df['current_cost']).mean(),
                "current_queue_ratio": (df['current_queue'] / df['current_cost']).mean(),
                "switch_processing_ratio": (df['switch_processing'] / df['switch_cost']).mean(),
                "switch_queue_ratio": (df['switch_queue'] / df['switch_cost']).mean(),
            }
        }

        # 有意义的差异分析
        significant_diff = df[abs(df['processing_diff']) > 0.001]
        analysis["significant_diff_events"] = len(significant_diff)

        return analysis
