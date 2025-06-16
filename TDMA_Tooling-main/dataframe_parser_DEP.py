import pandas as pd
import matplotlib.pyplot as plt

from parse_utils import parse_file

# Parses XCTU Output *.log file

plt.rcParams['figure.dpi'] = 1200
plt.rcParams['savefig.dpi'] = 1200

if __name__ == "__main__":
    file = input("Enter name of file")
    output = input("Enter desired output name: ")
    time_slot = input("Enter time slot: ")
    data = []
    keys = ["Time", "Number", "Type", "Data", "Error", "Repeated"]
    data = parse_file(file, keys)

    # Convert from HEX to ASCII String
    for entry in data:
        try:
            entry["Data"] = bytearray.fromhex(entry.get("Data")).decode()
        except:
            pass
    df = pd.DataFrame(data, columns=keys)
    df = df[df["Number"].notna()]
    df["Time"] = pd.to_datetime(df["Time"])
    df = df[df["Type"] != "AT"]
    df["Data"] = df["Data"].str.replace('\r', '').str.split('\n')

    cleaned_df = df[["Time", "Type", "Number", "Data"]].explode("Data").query("Data != ''")
    # cleaned_df[cleaned_df.index.duplicated()]
    cleaned_df["Type"].mask(cleaned_df["Data"] == "SOURCE", "SYNC", inplace=True)
    cleaned_df["Type"].mask(cleaned_df["Data"] == "RELAY", "SYNC", inplace=True)
    cleaned_df["Type"].mask(cleaned_df["Data"] == "Hello|s", "SOURCE", inplace=True)
    cleaned_df["Type"].mask(cleaned_df["Data"] == "Hello|r", "RELAY", inplace=True)

    test_df = cleaned_df.copy()

    test_df["Shift"] = test_df["Data"].shift()
    test_df["Count"] = (test_df["Data"] == test_df["Shift"]).cumsum()
    m = test_df['Type'].eq('RECV')
    tg = m.ne(m.shift()).where(m).cumsum().fillna(0)
    tg.drop_duplicates()
    test_df.drop_duplicates()
    tg1 = test_df.groupby(tg).cumcount()
    tg1.drop_duplicates()
    tg1 = tg1.mask(m, tg1 // 2)
    tg1.drop_duplicates()
    test_df2 = (test_df.groupby(['Type', tg, tg1], sort=False)['Data']
           .agg(''.join)
           .droplevel([1, 2])
           .reset_index())

    test_df2["Shift"] = test_df2["Data"].shift()
    test_df2["Count"] = (test_df2["Data"] == test_df2["Shift"]).cumsum()

    preserve_df = cleaned_df[["Time", "Number"]]
    # group = ("RECV" != cleaned_df['Type'].shift()).cumsum().rename('group')
    # cleaned_df = cleaned_df.groupby(["Type", group], sort=False)['Data'].agg(''.join).reset_index().drop('group', axis=1)
    # group = "RECV".ne(cleaned_df['Type'].shift()).cumsum()
    group = ("RECV" != cleaned_df['Type'].shift()).cumsum().rename('group')
    group1 = cleaned_df.groupby(group).cumcount() // 2

    cleaned_df = (cleaned_df.groupby(['Type', group, group1], sort=False)['Data']
                  .agg(''.join)
                  .droplevel([1, 2])
                  .reset_index())

    cleaned_df["Type"].mask(cleaned_df["Data"] == "Hello|r", "RELAY", inplace=True)
    cleaned_df["Type"].mask(cleaned_df["Data"] == "Hello|s", "SOURCE", inplace=True)
    cleaned_df["Number"] = cleaned_df.index
    cleaned_df = cleaned_df[["Number", "Type", "Data"]]

    consecutive_df = cleaned_df.copy()
    consecutive_df["Shift"] = consecutive_df["Type"].shift()
    consecutive_df["Repeated"] = consecutive_df["Type"] == consecutive_df["Shift"]
    # consecutive_df = consecutive_df[consecutive_df["Repeated"]]
    consecutive_df = consecutive_df[consecutive_df['Type'].isin(["SOURCE", "RELAY"])]
    consecutive_df["Count"] = (consecutive_df["Type"] == consecutive_df["Shift"]).cumsum()

    # Generate Images and CSV Output
    consecutive_df["Count"].plot()
    plt.title(f"Number of Repeats vs. Packet of Received Packets for {time_slot}ms Time-Slot")
    plt.ylabel("Repeats")
    plt.xlabel("Packet Number")
    plt.savefig(f"{output}.png")
    plt.show()
    consecutive_df.to_csv(f"{output}.txt", encoding='utf-8', index=False)
