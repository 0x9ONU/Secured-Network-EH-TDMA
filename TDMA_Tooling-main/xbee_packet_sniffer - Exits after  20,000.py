from datetime import datetime

import serial

if __name__ == "__main__":
    count = 0
    output_file = input("Enter output file name: ")
    port = input("Enter serial port: ")
    xbee = serial.Serial(
        port=port,
        baudrate=9600,
    )
    f = open(f'{output_file}.txt', 'w')
    try:
        while count < 20000:
            line = xbee.readline()
            count = count + 1
            data = f'{line.decode().rstrip()}'
            f.write(f'{datetime.now()}, {count}, {data}\n')
            print(f'{datetime.now()}, {count}, {data}')
        f.close()
        exit(0)
    except KeyboardInterrupt:
        f.close()
        exit(1)
