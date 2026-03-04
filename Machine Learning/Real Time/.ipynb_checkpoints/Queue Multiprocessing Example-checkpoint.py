import multiprocessing
import serial
import os
import sys
import numpy as np
import pandas as pd
import joblib 
import time
from datetime import datetime

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyUSB0' 
SYNC_PACKET = "BB:0:1:0"
ATTACK = "Normal"
MODEL_PATH = 'KNN.sav'
FEATURES_PATH = 'Trojan_100_ALL_X_test.csv'
NUM_PROCESSORS = 3  
# ---------------------

def calculate_interval_metrics(buffer, global_ref):
    """Calculates stats for the entire batch of packets in the file."""
    if not buffer:
        return None
        
    data_packets = [p for p in buffer if p['is_data']]
    ack_count = sum(1 for p in buffer if p['ack'] == '1')
    
    # We need at least some data to make a prediction
    if not data_packets:
        return None

    data_count = len(data_packets)
    sync_count = sum(1 for p in buffer if p['sync'] == '1')

    rel_start = (buffer[0]['ts'] - global_ref).total_seconds()
    rel_end = (buffer[-1]['ts'] - global_ref).total_seconds()
    interval_duration = rel_end - rel_start

    # Sequence/Slot logic
    missing_slots = 0
    id_map = {"01": 1, "02": 2, "03": 3}
    for i in range(len(buffer) - 1):
        curr_id_val = id_map.get(buffer[i]['id'])
        next_id_val = id_map.get(buffer[i+1]['id'])
        if curr_id_val and next_id_val:
            expected_next = (curr_id_val % 3) + 1
            if next_id_val != expected_next:
                missing_slots += 1

    data_intervals = []
    if len(data_packets) > 1:
        for i in range(len(data_packets) - 1):
            diff = (data_packets[i+1]['ts'] - data_packets[i]['ts']).total_seconds()
            data_intervals.append(diff)
    
    return {
        "Rel Start (s)": round(rel_start, 6),
        "Rel End (s)": round(rel_end, 6),
        "Interval Duration": round(interval_duration, 6),
        "Syncs": sync_count,
        "Acks": ack_count,
        "Data Packets": data_count,
        "Missing Slots": missing_slots,
        "Max Data Gap": max(data_intervals) if data_intervals else 0,
        "Min Data Gap": min(data_intervals) if data_intervals else 0
    }

def process_txt_to_dataframe(input_file_path, worker_id):
    """Parses the TXT file. Now ignores the 'sync == 1' requirement to trigger processing."""
    interval_rows = []
    packet_buffer = []
    global_start_time = None 
    
    try:
        with open(input_file_path, 'r') as f:
            lines = f.readlines()
            
        if not lines:
            return None

        for line in lines:
            line = line.strip()
            if not line: continue
            parts = line.split(', ')
            if len(parts) < 3: continue
            
            raw_ts = parts[0]
            # ID:Type:Sync:Ack:Data
            raw_data = parts[2].split(':', 4)
            while len(raw_data) < 5: raw_data.append("")
            
            ts_obj = datetime.strptime(raw_ts, '%Y-%m-%d %H:%M:%S.%f')
            if global_start_time is None: global_start_time = ts_obj

            current_packet = {
                'ts': ts_obj, 
                'id': raw_data[0], 
                'sync': raw_data[2],
                'ack': raw_data[3], 
                # Check if the data part is not empty
                'is_data': bool(raw_data[4].strip())
            }
            packet_buffer.append(current_packet)

        # Process the whole file as a single flow interval
        if packet_buffer:
            metrics = calculate_interval_metrics(packet_buffer, global_start_time)
            if metrics:
                interval_rows.append(metrics)
            
    except Exception as e:
        print(f"[Processor {worker_id}] Error Parsing: {e}")
        return None

    return pd.DataFrame(interval_rows) if interval_rows else None

def data_collector(data_queue):
    """Task 1: Monitors Serial and slices flows at each SYNC_PACKET."""
    if not os.path.exists("txt"): os.makedirs("txt")
    
    try:
        xbee = serial.Serial(port=SERIAL_PORT, baudrate=9600, timeout=1.0)
        print(f"[Collector] Serial Connected to {SERIAL_PORT}")
    except Exception as e:
        print(f"[Collector] SERIAL ERROR: {e}")
        return

    session_buffer = []
    
    while True:
        line_raw = xbee.readline()
        if not line_raw: continue
        
        line = line_raw.decode(errors='ignore').rstrip()
        
        if SYNC_PACKET in line:
            # We found a boundary. If we have data, save it as a finished flow.
            if session_buffer:
                timestamp = datetime.now().strftime('%H%M%S_%f')
                filename = f"txt/flow_{timestamp}.txt"
                with open(filename, 'w') as f:
                    f.write("\n".join(session_buffer) + "\n")
                data_queue.put(filename)
                print(f"[Collector] Dispatched Flow | Lines: {len(session_buffer)} | Q Size: {data_queue.qsize()}")
            
            # Start a new buffer. 
            # We include the SYNC packet in the new buffer so 'Syncs' count is accurate.
            session_buffer = []
            formatted_entry = f"{datetime.now()}, 0, {line}"
            session_buffer.append(formatted_entry)
        else:
            # Accumulate normal data packets
            formatted_entry = f"{datetime.now()}, {len(session_buffer)}, {line}"
            session_buffer.append(formatted_entry)

def data_processor(data_queue, worker_id):
    """Task 2: Pulls files from queue and runs inference."""
    print(f"[Processor {worker_id}] Loading Model...")
    try:
        model = joblib.load(MODEL_PATH)
        feature_schema = pd.read_csv(FEATURES_PATH, nrows=0).columns.tolist()
        print(f"[Processor {worker_id}] Ready.")
    except Exception as e:
        print(f"[Processor {worker_id}] FATAL LOAD ERROR: {e}")
        return

    while True:
        txt_filename = data_queue.get() 
        start_time = time.time()
        
        df_inference = process_txt_to_dataframe(txt_filename, worker_id)
        
        if df_inference is not None and not df_inference.empty:
            # Align features with the training schema (fill missing with 0)
            X_inference = df_inference.reindex(columns=feature_schema).fillna(0)
            
            # Run KNN
            predictions = model.predict(X_inference)
            hits = sum(predictions == 1)
            duration = round(time.time() - start_time, 3)
            
            print(f"[Processor {worker_id}] Done: {os.path.basename(txt_filename)} | Hits: {hits} | Time: {duration}s")
        else:
            # This will happen if a flow contains zero 'is_data' packets
            print(f"[Processor {worker_id}] Skipped: No data packets found in flow.")
        
        # Always cleanup the file
        if os.path.exists(txt_filename):
            os.remove(txt_filename)

if __name__ == "__main__":
    data_q = multiprocessing.Queue()

    p_collector = multiprocessing.Process(target=data_collector, args=(data_q,))
    p_collector.daemon = True
    p_collector.start()

    for i in range(NUM_PROCESSORS):
        p = multiprocessing.Process(target=data_processor, args=(data_q, i))
        p.daemon = True
        p.start()

    print(f"[System] Setup complete. Listening on {SERIAL_PORT}...")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[System] Shutdown requested.")