# TDMA_Tooling

A set of tools mostly for analysis of TDMA network data. This README serves as a reminder for me to know what each
script does as not even I know what is going on. If you are tasked with modifying these codes, see previous sentence,
I am deeply sorry :). Don't worry it'll buff.

TODO: Attach examples of outputs and proper input file formats.

## node_resync_time.py

Calculates the average interval between transmissions from a node. If you are using this script on 100% energy
harvesting rate take a few steps back and think about what you did...

## EH_trial_analyzer.py

This script is used to analyze the output from the energy harvesting version of the network. Since there are no
guarantees that the coordinator will detect true errors, this script checks for those errors.
Let's begin with the logic! First, it does the typical import and parse of each file, iteratively, in the inputted
directory. Some "touch-ups" are done to the data, namely timestamps, to improve readability. The time difference
and packet number difference is calculated in stored in their own columns. The repeated column will be true
any time the difference between two packets is 0. If there are two repeating packets and the time difference between
them is less than 2 times the timeslot, minus an error margin, then an error has occurred on the coordinator. **File is
finally outputted as a .csv. For convince the number of errors and number of repeated packets are printed to the
console.

## sync_frequency_analyzer.py

Script is used to manipulate captured packets, reducing log file to just SYNC
messages. This can be used for finding the interval between synchronizations. Output Columns: Timestamp, Packet number,
Data, (internal) isSync.
Input a directory of *.txt files.

## repeat_analyzer.py

Script takes in *.txt file containing network data. Outputs a .csv file and graph showing the number of repeats that
occurred in the network.
Input individual *.txt files.

## xbee_packet_sniffer.py

Uses pyserial to read data from XBee module and output to *.txt file. Outputted data is in a comma-delimiter format,
although not stored as .csv. **probably should be

Output file has the following format:  
``| Timestamp | Packet Number | Data |
| ----------- | ------------- |----- |  
| Time01 | 0 | N1 |  
| Time | 1 | N2 |  ``

## parse_utils.py
Helper functions that are used to assist other scripts with file parsing. Does not perform any manipulation of data.

## Other Scripts
If script name is marked as *_DEP.py then it has been deprecated and should not be used
