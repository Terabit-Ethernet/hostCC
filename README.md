# hostCC

**Host congestion.** Recent adoption of high-bandwidth links, along with relatively stagnant technology trends for intra-host resources (like cache sizes, memory bandwidth per core, NIC buffer sizes, etc) have led to emergence of new bottlenecks within the host-interconnect---the network within the host that enables communication between the NIC and CPU/memory. This problem of host congestion has been observed at various major cloud providers running state-of-the-art network stacks ([Google](https://dl.acm.org/doi/abs/10.1145/3563766.3564110), [Alibaba](https://www.usenix.org/system/files/fast23-li-qiang_more.pdf) and [ByteDance](https://www.usenix.org/system/files/nsdi23-liu-kefei.pdf)). Our [SIGCOMM'23 paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf) also reproduces the problem in the Linux network stack, over our local testbed.

**hostCC** is a congestion control architecture proposed to effectively handle host congestion along with network fabric congestion. hostCC embodies three key design ideas: 

(i) in addition to congestion signals originating within the network fabric (for eg., ECNs or delay computed using switch buffer occupancies), hostCC collects *host congestion signals* at *sub-RTT granularity*---indicating the precise time, location and reason for host congestion; 

(ii) in addition to the network congestion response performed by conventional congestion control protocols, hostCC performs *host-local congestion response*, at a *sub-RTT granularity*; and 

(iii) hostCC employs both network congestion signals and host congestion signals in order to perform effective network congestion response.


## Overview

### Repository overview

+ *src/* contains the hostCC implementation logic (more details below)
+ *utils/* contains necessary tools to recreate host congestion scenarios (apps which can create network-bound and host memory-bound traffic), and to measure various useful app-level metrics (throughput, latency or packet drops for network apps) and host-level metrics (PCIe bandwidth, memory bandwidth, etc)
+ *scripts/* contains necessary scripts to run similar experiments as in the [SIGCOMM'23 paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf)

### hostCC implementation overview
The *src/* directory contains following key files:
+ *hostcc.c*: implements the logic to initialize the hostcc kernel module
+ *hostcc-signals.c*: implements the logic to collect host congestion signals at sub-microsecond scale.
+ *hostcc-local-response.c*: implements the logic to perform host-local resource allocation at sub-RTT scale.
+ *hostcc-network-response.c*: implements the logic to echo back the host congestion signal to the network sender.

## Running hostCC

Note: Current implementation requires Intel CascadeLake architecture to run correctly. The MSR locations needed for host congestion signals and host-local response with newer architectures will be different. We plan to add support for newer architectures soon (see planned extensions below). 

### Specifying paramters needed to build hostCC

Specify the required system specific inputs in the *src/config.json* file (for eg., the cores used for collecting host congestion signals, parameters for specifying the granularity of host-local response etc.). More details for each parameter is provided in the README inside the src/ directory. 
```
cd src
vim config.json
```

### Building hostCC

After modifying the config file, build hostCC by simply running
```
make
```
This will produce a loadable kernel module (hostcc-module)

### Running hostCC

One can run hostCC by simply loading the kernel module from within the src/ directory
```
sudo insmod hostcc-module.ko
```
hostCC can also take any user-specified values for IIO occupancy and PCIe bandwidth thresholds (I_T and B_T used in the [paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf)) as command line input. More details provided in the README inside the src/ directory. 

To stop running hostCC simply unload the module
```
sudo rmmod hostcc-module
```

### Installing required utilities

Instructions to install required set of benchmarking applications and measurement tools (for running similar experiments in SIGCOMM'23 paper) is provided in the README in in *utils/* directory. 
+ Benchmarking applications: We use **iperf3** as network app generating throughput-bound traffic, **netperf** as network app generating latency-sensitive traffic, and **mlc** as the CPU app generating memory-intensive traffic.
+ Measurement tools: We use **Intel PCM** for measuring the host-level metrics and **Intel Memory Bandwidth Allocation** tool for performing host-local response. We also use **sar** utility to measure CPU utilization.

### Specifying desired experimental settings

Desired experiment settings, for eg., enabling DDIO, configuring MTU size, number of clients/servers used by the network-bound app, enabling TCP optimizations like TSO/GRO/aRFS (currently TCP optimizations can be configured using the provided script in this repo only for Mellanox CX5 NICs), etc can tuned using the script *utils/setup-envir.sh*. Run the script with -h flag to get list of all parameters, and their default values.  
```
sudo bash utils/setup-envir.sh -h
```



### Running benchmarking experiments

To help run experiments similar to those in SIGCOMM'23 paper, we provide following scripts in the *scripts/* directory:

+ *run-hostcc-tput-experiment.sh*
+ *run-hostcc-latency-experiment.sh*

Run these scripts with -h flag to get list of all possible input parameters, including specifying the home directory (which is assumed to contain hostCC repo), client/server IP addresses, experimental settings as discussed above. The output results are generated in *utils/reports/* directory, and measurement logs are stored in *utils/logs/*.


## Reproducing results in the [SIGCOMM'23 paper](www.google.com)

For ease of use, we also provide wrapper scripts inside the *scripts/SIGCOMM23-experiments* directory which run identical experiments as in the SIGCOMM'23 paper in order to reproduce our key evaluation results. Refer the README in *scripts/SIGCOMM23-experiments/* directory for details on how to run the scripts. 


### Factors affecting reproducibality of results
The results are sensitive to the setup, and any difference in the setup for following factors may lead to results different from the paper. A list of potential factors:
+ Processor: Number of NUMA nodes, whether hyperthreading is enabled, whether hardware prefetching is enabled, L1/L2/LLC sizes, CPU clock frequency, etc
+ Memory: DRAM generation (DDR3/DDR4/DDR5), DRAM frequency, number of DRAM channels per NUMA node
+ Network: NIC hardware (for eg., Mellanox CX5), NIC driver (for eg., OFED version for Mellanox NICs), MTU size
+ Boot settings: whether IOMMU is enabled, whether hugepages are enabled
+ Linux kernel version
+ Topology: minimum RTT between the servers
+ Optimizations: whether Linux network stack optimizations like TSO/GRO/aRFS are enabed
+ DCTCP parameters: Tx/Rx socket buffer sizes, DCTCP alpha, whether delayed ACKs is enabled

We specify the parameters used for our hostCC evaluation setup in *scripts/SIGCOMM23-experiments/default-parameters.txt*.


## Current limitations, and planned extensions

**Additional signals and CC protocols.** Current implementation only uses IIO occupancy as host congestion signal (which is echoed back to the network sender via ECNs). We intend to extend hostCC to support (a) additional signals NIC buffer occupancy when possible on our commodity Nvidia CX-5 NICs, and (b) incorporating additional congestion control protocols other than DCTCP to make use of hostCC architecture.

**Additional allocation policies.** Current repo only provides a single example host resource allocation policy, providing a static (user-specified) utilization guarantee for the network traffic. We plan to incorporate additional policies like max-min fairness between network/host-local traffic and providing guranteees for host-local traffic.

**Additional hardware support.** Current repo only includes scripts for the setup described in [hostCC paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf). We plan to add support for additional Intel /AMD architectures.

**Performance optimizations.** Current implementation uses two dedicated cores to sample host congestion signals, and perform sub-RTT response. We plan to add possible optimizations (for eg., inline sampling via cores used for network stack processing) to reduce this overhead. However, note that the number of dedicated cores will remain the same even with increasing access link bandwidths, hence the implementation is already in-principle quite scalable. We plan to test the implementation with higher bandwidth NICs once we gain access to the hardware.


## Contributing to hostCC
hostCC codebase is relatively simple (<1000 LOC) with clear separation of logic between the implementation of three hostCC design ideas. Feel free to experiment with hostCC and send a PR with a proposed extension which may be useful to the community.

## Contact
Saksham Agarwal (saksham@cs.cornell.edu)
