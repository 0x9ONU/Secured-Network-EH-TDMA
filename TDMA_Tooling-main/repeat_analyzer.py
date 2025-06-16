import pandas as pd
import matplotlib.pyplot as plt

from parse_utils import parse_file

plt.rcParams['figure.dpi'] = 1200
plt.rcParams['savefig.dpi'] = 1200

if __name__ == "__main__":
    file = input("Enter name of file")
    output = input("Enter desired output name: ")
    time_slot = input("Enter time slot: ")
    keys = ['DateTime', 'Number', 'Data']
    data = parse_file(file, keys)

    original_df = pd.DataFrame(data, columns=keys)
    original_df['DateTime'] = pd.to_datetime(original_df['DateTime'])

    concat_df = original_df.copy()
    concat_df["Shift"] = concat_df["Data"].shift()
    concat_df["Repeated"] = concat_df["Data"] == concat_df["Shift"]
    concat_df["Count"] = (concat_df["Data"] == concat_df["Shift"]).cumsum()
    concat_df["Count"].plot()
    plt.title(f"Number of Repeats vs. Packet of Received Packets for {time_slot}ms Time-Slot")
    plt.ylabel("Repeats")
    plt.xlabel("Packet Number")
    plt.savefig(f"{output}.png")
    plt.show()
