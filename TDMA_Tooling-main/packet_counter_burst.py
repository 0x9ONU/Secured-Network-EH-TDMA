from datetime import datetime, timedelta

def is_ack(packet):
    parts = packet.split(":")
    return len(parts) >= 4 and parts[2] == "0" and parts[3] == "1"

def is_data(packet):
    parts = packet.split(":")
    return len(parts) >= 4 and parts[2] == "0" and parts[3] == "0"

def parse_timestamp(ts_str):
    try:
        return datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S.%f")
    except ValueError:
        return None

def process_log(file_path, burst_threshold_count=7, burst_time_window_sec=0.5):
    try:
        with open(file_path, "r") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File not found at '{file_path}'")
        return [], 0

    cycle_counts = []
    current_cycle = []

    for line in lines:
        parts = line.strip().split(", ")
        if len(parts) < 3:
            continue

        timestamp = parse_timestamp(parts[0])
        if timestamp is None:
            continue

        packet = parts[2]

        if packet.startswith("BB:"):
            # Filter bursts from current cycle
            filtered_packets = []
            burst_window = []

            for pkt_time, pkt in current_cycle:
                # Maintain a sliding window of recent timestamps
                burst_window.append((pkt_time, pkt))
                # Drop old entries outside the burst window
                burst_window = [
                    (t, p) for t, p in burst_window
                    if (pkt_time - t).total_seconds() <= burst_time_window_sec
                ]

                if len(burst_window) <= burst_threshold_count:
                    filtered_packets.append((pkt_time, pkt))  # Accept this packet
                else:
                    continue  # Detected burst — discard

            # Count valid data packets (non-bursty)
            valid_data_packets = sum(1 for _, pkt in filtered_packets if is_data(pkt))

            if valid_data_packets > 0:
                cycle_counts.append(valid_data_packets)

            current_cycle = []

        else:
            current_cycle.append((timestamp, packet))

    # Compute average
    if cycle_counts:
        average_cycles = sum(cycle_counts) / len(cycle_counts)
    else:
        average_cycles = 0

    return cycle_counts, average_cycles

# === Main Entry Point ===
if __name__ == "__main__":
    log_file = input("Enter the full path to the log file: ").strip()
    cycle_counts, avg = process_log(log_file)

    if cycle_counts:
        print("\n--- Cycle Counts Between BB Resyncs (excluding replay bursts) ---")
        print(f"Average number of cycles between BB resyncs: {avg:.2f}")
    else:
        print("No BB intervals or data cycles found.")

