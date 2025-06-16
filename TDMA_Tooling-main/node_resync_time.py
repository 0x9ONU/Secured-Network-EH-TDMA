import os

import pandas as pd

from parse_utils import parse_file


def process_file(file_path: str, headers: list, timeslot: int):
    epsilon = 56  # Acceptable margin for t_diff

    # Parse
    output_name = file_path.split('.')[0]
    data = parse_file(file_path, headers)

    # Manipulate
    sync_df = pd.DataFrame(data)
    sync_df.dropna(inplace=True)

    sync_df['Timestamp'] = pd.to_datetime(sync_df['Timestamp'], format='%Y-%m-%d %H:%M:%S.%f')
    sync_df['Timestamp'] = (sync_df['Timestamp'] - pd.Timestamp("1970-01-01")) // pd.Timedelta("1ns")  # To UNIX Time

    # create default index if necessary
    sync_df = sync_df.reset_index(drop=True)

    # Get Time and Packet Difference
    sync_df.dropna(inplace=True)  # Drop empties, prevents converting SYNC to numeric, only removes the first sync
    sync_df['Data'] = pd.to_numeric(sync_df['Data'], errors='coerce')
    sync_df['Packet_Number'] = pd.to_numeric(sync_df['Packet_Number'])
    sync_df['Timestamp'] = pd.to_datetime(sync_df['Timestamp'], unit='ns')  # Put timestamp back to readable format

    # Filter the dataframe to keep only rows with DATA equal to 1
    filtered_df = sync_df[sync_df['Data'] == 1]

    # Calculate the packet difference between consecutive occurrences of 1
    filtered_df['Packet_Diff'] = filtered_df['Packet_Number'].diff()
    average_packet_diff = filtered_df['Packet_Diff'].mean()
    # Output
    output_file = open(f'{output_name}.csv', 'w')
    sync_df.to_csv(f'{output_name}.csv', index=False)
    print(f'The average interval between node 1 transmissions is: {average_packet_diff}')
    output_file.close()


if __name__ == "__main__":
    # Input
    directory = input("Enter directory containing files to analyze: ")
    timeslot = input("Enter the timeslot: ")
    # Hardcoded keys, shouldn't be, but I'm lazy (fix if needed)
    keys = ['Timestamp', 'Packet_Number', 'Data']
    # Get list of all files in directory
    files = [f for f in os.listdir(directory) if f.endswith('.txt')]

    # Process files sequentially
    for file in files:
        try:
            process_file(os.path.join(directory, file), keys, int(timeslot))
            print(f"Analyzed: {file}")
        except KeyError:
            print(f"FAILED on: {file}")
