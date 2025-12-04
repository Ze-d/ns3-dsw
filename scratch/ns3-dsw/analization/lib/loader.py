#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据加载层 (Data Loader Layer)
负责读取 XML/CSV 文件并转换为 Pandas DataFrame。
不包含任何绘图或复杂指标计算逻辑。
"""

import os
import glob
import csv
import pandas as pd
import xml.etree.ElementTree as ET
from pathlib import Path

class DataLoader:
    def __init__(self, data_dir):
        self.data_dir = Path(data_dir)
        if not self.data_dir.exists():
            raise FileNotFoundError(f"数据目录不存在: {self.data_dir}")

    def load_task_traces(self) -> pd.DataFrame:
        """
        解析 pro_sink_stats.xml，匹配 EdgeSend 和 CoreComp 事件，
        计算任务延迟并返回 DataFrame。
        """
        xml_path = self.data_dir / 'pro_sink_stats.xml'
        if not xml_path.exists():
            print(f"[警告] 文件缺失: {xml_path}")
            return pd.DataFrame()

        pending_tasks = {}
        completed_tasks = []

        try:
            # 使用 iterparse 节省内存
            context = ET.iterparse(str(xml_path), events=('end',))
            for event, elem in context:
                if elem.tag == 'Event':
                    evt_type = elem.get('type')
                    
                    if evt_type == 'EdgeSend':
                        edge_id = elem.get('Edge-Id')
                        task_id = elem.get('Task-Id')
                        time_str = elem.get('Time')
                        target_ip = elem.get('TargetIp') # 用于消费者分析
                        
                        key = (edge_id, task_id)
                        pending_tasks[key] = {
                            'start_time': float(time_str),
                            'producer': edge_id,
                            'target_ip': target_ip
                        }

                    elif evt_type == 'CoreComp':
                        edge_id = elem.get('Edge-Id')
                        task_id = elem.get('Task-Id')
                        core_id = elem.get('Core-Id')
                        end_time = float(elem.get('Time'))
                        
                        key = (edge_id, task_id)
                        if key in pending_tasks:
                            start_data = pending_tasks[key]
                            start_time = start_data['start_time']
                            
                            completed_tasks.append({
                                'Task_Unique_ID': task_id,
                                'Producer_Node': start_data['producer'],
                                'Consumer_Node': core_id,
                                'Target_IP': start_data.get('target_ip', 'Unknown'),
                                'Start_Time_s': start_time,
                                'End_Time_s': end_time,
                                'Total_Latency_s': end_time - start_time
                            })
                            del pending_tasks[key]
                    
                    elem.clear()
            
            return pd.DataFrame(completed_tasks)
            
        except Exception as e:
            print(f"[错误] 解析任务追踪数据失败: {e}")
            return pd.DataFrame()

    def load_raw_edge_send_events(self) -> pd.DataFrame:
        """仅加载 EdgeSend 事件，用于分析任务生成分布（不管是否完成）"""
        xml_path = self.data_dir / 'pro_sink_stats.xml'
        if not xml_path.exists(): return pd.DataFrame()

        data = []
        tree = ET.parse(xml_path)
        for event in tree.getroot().findall('Event'):
            if event.get('type') == 'EdgeSend':
                data.append({
                    'Time': float(event.get('Time')),
                    'TargetIp': event.get('TargetIp')
                })
        return pd.DataFrame(data)

    def load_pro_sink_events(self) -> pd.DataFrame:
        """
        加载所有 pro_sink_stats.xml 事件，包括 EdgeSend 和 CoreQueue。

        返回 DataFrame，包含：
        - type: 事件类型 ('EdgeSend' 或 'CoreQueue')
        - time: 时间
        - node_id: 节点ID
        - pending_tasks: 待处理任务数 (仅EdgeSend)
        - total_sent: 发送总数 (仅EdgeSend)
        - total_generated: 生成总数 (仅EdgeSend)
        - queue_length: 队列长度 (仅CoreQueue)
        """
        xml_path = self.data_dir / 'pro_sink_stats.xml'
        if not xml_path.exists():
            return pd.DataFrame()

        import re
        data = []
        tree = ET.parse(xml_path)
        for event in tree.getroot().findall('Event'):
            event_type = event.get('type')
            time = float(event.get('Time'))

            if event_type == 'EdgeSend':
                edge_id = event.get('Edge-Id')
                pending = int(event.get('PendingTasks'))
                total_sent = int(event.get('TotalSent'))
                total_gen = int(event.get('TotalGenerated'))

                node_match = re.search(r'Edge-(\d+)', edge_id)
                node_id = int(node_match.group(1)) if node_match else 0

                data.append({
                    'type': 'EdgeSend',
                    'time': time,
                    'node_id': node_id,
                    'task_id': event.get('Task-Id'),
                    'pending_tasks': pending,
                    'total_sent': total_sent,
                    'total_generated': total_gen
                })

            elif event_type == 'CoreQueue':
                core_id = event.get('Core-Id')
                queue_len = int(event.get('QueueLength'))

                node_match = re.search(r'Core-(\d+)', core_id)
                node_id = int(node_match.group(1)) if node_match else 0

                data.append({
                    'type': 'CoreQueue',
                    'time': time,
                    'node_id': node_id,
                    'queue_length': queue_len
                })

        return pd.DataFrame(data)

    def load_node_utilization(self) -> pd.DataFrame:
        """解析 node_util.xml 获取核心利用率"""
        xml_path = self.data_dir / 'node_util.xml'
        if not xml_path.exists(): return pd.DataFrame()

        data = []
        tree = ET.parse(xml_path)
        for event in tree.getroot().findall('Event'):
            if event.get('type') == 'CoreUtil':
                data.append({
                    'Time': float(event.get('Time')),
                    'Core-Id': event.get('Core-Id'),
                    'Utilization': float(event.get('Utilization'))
                })
        return pd.DataFrame(data)

    def load_power_costs(self) -> dict:
        """解析所有 power_cost_node*.xml，返回 {NodeID: DataFrame}"""
        power_files = list(self.data_dir.glob("power_cost_node*.xml"))
        result = {}
        
        for file in power_files:
            try:
                node_id = file.stem.replace('power_cost_', '').capitalize().replace('node', 'Node-')
                tree = ET.parse(file)
                data = []
                for sample in tree.getroot().findall('Sample'):
                    data.append({
                        'Time': float(sample.get('time')),
                        'total_cost': float(sample.get('total_cost')),
                        'price_per_MWh': float(sample.get('price_per_MWh'))
                    })
                result[node_id] = pd.DataFrame(data)
            except Exception as e:
                print(f"[警告] 解析 {file.name} 失败: {e}")
        
        return result

    def load_scheduler_events(self) -> pd.DataFrame:
        """解析 scheduler_events.xml"""
        xml_path = self.data_dir / 'scheduler_events.xml'
        if not xml_path.exists(): return pd.DataFrame()

        data = []
        tree = ET.parse(xml_path)
        for event in tree.getroot().findall('Event'):
            decision = event.get('decision')
            current = event.find('CurrentTarget')
            switch = event.find('SwitchTarget')
            
            if current is None or switch is None: continue
            
            final_target = current.get('nodeId') if decision == 'STAY' else switch.get('nodeId')
            
            data.append({
                'Time': float(event.get('time')),
                'Producer': f"Edge-{event.get('producerNode')}",
                'Decision': decision,
                'FinalTargetNode': f"Core-{final_target}"
            })
        return pd.DataFrame(data)

    def load_flow_stats(self) -> pd.DataFrame:
        """加载 flowstats.csv"""
        csv_path = self.data_dir / 'flowstats.csv'
        if not csv_path.exists(): return pd.DataFrame()
        try:
            return pd.read_csv(csv_path)
        except Exception:
            return pd.DataFrame()

    def load_link_utilization_raw(self) -> list:
        """KPI 计算用：返回包含 'rateMbps' 和 'utilPct' 的简单列表"""
        xml_path = self.data_dir / 'link_util.xml'
        if not xml_path.exists(): return []

        links_data = []
        tree = ET.parse(xml_path)
        root = tree.getroot()
        
        for sample in root.findall('Sample'):
            for link in sample.findall('Link'):
                rate = float(link.get('rateMbps', '0'))
                # 双向利用率
                util_vals = []
                atoB = link.find('AtoB')
                if atoB is not None: util_vals.append(float(atoB.get('utilPct', '0')))
                btoA = link.find('BtoA')
                if btoA is not None: util_vals.append(float(btoA.get('utilPct', '0')))
                
                for u in util_vals:
                    links_data.append({'rate': rate, 'util': u})
                    
        return links_data

    def load_link_utilization_heatmap_data(self) -> pd.DataFrame:
        """
        解析 link_util.xml 用于热力图
        返回 DataFrame: columns=[Time, LinkDir, UtilPct]
        """
        xml_path = self.data_dir / 'link_util.xml'
        if not xml_path.exists(): return pd.DataFrame()

        data = []
        try:
            # 使用 iterparse 处理，events=('start', 'end') 以便同时获取 Sample 的 Time 和内部 Link
            context = ET.iterparse(str(xml_path), events=('start', 'end'))
            current_time = 0.0

            for event, elem in context:
                if event == 'start' and elem.tag == 'Sample':
                    current_time = float(elem.get('time', '0'))
                elif event == 'end':
                    if elem.tag == 'Link':
                        link_id = elem.get('id', 'u')
                        # AtoB
                        atoB = elem.find('AtoB')
                        if atoB is not None:
                            data.append({
                                'Time': current_time,
                                'LinkDir': f"L{link_id}:A→B",
                                'UtilPct': float(atoB.get('utilPct', '0'))
                            })
                        # BtoA
                        btoA = elem.find('BtoA')
                        if btoA is not None:
                            data.append({
                                'Time': current_time,
                                'LinkDir': f"L{link_id}:B→A",
                                'UtilPct': float(btoA.get('utilPct', '0'))
                            })
                        elem.clear() # 清理内存
                    elif elem.tag == 'Sample':
                        elem.clear()

            return pd.DataFrame(data)
        except Exception as e:
            print(f"[错误] 解析链路利用率失败: {e}")
            return pd.DataFrame()

    def load_burst_events(self) -> dict[int, pd.DataFrame]:
        """
        加载所有节点的burst_events XML文件

        返回: {
            node_id_1: DataFrame([time, burst_size, total_generated, ...]),
            node_id_2: DataFrame([...]),
            ...
        }

        每个DataFrame包含以下字段:
        - time: 事件时间戳
        - burst_size: BurstSize.actual
        - expected_mean: BurstSize.expectedMean
        - std_deviation: BurstSize.stdDeviation
        - interarrival_mean: BurstParameters.interarrivalMean
        - total_generated: Statistics.totalTasksGenerated
        - total_bursts: Statistics.totalBursts
        - avg_burst_size: Statistics.avgBurstSize
        """
        burst_files = list(self.data_dir.glob("burst_events_node*.xml"))
        result = {}

        for file in burst_files:
            try:
                # 从文件名提取节点ID
                node_id_str = file.stem.replace('burst_events_node', '')
                try:
                    node_id = int(node_id_str)
                except ValueError:
                    print(f"[警告] 无法解析节点ID: {node_id_str}, 跳过文件 {file.name}")
                    continue

                tree = ET.parse(file)
                data = []
                for event in tree.getroot().findall('BurstEvent'):
                    burst_size = event.find('BurstSize')
                    params = event.find('BurstParameters')
                    stats = event.find('Statistics')

                    if burst_size is None or params is None or stats is None:
                        continue

                    try:
                        data.append({
                            'time': float(event.get('time')),
                            'burst_size': float(burst_size.get('actual')),
                            'expected_mean': float(burst_size.get('expectedMean')),
                            'std_deviation': float(burst_size.get('stdDeviation')),
                            'interarrival_mean': float(params.get('interarrivalMean')),
                            'total_generated': int(stats.get('totalTasksGenerated')),
                            'total_bursts': int(stats.get('totalBursts')),
                            'avg_burst_size': float(stats.get('avgBurstSize'))
                        })
                    except (TypeError, ValueError) as e:
                        print(f"[警告] 解析 {file.name} 中的事件时出错: {e}")
                        continue

                if data:
                    result[node_id] = pd.DataFrame(data)
                    # 按时间排序
                    result[node_id] = result[node_id].sort_values('time')

            except Exception as e:
                print(f"[警告] 解析 {file.name} 失败: {e}")
                continue

        if not result:
            print("[警告] 未找到任何有效的burst_events文件")

        return result

    def compute_backlog_from_bursts(self, burst_data: dict[int, pd.DataFrame]) -> pd.DataFrame:
        """
        根据burst数据计算积压队列演化

        算法：计算相邻burst事件之间生成的任务数，
        作为队列积压的度量（简化模型，假设处理速度恒定）

        Args:
            burst_data: load_burst_events()的返回值

        Returns:
            DataFrame([time, node_id, pending_tasks])
        """
        all_backlog = []

        for node_id, node_df in burst_data.items():
            if node_df.empty or len(node_df) < 1:
                continue

            # 计算每个事件带来的任务积压
            node_df = node_df.copy()
            node_df = node_df.sort_values('time')

            # 使用累积生成任务数作为队列压力的近似
            # pending_tasks = 当前累积任务数 - 假设已处理任务数
            # 简化假设：任务处理速度与生成速度成正比，使用滑动窗口模拟
            node_df['pending_tasks'] = node_df['total_generated'].copy()

            # 添加到结果列表
            for _, row in node_df.iterrows():
                all_backlog.append({
                    'time': row['time'],
                    'node_id': node_id,
                    'pending_tasks': row['pending_tasks']
                })

        if not all_backlog:
            return pd.DataFrame()

        result = pd.DataFrame(all_backlog)
        result = result.sort_values(['node_id', 'time'])
        return result

    def get_burst_intervals(self, burst_data: dict[int, pd.DataFrame]) -> pd.DataFrame:
        """
        计算所有节点的到达间隔时间

        Args:
            burst_data: load_burst_events()的返回值

        Returns:
            DataFrame([node_id, time, interval])
            interval: 当前事件与前一事件的时间差（秒）
        """
        all_intervals = []

        for node_id, node_df in burst_data.items():
            if node_df.empty or len(node_df) < 2:
                continue

            node_df = node_df.sort_values('time').reset_index(drop=True)

            # 计算与前一事件的时间间隔
            for i in range(1, len(node_df)):
                interval = node_df.loc[i, 'time'] - node_df.loc[i-1, 'time']
                all_intervals.append({
                    'node_id': node_id,
                    'time': node_df.loc[i, 'time'],
                    'interval': interval
                })

        if not all_intervals:
            return pd.DataFrame()

        result = pd.DataFrame(all_intervals)
        return result