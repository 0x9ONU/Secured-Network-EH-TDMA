import os
import re
import csv
from collections import defaultdict

# Configuration
BASE_DIRECTORY = '.' 
OUTPUT_PRECISION = 'precision_results.csv'
OUTPUT_RECALL = 'recall_results.csv'
OUTPUT_F1 = 'f1_score_results.csv'

def calculate_metrics(tp, fp, tn, fn):
    precision = tp / (tp + fp) if (tp + fp) > 0 else 0
    recall = tp / (tp + fn) if (tp + fn) > 0 else 0
    f1 = 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0
    return precision, recall, f1

def parse_summary_file(filepath, filename):
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
    except:
        return None

    # Model extraction from filename (e.g., Trojan_40_ALL_KNN_Results.txt -> KNN)
    parts_name = filename.split('_')
    model = parts_name[3] if len(parts_name) >= 4 else "Unknown"

    def get_num(label_pattern):
        match = re.search(label_pattern + r".*?:\s*(\d+)", content)
        return int(match.group(1)) if match else 0

    tp = get_num(r"True Positives \(TP\)")
    fp = get_num(r"False Positives \(FP\)")
    tn = get_num(r"True Negatives \(TN\)")
    fn = get_num(r"False Negatives \(FN\)")

    if tp == 0 and fp == 0 and tn == 0 and fn == 0:
        return None

    return (model, calculate_metrics(tp, fp, tn, fn))

def sort_logic(key_tuple):
    """
    Handles sorting: Energy (100 -> 80) then Offset (300ms -> 10ms).
    Returns a tuple of integers for accurate numeric reverse sorting.
    """
    energy_str, offset_str = key_tuple
    # Extract digits: "100 percent" -> 100, "300ms" -> 300
    energy_num = int(re.search(r'\d+', energy_str).group()) if re.search(r'\d+', energy_str) else 0
    offset_num = int(re.search(r'\d+', offset_str).group()) if re.search(r'\d+', offset_str) else 0
    # Return as negative for descending sort (High to Low)
    return (-energy_num, -offset_num)

def main():
    grouped_data = defaultdict(dict)
    all_models = set()

    for root, dirs, files in os.walk(BASE_DIRECTORY):
        for filename in files:
            if filename.endswith(".txt") and "Results" in filename:
                parts = os.path.normpath(root).split(os.sep)
                if len(parts) >= 2:
                    offset_val = parts[-1]      # e.g., "300ms"
                    energy_val = parts[-2]      # e.g., "100 percent"
                    
                    result = parse_summary_file(os.path.join(root, filename), filename)
                    if result:
                        model, metrics = result
                        grouped_data[(energy_val, offset_val)][model] = metrics
                        all_models.add(model)

    if not grouped_data:
        print("No data found. Check your directory structure.")
        return

    # Column order for models
    sorted_models = sorted(list(all_models))
    
    # Row order: 100% -> 80% and 300ms -> 10ms
    sorted_keys = sorted(grouped_data.keys(), key=sort_logic)

    metrics_indices = [('Precision', 0), ('Recall', 1), ('F1 Score', 2)]
    output_files = [OUTPUT_PRECISION, OUTPUT_RECALL, OUTPUT_F1]

    for (label, idx), out_file in zip(metrics_indices, output_files):
        with open(out_file, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(['Energy (%)', 'Trojan Offset'] + sorted_models)
            
            for key in sorted_keys:
                energy, offset = key
                row = [energy, offset]
                for model in sorted_models:
                    val = grouped_data[key].get(model, (0, 0, 0))[idx]
                    row.append(round(val, 4))
                writer.writerow(row)

    print(f"Success! Processed {len(sorted_keys)} scenarios.")
    print(f"Files saved with descending order (100->80, 300ms->10ms).")

if __name__ == "__main__":
    main()