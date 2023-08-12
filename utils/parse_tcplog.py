import sys
import csv
import os


EXPERIMENT = sys.argv[1]
os.system("mkdir -p " + "logs/" + EXPERIMENT)
os.system("mkdir -p " + "reports/" + EXPERIMENT)
INPUT_FILE = "logs/" + EXPERIMENT + '/tcp.trace.log'
OUTPUT_FILE = "reports/" + EXPERIMENT + '/tcp.trace.csv'

records = []

with open(INPUT_FILE) as f1:
    for line in f1:
        if(line[0] == '#'):
            continue
        else:
            line_str = line.split()
            record = {}

            record['time_s'] = float(line_str[3].split(':')[0])

            src_str = line_str[5]
            src_ip = src_str.split('=')[1].split(':')[0]
            src_port = src_str.split('=')[1].split(':')[1]
            record['src_ip'] = src_ip
            record['src_port'] = int(src_port)

            dst_str = line_str[6]
            dst_ip = dst_str.split('=')[1].split(':')[0]
            dst_port = dst_str.split('=')[1].split(':')[1]
            record['dst_ip'] = dst_ip
            record['dst_port'] = int(dst_port)

            record['snd_nxt'] = int(line_str[9].split('=')[1],base=16)
            record['snd_una'] = int(line_str[10].split('=')[1],base=16)
            record['snd_cwnd'] = int(line_str[11].split('=')[1])
            record['ssthresh'] = int(line_str[12].split('=')[1])
            record['snd_wnd'] = int(line_str[13].split('=')[1])
            record['srtt'] = int(line_str[14].split('=')[1])
            record['rcv_wnd'] = int(line_str[15].split('=')[1])

            records.append(record)

assert(len(records) > 0)
columns = list(records[0].keys())
csv_file = OUTPUT_FILE

try:
    with open(csv_file, 'w') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=columns)
        writer.writeheader()
        for data in records:
            writer.writerow(data)
except IOError:
    print("I/O error")

