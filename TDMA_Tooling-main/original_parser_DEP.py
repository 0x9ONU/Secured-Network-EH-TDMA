import matplotlib.pyplot as plt
import pandas as pd


# Parses XCTU Output *.log file

def main(file, output, chart_title):
    file = open(file, "r")
    data = file.read()
    raw = data.split('\n')
    parsed = []
    for item in raw:
        parsed.append(item.split(","))
    keys = ["Time", "Number", "Type", "Data", "Error", "Repeated"]
    parsed = [dict(zip(keys, item)) for item in parsed]
    for item in parsed:
        data_val = item.get("Data")
        try:
            # Format data
            item["Data"] = bytearray.fromhex(data_val).decode()
            item["Data"] = item["Data"].rstrip()
            if "\r\n" in item["Data"]:
                name = item["Number"]
                item["Error"] = True
                print(f"Packet Number {name} is corrupted")
            if item["Time"] == '':
                parsed.remove(item)
            if item["Data"] not in ["Hello|s", "Hello|r", "SOURCE"]:
                item["Error"] = True
            if item["Data"] == "SOURCE":
                item["Type"] = "SYNC"
                item["Error"] = False
            elif item["Data"] == "Hello|r":
                item["Type"] = "RELAY"
                item["Error"] = False
            elif item["Data"] == "Hello|s":
                item["Type"] = "SOURCE"
                item["Error"] = False
            item["Repeated"] = False
        except:
            print(f"An error has occurred")
            parsed.remove(item)

    # TODO: Separate Hello|s\r\nHello|r into 2 packets

    df = pd.DataFrame(parsed, columns=keys)

    # Create Dataframe to house all consecutive repeated packets
    consecutive_df = df.copy()
    consecutive_df["Shift"] = consecutive_df["Data"].shift()
    consecutive_df["Repeated"] = consecutive_df["Data"] == consecutive_df["Shift"]
    df["Repeated"] = consecutive_df["Repeated"]
    consecutive_df = consecutive_df[consecutive_df["Repeated"]]
    consecutive_df = consecutive_df[consecutive_df['Data'].isin(["Hello|s", "Hello|r"])]
    consecutive_df["Count"] = (consecutive_df["Data"] == consecutive_df["Shift"]).cumsum()

    consecutive_df.Time = pd.to_datetime(consecutive_df.Time).dt.time
    print(df)
    print(consecutive_df)
    consecutive_df["Count"].plot()
    plt.title(f"Number of Repeats vs. Packet Number for {chart_title}ms Time-Slot")
    plt.ylabel("Repeats")
    plt.xlabel("Packet Number")
    plt.savefig(f"{output}.png")
    plt.show()

    consecutive_df.to_csv(output, encoding='utf-8', index=False)
    df.to_csv(f"{output}_original", encoding='utf-8', index=False)


if __name__ == "__main__":
    file_input = input("Enter path of log file: ")
    output = input("Enter desired name of output files: ")
    time_slot = input("Enter length of time slot: ")
    main(file_input, output, time_slot)
