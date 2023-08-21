# Parameters needed to build hostCC

## NIC parameters

+ "NIC_INTERFACE": Name of the interface used to send/receive packets (for eg., eno100)
+ "NIC_LOCAL_SOCKET": NUMA node which contains the NIC; usually it is zero, but can be checked using lshw
+ "NIC_IIO_STACK": The "IIO stack" number containing the PCIe lanes attached to the NIC. Cascadelake architecture has IIO stacks on each NUMA node. To find the correct stack number, run ./pcm-iio from [Intel PCM](https://github.com/intel/pcm), and inspect the list of devices written below the row corresponding to each IIO stack

## Configuring Memory Bandwidth Allocation (MBA) response

Current implementation by default uses the same setup as the SIGCOMM'23 paper -- server containing 4 NUMA nodes, with 8 cores per NUMA node, and with NIC attached to NUMA node 0 and network traffic running on NUMA 0 cores {0,4,8,12,16,24,28}. We also assume memory-intensive app is running on cores in remote NUMA nodes, and generating traffic targeted to memory location in NUMA 0 (hence contending with the network traffic). We use Intel Memory Bandwidth Allocation (MBA) tool to perform host-local response. Configuring MBA response is a two step process:

+ **Configuring MBA Class of Service:** MBA maintains upto 8 classes of service (COS) independently for each NUMA node -- each COS within a NUMA node is identified by a COS ID (0-7) and can be assigned/revoked a CPU core from within the corresponding NUMA node. By default, all CPU cores are assigned to COS ID 0. We provide helper scripts in *utils/* directory set CPU cores 1,2,3 (which belong to NUMA nodes 1,2 and 3 respectively) to MBA COS ID 1 on each respective NUMA node.

+ **Configuring MBA High/Low Values:** MBA performs rate-limiting for CPU-memory traffic for all CPU cores belonging to the same COS within a NUMA node at the same time. The rate-limiting is performed by performing an MSR write to an MBA specific register for any core belonging to the target NUMA node (for our case, we use cores 29,30,31 as default cores to represent NUMA nodes 1,2 and 3). Specifying the correct register requires an offset which depends upon the chosen COS ID. And the value written to the register specifies the amount of rate limiting performed: the value written can be from {0,10,20,...,90} -- larger value means lower CPU-memory traffic rate. 

### Currently employed MBA levels

Current implementation assumes MApp traffic to be rate-limited is generated using cores 1,2,3. All cores are assigned COS ID of 1 in their respective NUMA nodes (since MBA is performed independetly for each NUMA node, they could have also been different values without loss of generality). Therefore it uses following 4 MBA level configurations:

+ Level 0: All NUMA node MSRs have value = 0 written for COS 1 (thus, no rate limiting for MApp traffic)
+ Level 1: Only MSR for NUMA node 1 have value = 90 written for COS 1 (max hardware supported rate-limiting for MApp traffic from core 1)
+ Level 2: MSRs for NUMA nodes 1 & 2 have value = 90 written for COS 1 (max hardware supported rate-limiting for MApp traffic from cores 1 & 2)
+ Level 3: MSRs for NUMA nodes 1,2 & 3 have value = 90 written for COS 1 (max hardware supported rate-limiting for MApp traffic from cores 1,2 & 3)

Therefore, to go to a higher level, MBA_VAL_HIGH is written to an MSR, and to reduce to a lower level MBA_VAL_LOW is written to an MSR. One could change this logic by editing the *hostcc-local-response.c* file, and default high/low values by editing the *config.json* file.

Summary for default MBA config:
+ "MBA_LEVEL_1_CORE": Any core within NUMA node 1,
+ "MBA_LEVEL_2_CORE": Any core within NUMA node 2,
+ "MBA_LEVEL_3_CORE": Any core within NUMA node 3,
+ "MBA_VAL_HIGH": Can be any multiple of 10 ranging from 10 to 90; recommended to use 90,
+ "MBA_VAL_LOW": Should be 0 with curent config,
+ "MBA_COS_ID": Can be any value between 1-7 (and not 0),

#### Usage instructions for dual/singal socket machines

If your server does not have 4 NUMA nodes, or your intend to use MApp on cores belonging to 1 or 2 NUMA nodes, you may have to create multiple COS IDs within a single NUMA node to achieve similar MBA response. This can be done by making minor modifications in the *hostcc-local-response.c* file in the methods *increase_mba_val()* and *decrease_mba_val()*.  

### Using process scheduling to overcome current hardware limitations

Current MBA implementation only introduces a fixed maximum hardware supported rate-throttling for CPU-memory traffic (when using the max possible value of 90 above). This may not be enough to achieve desired network throughput for the NetApp. Therefore we also provide an aditional MBA level 4 (inidicated by the flag USE_PROCESS_SCHEDULER) which takes as input PID for a process (see command line inputs below), and sends a SIGSTOP signal to the process when hostCC's local response requires going to the level 4. This is equivalent to completely throttling the rate at which the process cores generate memory traffic. When hostCC intends to go back to level 3, it then sends a SIGCONT signal to the process to resume its execution. Current implementation assumes that there is a single MApp process, and is available when hostCC module is inserted. 

+ "USE_PROCESS_SCHEDULER": Should be 1 if you intend to use hostCC's additional local response level,

## Signal measurement/logging parameters

+ "IIO_CORE": Core used to measure IIO occupancy values. Must be located local to the NUMA node containing the NIC.
+ "IIO_LOGGING": Set to 1 if desired to dump IIO occupancy log to the kernel message buffer upon exiting hostCC. Set to 0 otherwise.
+ "PCIE_CORE": Core used to measure PCIe bandwidth. Must be located local to the NUMA node containing the NIC.
+ "PCIE_LOGGING": Set to 1 if desired to dump PCIe bandwidth log to the kernel message buffer upon exiting hostCC. Set to 0 otherwise.
+ "ECN_LOGGING": Set to 1 if desired to ECN marking log to the kernel message buffer upon exiting hostCC. Set to 0 otherwise.
+ "LOG_SIZE": Number of entries for each log (keep in mind the kernel message buffer is limited in size, and may roll over in case the log size is set too high. Recommended to use few thousand entries.)


## Command-line parameters to run hostCC

hostCC uses following command line input parameters:
+ target_iio_wr_threshold: used to change the default value of 70 for IIO occupancy threshold (for Rx datapath)
+ target_iio_rd_threshold: used to change the default value of 190 for IIO occupancy threshold (for Tx datapath)
+ target_pcie_threshold: used to change the default value of 84 for PCIe bandwidth threshold
+ mode: 0 (default) means hostCC is running in Rx datapath mode, and 1 means hostCC is running in Tx datapath mode

### Building and Running hostCC

After configuring the config.json file, make hostCC by running make from within the src/ directory.

```
sudo make
```

To run at the receiver-side, use following command from within the src/ directory:

```
sudo insmod hostcc-module.ko mode=0 target_iio_wr_threshold=70 target_pcie_threshold=84
```

To run at the sender-side, use the following command:

```
sudo insmod hostcc-module.ko mode=0 target_iio_rd_threshold=190 target_pcie_threshold=84
```

to stop running hostCC at either side:

```
sudo rmmod hostcc-module
```
