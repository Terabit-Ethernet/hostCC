import sys

INPUT_FILE = sys.argv[1]

cpu_util = {}
num_samples = {}

with open(INPUT_FILE) as f1:
    for line in f1:
        elements = line.split()
        if len(elements) == 9 and elements[2] != "CPU":
            cpu = int(elements[2])
            util = float(elements[8])
            if cpu not in cpu_util:
                cpu_util[cpu] = (100 - util)
                num_samples[cpu] = 1
            else:
                cpu_util[cpu] += (100 - util)
                num_samples[cpu] += 1

total_util = 0
num_cpus = 0
for cpu in cpu_util:
    if num_samples[cpu] != 0:
        cpu_util[cpu] /= num_samples[cpu]
        total_util += cpu_util[cpu]
        num_cpus += 1

print("cpu_utils: ",cpu_util)
print("num_samples: ",num_samples)
print("avg_cpu_util: ",total_util/num_cpus)
