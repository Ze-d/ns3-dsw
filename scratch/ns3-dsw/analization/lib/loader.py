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