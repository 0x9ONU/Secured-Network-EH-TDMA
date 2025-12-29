import sys
from datetime import datetime
import statistics

def parse_file(file_path):
    sync_times = []
    last_packet_type = None
    last_legit_sync = None

    with open(file_path, 'r') as file:
        for line in file:
            try:
                timestamp_str, _, packet = line.strip().split(',', 2)
                timestamp = datetime.strptime(timestamp_str.strip(), "%Y-%m-%d %H:%M:%S.%f")
                packet = packet.strip()

                if packet.startswith("BB"):
                    # Ignore if previous packet was also a BB (bug)
                    if last_packet_type == "BB":
                        continue

                    # Ignore if this sync is <15ms from last legit sync
                    if last_legit_sync:
                        delta = (timestamp - last_legit_sync).total_seconds()
                        if delta < 0.015:
                            continue

                    sync_times.append(timestamp)
                    last_legit_sync = timestamp
                    last_packet_type = "BB"
                else:
                    last_packet_type = "DATA"  # or anything else not BB

            except ValueError:
                continue  # skip malformed lines

    return sync_times

def compute_statistics(sync_times):
    if len(sync_times) < 2:
        print("Not enough sync packets to compute intervals.")
        return

    raw_intervals = [(t2 - t1).total_seconds() for t1, t2 in zip(sync_times, sync_times[1:])]
    valid_intervals = [i for i in raw_intervals if i >= 0.250]  # Exclude intervals < 150ms

    print("Number of raw sync intervals:", len(raw_intervals))
    print("Number of valid sync intervals (>=150ms):", len(valid_intervals))

    if not valid_intervals:
        print("No valid sync intervals found.")
        return

    print(f"Average interval: {statistics.mean(valid_intervals):.6f} s")
    print(f"Minimum interval: {min(valid_intervals):.6f} s")
    print(f"Maximum interval: {max(valid_intervals):.6f} s")
    print(f"Standard deviation: {statistics.stdev(valid_intervals):.6f} s")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python analyze_syncs.py <path_to_log_file>")
        sys.exit(1)

    file_path = sys.argv[1]
    sync_times = parse_file(file_path)
    compute_statistics(sync_times)
