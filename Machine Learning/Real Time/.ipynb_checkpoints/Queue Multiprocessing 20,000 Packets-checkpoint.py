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
MODEL_PATH = 'KNN.sav'
FEATURES_PATH = 'Trojan_100_ALL_X_test.csv'
NUM_PROCESSORS = 3  
TARGET_PACKETS = 20000
RESULTS_DIR = "results"
# ---------------------

def calculate_interval_metrics(buffer, global_ref):
    """Calculates stats for the entire batch of packets in the file."""
    if not buffer: return None
    data_packets = [p for p in buffer if p['is_data']]
    if not data_packets: return None

    ack_count = sum(1 for p in buffer if p['ack'] == '1')
    sync_count = sum(1 for p in buffer if p['sync'] == '1')
    
    rel_start = (buffer[0]['ts'] - global_ref).total_seconds()
    rel_end = (buffer[-1]['ts'] - global_ref).total_seconds()
    
    missing_slots = 0
    id_map = {"01": 1, "02": 2, "03": 3}
    for i in range(len(buffer) - 1):
        curr = id_map.get(buffer[i]['id'])
        nxt = id_map.get(buffer[i+1]['id'])
        if curr and nxt and nxt != (curr % 3) + 1:
            missing_slots += 1

    data_intervals = []
    if len(data_packets) > 1:
        for i in range(len(data_packets) - 1):
            diff = (data_packets[i+1]['ts'] - data_packets[i]['ts']).total_seconds()
            data_intervals.append(diff)
    
    return {
        "Rel Start (s)": round(rel_start, 6),
        "Interval Duration": round(rel_end - rel_start, 6),
        "Syncs": sync_count,
        "Acks": ack_count,
        "Data Packets": len(data_packets),
        "Missing Slots": missing_slots,
        "Max Data Gap": max(data_intervals) if data_intervals else 0,
        "Min Data Gap": min(data_intervals) if data_intervals else 0
    }

def process_txt_to_dataframe(input_file_path, worker_id):
    """Parses TXT into memory and prints worker status."""
    interval_rows = []
    packet_buffer = []
    global_start_time = None 
    try:
        with open(input_file_path, 'r') as f:
            lines = f.readlines()
        
        for line in lines:
            parts = line.strip().split(', ')
            if len(parts) < 3: continue
            raw_data = parts[2].split(':', 4)
            while len(raw_data) < 5: raw_data.append("")
            
            ts_obj = datetime.strptime(parts[0], '%Y-%m-%d %H:%M:%S.%f')
            if global_start_time is None: global_start_time = ts_obj
            
            packet_buffer.append({
                'ts': ts_obj, 'id': raw_data[0], 'sync': raw_data[2],
                'ack': raw_data[3], 'is_data': bool(raw_data[4].strip())
            })
            
        if packet_buffer:
            m = calculate_interval_metrics(packet_buffer, global_start_time)
            if m: interval_rows.append(m)
    except Exception as e:
        print(f"[Processor {worker_id}] Error Parsing {input_file_path}: {e}")
        return None
    return pd.DataFrame(interval_rows) if interval_rows else None

def data_collector(data_queue, stop_event):
    """Task 1: Collects 20,000 packets and saves flows."""
    if not os.path.exists("txt"): os.makedirs("txt")
    try:
        xbee = serial.Serial(port=SERIAL_PORT, baudrate=9600, timeout=1.0)
        print(f"[Collector] Connected. Monitoring {SERIAL_PORT} for {TARGET_PACKETS} packets...")
    except Exception as e:
        print(f"[Collector] Serial Error: {e}"); return

    total_packets_collected = 0
    session_buffer = []
    
    while total_packets_collected < TARGET_PACKETS:
        line_raw = xbee.readline()
        if not line_raw: continue
        line = line_raw.decode(errors='ignore').rstrip()
        
        total_packets_collected += 1
        
        if SYNC_PACKET in line:
            if session_buffer:
                fname = f"txt/flow_{datetime.now().strftime('%H%M%S_%f')}.txt"
                with open(fname, 'w') as f: f.write("\n".join(session_buffer))
                data_queue.put(fname)
                print(f"[Collector] Progress: {total_packets_collected}/{TARGET_PACKETS} | Flow Dispatched.")
            
            session_buffer = [f"{datetime.now()}, 0, {line}"]
        else:
            session_buffer.append(f"{datetime.now()}, {len(session_buffer)}, {line}")

    print(f"\n[Collector] TARGET {TARGET_PACKETS} REACHED. Signaling workers to stop...")
    stop_event.set()
    for _ in range(NUM_PROCESSORS): 
        data_queue.put(None) # Poison pill for each worker

def data_processor(data_queue, results_list, worker_id):
    """Task 2: Predicts anomalies and prints live hits to console."""
    print(f"[Processor {worker_id}] Loading Model...")
    model = joblib.load(MODEL_PATH)
    schema = pd.read_csv(FEATURES_PATH, nrows=0).columns.tolist()
    print(f"[Processor {worker_id}] Online.")
    
    while True:
        fname = data_queue.get()
        if fname is None: break 
        
        start_time = time.time()
        df = process_txt_to_dataframe(fname, worker_id)
        
        if df is not None and not df.empty:
            X = df.reindex(columns=schema).fillna(0)
            preds = model.predict(X)
            hits = int(sum(preds == 1))
            
            # Save to shared list for final file
            results_list.append({
                'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                'file': os.path.basename(fname),
                'hits': hits,
                'preds': preds.tolist()
            })
            
            duration = round(time.time() - start_time, 3)
            print(f"[Processor {worker_id}] Finished {os.path.basename(fname)} | Hits: {hits} | Time: {duration}s")
        else:
            print(f"[Processor {worker_id}] Flow contained no valid data packets.")

        if os.path.exists(fname): os.remove(fname)

if __name__ == "__main__":
    manager = multiprocessing.Manager()
    data_q = manager.Queue()
    results_list = manager.list()
    stop_event = multiprocessing.Event()

    p_col = multiprocessing.Process(target=data_collector, args=(data_q, stop_event))
    p_col.start()

    procs = []
    for i in range(NUM_PROCESSORS):
        p = multiprocessing.Process(target=data_processor, args=(data_q, results_list, i))
        p.start()
        procs.append(p)

    p_col.join()
    for p in procs: p.join()

    # --- SAVE FINAL RESULTS ---
    if not os.path.exists(RESULTS_DIR): os.makedirs(RESULTS_DIR)
    
    model_type = os.path.splitext(os.path.basename(MODEL_PATH))[0]
    feature_context = "_".join(os.path.basename(FEATURES_PATH).split("_")[:3])
    final_path = os.path.join(RESULTS_DIR, f"{feature_context}_{model_type}_Results.txt")

    print("\n" + "="*50)
    print(f"COLLECTION COMPLETE. Saving results to {final_path}...")
    
    with open(final_path, 'w') as f:
        f.write(f"SUMMARY REPORT: {feature_context} | {model_type}\n")
        f.write(f"Total Packets Collected: {TARGET_PACKETS}\n")
        f.write("-" * 50 + "\n")
        total_hits = 0
        for res in results_list:
            total_hits += res['hits']
            f.write(f"{res['timestamp']} | Hits: {res['hits']} | File: {res['file']}\n")
        f.write("-" * 50 + "\n")
        f.write(f"TOTAL ANOMALIES (HITS): {total_hits}\n")

    print(f"FINAL TOTAL ANOMALIES: {total_hits}")
    print("="*50)