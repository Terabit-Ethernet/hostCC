#ifndef HOSTCC_SIGNALS_H
#define HOSTCC_SIGNALS_H
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/threads.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>



extern u64 last_changed_level_tsc;

// memory bandwidth logging
extern unsigned int *mmconfig_ptr;         // must be pointer to 32-bit int so compiler will generate 32-bit loads and stores
extern uint64_t latest_avg_rd_bw;
extern uint64_t latest_avg_wr_bw;

extern int target_pid;
extern int target_pcie_thresh;
extern int target_iio_wr_thresh;
extern int target_iio_rd_thresh;
extern int mode; //mode = 0 => Rx; mode = 1 => Tx

// IIO Rd occupancy related vars
#define IIO_MSR_PMON_CTL_BASE 0x0A48L
#define IIO_MSR_PMON_CTR_BASE 0x0A41L
#define IIO_OCC_VAL 0x00004000004001D5
#define CORE_IIO_RD 24
#define IIO_RD_COUNTER_OFFSET 2
extern uint64_t cur_rdtsc_iio_rd;
extern uint64_t latest_avg_occ_rd;
extern uint64_t smoothed_avg_occ_rd;
extern uint64_t latest_time_delta_iio_rd_ns;

// IIO Wr occupancy related vars
#define IRP_MSR_PMON_CTL_BASE 0x0A5BL
#define IRP_MSR_PMON_CTR_BASE 0x0A59L
#define IRP_OCC_VAL 0x0040040F
#define STACK 2 //We're concerned with stack #2 on our machine
#define CORE_IIO 24
#define IIO_WR_COUNTER_OFFSET 0
extern uint64_t cur_rdtsc_iio_wr;
extern uint64_t latest_avg_occ_wr;
extern uint64_t smoothed_avg_occ_wr;
extern uint64_t latest_time_delta_iio_wr_ns;

// PCIe bandwidth and MBA update related vars

#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define IIO_PCIE_1_PORT_0_BW_OUT 0x0B24 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)



extern uint64_t cur_rdtsc_mba;

extern uint32_t latest_mba_val;
extern uint64_t latest_time_delta_mba_ns;
extern uint32_t latest_measured_avg_occ_wr;
extern uint32_t latest_measured_avg_occ_rd;
extern uint32_t latest_avg_pcie_bw;
extern uint32_t smoothed_avg_pcie_bw;

extern uint32_t latest_avg_pcie_bw_rd;
extern uint32_t smoothed_avg_pcie_bw_rd;

extern uint32_t app_pid;
extern uint64_t last_reduced_tsc;

void update_iio_rd_occ_ctl_reg(void);
void sample_iio_rd_occ_counter(int c);
void sample_iio_rd_time_counter(void);
void sample_counters_iio_rd(int c);
void update_iio_rd_occ(void);
void update_iio_wr_occ_ctl_reg(void);
void sample_iio_wr_occ_counter(int c);
void sample_iio_wr_time_counter(void);
void sample_counters_iio_wr(int c);
void update_iio_wr_occ(void);
void sample_pcie_bw_counter(int c);
void sample_mba_time_counter(void);
uint32_t PCI_cfg_index(unsigned int Bus, unsigned int Device, unsigned int Function, unsigned int Offset);
void sample_imc_counters(void);
void sample_counters_pcie_bw(int c);
void update_pcie_bw(void);
void update_imc_bw(void);
void update_imc_config(void);
void init_mmconfig(void);

#endif