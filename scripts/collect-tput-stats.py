import sys
import numpy as np
import statistics
import subprocess

EXP_NAME = sys.argv[1]
NUM_RUNS = int(sys.argv[2])
COLLECT_MLC_TPUT = int(sys.argv[3])

FILE_NAME = "../utils/reports/" + EXP_NAME
command = 'mkdir -p ' + FILE_NAME
result = subprocess.run(command, shell=True, capture_output=True, text=True)


net_tputs = []
retx_rates = []
mem_bws = []
mlc_tputs = []

for i in range(NUM_RUNS):
    with open(FILE_NAME + '-RUN-' + str(i) + '/iperf.bw.rpt') as f1:
        for line in f1:
            tput = float(line.split()[-1])
            if(tput > 0):
                net_tputs.append(tput)
            break
    
    with open(FILE_NAME + '-RUN-' + str(i) + '/retx.rpt') as f1:
        for line in f1:
            line_str = line.split()
            if(line_str[0] != 'Retx_percent:'):
                continue
            else:
                retx_pct = float(line_str[-1])
                if(retx_pct >= 0):
                    retx_rates.append(retx_pct)
                break

    with open(FILE_NAME + '-RUN-' + str(i) + '/membw.rpt') as f1:
        for line in f1:
            line_str = line.split()
            if(line_str[0] != 'Node0_total_bw:'):
                continue
            else:
                membw = float(line_str[-1])
                if(membw >= 0):
                    mem_bws.append(membw)
                break

    if(COLLECT_MLC_TPUT > 0):
        with open(FILE_NAME + '-MLCRUN-' + str(i) + '/mlc.log') as f1:
            for line in f1:
                if line.startswith(' 00000'):
                    tput = float(line.split()[-1])
                    if(tput >= 0):
                        mlc_tputs.append(tput)
                    break


net_tput_mean = statistics.mean(net_tputs)
net_tput_stddev = statistics.stdev(net_tputs)

retx_rate_mean = statistics.mean(retx_rates)
retx_rate_stddev = statistics.stdev(retx_rates)

mem_bw_mean = statistics.mean(mem_bws)
mem_bw_stddev = statistics.stdev(mem_bws)

mlc_tput_mean = 0
mlc_tput_stddev = 0

if(COLLECT_MLC_TPUT > 0):
    mlc_tput_mean = statistics.mean(mlc_tputs)
    mlc_tput_stddev = statistics.stdev(mlc_tputs)


output_list = [net_tput_mean, net_tput_stddev, retx_rate_mean, retx_rate_stddev, mem_bw_mean, mem_bw_stddev, mlc_tput_mean, mlc_tput_stddev]

# Save array to DAT file
np.savetxt(FILE_NAME + '/tput_metrics.dat', output_list, fmt='%.10f')




