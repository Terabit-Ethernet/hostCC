# hostCC

**Host congestion.** Recent adoption of high-bandwidth links, along with relatively stagnant technology trends for intra-host resources (like cache sizes, memory bandwidth per core, NIC buffer sizes, etc) have led to emergence of new bottlenecks within the host-interconnect---the network within the host that enables communication between the NIC and CPU/memory. This problem of host congestion has been observed at various major cloud providers running state-of-the-art network stacks ([Google](https://dl.acm.org/doi/abs/10.1145/3563766.3564110), [Alibaba](https://www.usenix.org/system/files/fast23-li-qiang_more.pdf) and [ByteDance](https://www.usenix.org/system/files/nsdi23-liu-kefei.pdf)). Our [SIGCOMM'23 paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf) also reproduces the problem in the Linux network stack, over our local testbed.

**hostCC** is a congestion control architecture proposed to effectively handle host congestion along with network fabric congestion. hostCC embodies three key design ideas: 

(i) in addition to congestion signals originating within the network fabric (for eg., ECNs or delay computed using switch buffer occupancies), hostCC collects *host congestion signals*---indicating the precise time, location and reason for host congestion; 

(ii) in addition to the network congestion response performed by conventional congestion control protocols, hostCC performs *host-local congestion response*, at a *sub-RTT granularity*; and 

(iii) hostCC employs both network congestion signals and host congestion signals in order to perform effective network congestion response.


## Overview

### Repository overview

+ *src/* contains the hostCC implementation logic (more details below)
+ *utils/* contains necessary tools to recreate host congestion scenarios (using memory-intensive apps), and to measure app-level metrics (throughput, latency or packet drops for network apps) and host-level metrics (PCIe bandwidth, memory bandwidth, etc)
+ *scripts/* contains necessary scripts to run similar experiments as in the [SIGCOMM'23 paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf)
+ *tests/* contains unit tests and some toy experiments to validate hostCC is working as intended

### hostCC implementation overview
The *src/* directory contains three files, each implementing one of the key hostCC design elements: 
+ *hostcc-signals.c*: implements the logic to collect host congestion signals at sub-microsecond scale.
+ *hostcc-local-response.c*: implements the logic to perform host-local resource allocation at sub-RTT scale.
+ *hostcc-network-response.c*: implements the logic to echo back the host congestion signal to the network sender.

## Running hostCC

Note: Current implementation requires Intel CascadeLake architecture to run correctly. The MSR locations needed for host congestion signals and host-local response with newer architectures will be different. We plan to add support for newer architectures soon (see planned extensions below).

Specify the required system specific inputs in the *scripts/launch.config* file (for eg., the cores to run memory-intensive apps and network apps respectively, PCIe slots containing the NIC, etc.). Detailed description for each parameter (and how to obtain the desired input for specific hardware) is provided inline in the config file. 
```
vim scripts/launch.config
```

Disable logging using the config file (by specifying 0 in the field "logging") to optimize for performance. The logfiles (containing sampled system states as sub-microsecond scale) are otherwise stored in the a directory *logs/*.

Desired environmental setup settings, for eg., DDIO, MTU size, TCP optimizations like TSO/GRO/aRFS (currently TCP optimizations can be configured using our script only for Mellanox CX5 NICs) can tuned using the following script. Run the script with -h flag to get list of all parameters, their default values, and how to 
```
sudo bash utils/setup-envir.sh -h
```

Then build hostCC by simply running
```
make
```
This will produce a loadable kernel module. Running hostCC using the following command simply loads this module with the desired parameters computed from the config file parameters:
```
sudo bash scripts/run-hostcc.sh
```
The following command will stop running hostCC
```
sudo bash scripts/stop-hostcc.sh
```

[Optional] If you're able to successfully configure, build and run hostCC, you can test whether its working as intended using some simple tests: cd to the *tests/* directory, and follow the README instructions there.


## Reproducing results in the [SIGCOMM'23 paper](www.google.com)

To run the same experiments as in the paper, run the following script, specifying the figure number for the corresponding experiment in the paper:
```
sudo bash scripts/run-hostCC-experiments.sh --figno=<FIGNO>
```
Specify FIGNO=ALL to simply run all experiments in the paper. The generated figures are stored in a */hostCC-experiments/* directory.

Make sure to edit the config file *scripts/launch.config* before running the experiments.


### Factors affecting reproducibality
The results are sensitive to the setup, and any difference in the setup for following factors may lead to results different from the paper. A list of potential factors:
+ Processor: Number of NUMA nodes, whether hyperthreading is enabled, whether hardware prefetching is enabled, L1/L2/LLC sizes, CPU clock frequency, etc
+ Memory: DRAM generation (DDR3/DDR4/DDR5), DRAM frequency, number of DRAM channels per NUMA node
+ Network: NIC hardware (for eg., Mellanox CX5), NIC driver (for eg., OFED version for Mellanox NICs), MTU size
+ Boot settings: whether IOMMU is enabled, whether hugepages are enabled
+ Linux kernel version
+ Topology: minimum RTT between the servers
+ Optimizations: whether Linux network stack optimizations like TSO/GRO/aRFS are enabed
+ DCTCP parameters: Tx/Rx socket buffer sizes, DCTCP alpha, whether delayed ACKs is enabled

We specify the parameters used for our hostCC evaluation in *utils/parameters.txt*.


## Current limitations, and planned extensions

*Additional signals and CC protocols.* Current implementation only uses IIO occupancy as host congestion signal (which is echoed back to the network sender via ECNs). We intend to extend hostCC to support additional signals (like host-interconnect delay and NIC buffer occupancy), allowing (a) comparing efficacy of other signals against IIO occupancy, and (b) incorporating additional congestion control protocols to make use of hostCC architecture.

*Additional allocation policies.* Current repo only provides a single example host resource allocation policy, providing utilization guarantees only for the network traffic. We plan to incorporate additional policies like max-min fairness and providing guranteees for host-local traffic in the near future.

*Additional hardware support.* Current repo only includes scripts for the setup described in [hostCC paper](https://www.cs.cornell.edu/~ragarwal/pubs/hostcc.pdf). We plan to add support for additional Intel architectures (Icelake and Sapphire Rapids) in the near future.

*Additional optimizations.* Current implementation uses two dedicated cores to sample host congestion signals, and perform sub-RTT response. We plan to add possible optimizations (for eg., inline sampling via cores used for network stack processing) to reduce this overhead. However, note that the number of dedicated cores will remain the same even with increasing access link bandwidths, hence the implementation is already in-principle quite scalable. We plan to test the implementation with higher bandwidth NICs once we gain access to the hardware in the future.


## Contributing to hostCC
hostCC codebase is relatively simple (<1000 LOC) with clear separation of logic between the implementation of three hostCC design ideas. Feel free to experiment with hostCC and send a PR with a proposed extension which may be useful to the community.

## Contact
Saksham Agarwal (saksham@cs.cornell.edu)
