# Requirements

## Before running the setup script

Use the script *setup-envirs.sh* to tune system parameters as needed. 

Clone the following repos **in the home directory of the user** before running the script.

```
git clone https://github.com/Terabit-Ethernet/Understanding-network-stack-overheads-SIGCOMM-2021
git clone https://github.com/aliireza/ddio-bench.git
```

Follow the instructions in ddio-bench repo to create a binary for enabling/disabling DDIO for the required PCIe slot, with the following minor modification:

Rename the generated binary for enabling ddio to be "change-ddio-on" and disabling ddio to be "change-ddio-off" (instead of the default name "change-ddio" in the README for ddio-bench repo). The setup scripts assumes these binary names in order to enable/disable DDIO.

Install the following tools/libraries in your home directory:

+ Intel PCM: https://github.com/intel/pcm (for collecting PCIe bandwidth and memory bandwidth)
+ Intel MLC: https://www.intel.com/content/www/us/en/developer/articles/tool/intelr-memory-latency-checker.html (to run as memory intensive application)
+ Intel RDT: https://github.com/intel/intel-cmt-cat (to enable Intel MBA)
+ iperf3: https://github.com/esnet/iperf (to use as throughput bound network app)
+ netperf: https://github.com/HewlettPackard/netperf (to use as latency sensitive network app) [Note: use the supplied patch discussed below while installing netperf]
+ [Optional] If you're planning to run RDMA benchmarks, install rdma perftest: https://github.com/linux-rdma/perftest (for running network apps)
+ [Optional] Flamegraph: https://github.com/brendangregg/FlameGraph (optional: for lower level insights into stack bottlenecks)
+ Install sar (for collecting cpu utilization)
```
sudo apt-get install sysstat

```
Edit the /etc/default/sysstat file: change ENABLED="false" to ENABLED="true". Then restart the service:
```
service sysstat restart
```

## Required patch for netperf

Before installing netperf, use the supplied patch **tcp/netperf-logging.diff** to the latest netperf commit (commit: 3bc455b23f901dae377ca0a558e1e32aa56b31c4 dated: Thu Jan 21 17:02:24 2021 +0100 branch: master) to log higher tail latencies (i.e. 99.9 and 99.99 percentile values reported in our SIGCOMM'23 paper), which are not provided by default by netperf.

After cloning the netperf repo, apply the patch from inside the netperf/ directory:

```
git apply netperf-logging.diff

```

Then follow the same instructions to compile as provided in the netperf repo.





