import os

import pandas as pd

from parse_utils import parse_file


def process_file(file_path: str, headers: list):
    # Parse
    output_name = file_path.split('.')[0]
    data = parse_file(file_path, headers)

    # Manipulate
    sync_df = pd.DataFrame(data)
    sync_df.dropna(inplace=True)

    sync_df['Timestamp'] = pd.to_datetime(sync_df['Timestamp'], format='%Y-%m-%d %H:%M:%S.%f')
    sync_df['Timestamp'] = (sync_df['Timestamp'] - pd.Timestamp("1970-01-01")) // pd.Timedelta("1ms")
    sync_df['Packet_Number'] = pd.to_numeric(sync_df['Packet_Number'])

    # create default index if necessary
    sync_df = sync_df.reset_index(drop=True)
    # check Start and R
    match1 = sync_df['Data'].str.contains('SYNC')  # First
    match2 = sync_df['Data'].str.contains('R')  # Second

    # Group by SYNC's
    group = match1.cumsum()

    # Get first R in each group to a mask
    mask1 = match2.groupby(group).transform('idxmax').eq(sync_df.index)

    # Get first message after SYNC to mask
    mask2 = match1.groupby(group).shift(fill_value=True)

    # boolean indexing
    sync_df = sync_df[mask1 | mask2]

    # Get Time and Packet Difference
    sync_df['Time_Diff'] = sync_df['Timestamp'].groupby(group).diff().mask(mask2)
    sync_df['Packet_Diff'] = sync_df['Packet_Number'].groupby(group).diff().mask(mask2)

    # Output
    sync_df.dropna(inplace=True)  # Drop empties
    sync_df['Timestamp'] = pd.to_datetime(sync_df['Timestamp'], unit='ms')  # Put timestamp back to readable format
    sync_df['Time_Diff'] = sync_df['Time_Diff'] / 1000
    output_file = open(f'{output_name}.csv', 'w')
    sync_df.to_csv(f'{output_name}.csv', index=False)
    output_file.close()
    avg = sync_df['Packet_Diff'].mean()
    print(f'Average packetCount is {avg}')


if __name__ == "__main__":
    # Input
    directory = input("Enter directory containing files to analyze: ")

    # Hardcoded keys, shouldn't be, but I'm lazy (fix if needed)
    keys = ['Timestamp', 'Packet_Number', 'Data']
    # Get list of all files in directory
    files = [f for f in os.listdir(directory) if f.endswith('.txt')]

    # Process files sequentially
    for file in files:
        try:
            process_file(os.path.join(directory, file), keys)
            print(f"Analyzed: {file}")
        except KeyError:
            print(f"FAILED on: {file}")
