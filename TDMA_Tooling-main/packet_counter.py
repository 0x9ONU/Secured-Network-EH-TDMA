def is_ack(packet):
    parts = packet.split(":")
    return len(parts) >= 4 and parts[2] == "0" and parts[3] == "1"

def is_data(packet):
    parts = packet.split(":")
    return len(parts) >= 4 and parts[2] == "0" and parts[3] == "0"

def process_log(file_path):
    try:
        with open(file_path, "r") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File not found at '{file_path}'")
        return [], 0

    cycle_counts = []
    valid_data_packets = 0

    for line in lines:
        parts = line.strip().split(", ")
        if len(parts) < 3:
            continue

        packet = parts[2]

        if packet.startswith("BB:"):
            if valid_data_packets > 0:
                cycle_counts.append(valid_data_packets)
            valid_data_packets = 0
        elif is_data(packet):
            valid_data_packets += 1

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
        print("\n--- Cycle Counts Between BB Resyncs ---")
        print(f"\nAverage number of cycles between BB resyncs: {avg:.2f}")
    else:
        print("No BB intervals or data cycles found.")

