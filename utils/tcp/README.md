# TCP Benchmarks 

This directory provides wrappers to run simple network benchmarks, using the iperf3 and netperf tools. iperf3 is used as a throughput-intensive application, and network as a latency-sensitive application.

+ iperf3 can be downloaded from: https://github.com/esnet/iperf
+ netperf can be downloaded from: https://github.com/HewlettPackard/netperf 

## Required patch for netperf

Before installing netperf, use the supplied patch **netperf-logging.diff** to the latest netperf commit (commit: 3bc455b23f901dae377ca0a558e1e32aa56b31c4 dated: Thu Jan 21 17:02:24 2021 +0100 branch: master) to provide higher tail latencies (99.9 and 99.99 percentile values), which are not provided by default by netperf.

After cloning the netperf repo, apply the patch from inside the netperf/ directory:

```
git apply netperf-logging.diff

```

Then follow the same instructions to compile as provided in the netperf repo.
