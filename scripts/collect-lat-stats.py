import sys
import numpy as np
import statistics
import subprocess
import csv

EXP_NAME = sys.argv[1]
NUM_RUNS = int(sys.argv[2])

FILE_NAME = "../utils/reports/" + EXP_NAME
command = 'mkdir -p ' + FILE_NAME
result = subprocess.run(command, shell=True, capture_output=True, text=True)


p50_latencies = {}
p90_latencies = {}
p99_latencies = {}
p999_latencies = {}
p9999_latencies = {}

RPC_SIZES = [128,512,2048,8192,32768]


for rpc in RPC_SIZES:
    p50_latencies[rpc] = []
    p90_latencies[rpc] = []
    p99_latencies[rpc] = []
    p999_latencies[rpc] = []
    p9999_latencies[rpc] = []
    for i in range(NUM_RUNS):
        with open(FILE_NAME + '-LATRUN-' + str(i) + '/netperf-' + str(rpc) + '.lat.rpt') as f1:
            for line in f1:
                if line.startswith('50%'):
                    lat = float(line.split()[-1])
                    if(lat >= 0):
                        p50_latencies[rpc].append(lat)
                if line.startswith('90%'):
                    lat = float(line.split()[-1])
                    if(lat >= 0):
                        p90_latencies[rpc].append(lat)
                if line.startswith('99%'):
                    lat = float(line.split()[-1])
                    if(lat >= 0):
                        p99_latencies[rpc].append(lat)
                if line.startswith('99.9%'):
                    lat = float(line.split()[-1])
                    if(lat >= 0):
                        p999_latencies[rpc].append(lat)
                if line.startswith('99.99%'):
                    lat = float(line.split()[-1])
                    if(lat >= 0):
                        p9999_latencies[rpc].append(lat)
                    

data = []
data.append([statistics.mean(p50_latencies[i]) for i in RPC_SIZES])
data.append([statistics.mean(p90_latencies[i]) for i in RPC_SIZES])
data.append([statistics.mean(p99_latencies[i]) for i in RPC_SIZES])
data.append([statistics.mean(p999_latencies[i]) for i in RPC_SIZES])
data.append([statistics.mean(p9999_latencies[i]) for i in RPC_SIZES])

with open(FILE_NAME + '/lat_metrics.dat', 'w', newline='') as file:
    writer = csv.writer(file)

    # Write the column headings
    writer.writerow([str(i) for i in RPC_SIZES])

    # Write the data rows
    writer.writerows(data)




