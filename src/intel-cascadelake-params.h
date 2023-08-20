#ifndef INTEL_CASCADELAKE_PARAMS_H
#define INTEL_CASCADELAKE_PARAMS_H

// MSR locations for IIO occupancy measurements
// IIO write occupancy
#define IRP_MSR_PMON_CTL_BASE 0x0A5BL
#define IRP_MSR_PMON_CTR_BASE 0x0A59L
#define IRP_OCC_VAL 0x0040040F
#define IIO_WR_COUNTER_OFFSET 0
// IIO read occupancy
#define IIO_MSR_PMON_CTL_BASE 0x0A48L
#define IIO_MSR_PMON_CTR_BASE 0x0A41L
#define IIO_OCC_VAL 0x00004000004001D5
#define IIO_RD_COUNTER_OFFSET 2

// MSR locations for PCIe bandwidth measurements
#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define IIO_PCIE_1_PORT_0_BW_OUT 0x0B24 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)

// MSR location for MBA
#define PQOS_MSR_MBA_MASK_START 0xD50L

#endif


