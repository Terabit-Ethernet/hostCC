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
#include "SKX_IMC_BDF_Offset.h"



extern struct task_struct *app_pid_task;
u64 cur_tsc_sample_pre;
u64 cur_tsc_sample_post;
u64 extra_time_needed;

static struct sched_param {
  int sched_priority;
  int sched_policy;
};

// extern char *sh_mem;
extern struct pid *app_pid_struct;
struct timespec64 curr_time;
char time_str[32];
u64 sched_time;

extern u64 last_changed_level_tsc = 0;

// memory bandwidth logging
unsigned int *mmconfig_ptr;         // must be pointer to 32-bit int so compiler will generate 32-bit loads and stores
uint64_t imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
uint64_t prev_imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
uint64_t cur_imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
extern uint64_t latest_avg_rd_bw = 0;
extern uint64_t latest_avg_wr_bw = 0;

// kthread scheduling vars
// void thread_fun_poll_iio(struct work_struct *work);
// void thread_fun_poll_iio_rd(struct work_struct *work);
// void thread_fun_poll_mba(struct work_struct* work);

// static struct task_struct *thread_iio;
// static struct task_struct *thread_iio_rd;
// static struct task_struct *thread_mba;

extern int target_pid = 0;
extern int target_pcie_thresh = 84;
extern int target_iio_thresh = 70;
extern int target_iio_rd_thresh = 190;
extern int mode = 0; //mode = 0 => Rx; mode = 1 => Tx

// IIO Rd occupancy related vars
#define IIO_MSR_PMON_CTL_BASE 0x0A48L
#define IIO_MSR_PMON_CTR_BASE 0x0A41L
#define IIO_OCC_VAL 0x00004000004001D5
#define CORE_IIO_RD 24
uint64_t cum_occ_sample_rd;
uint64_t prev_cum_occ_rd;
uint64_t cur_cum_occ_rd;
uint64_t prev_rdtsc_iio_rd = 0;
extern uint64_t cur_rdtsc_iio_rd = 0;
uint64_t tsc_sample_iio_rd = 0;
extern uint64_t latest_avg_occ_rd = 0;
extern uint64_t smoothed_avg_occ_rd = 0;
extern uint64_t latest_time_delta_iio_rd_ns = 0;

// IIO occupancy related vars
#define IRP_MSR_PMON_CTL_BASE 0x0A5BL
#define IRP_MSR_PMON_CTR_BASE 0x0A59L
#define IRP_OCC_VAL 0x0040040F
#define STACK 2 //We're concerned with stack #2 on our machine
#define CORE_IIO 24
#define IIO_COUNTER_OFFSET 0
uint64_t prev_rdtsc_iio = 0;
extern uint64_t cur_rdtsc_iio = 0;
uint64_t prev_cum_occ = 0;
uint64_t cur_cum_occ = 0;
uint64_t tsc_sample_iio = 0;
uint64_t cum_occ_sample = 0;
extern uint64_t latest_avg_occ = 0;
extern uint64_t smoothed_avg_occ = 0;
extern uint64_t latest_time_delta_iio_ns = 0;

// PCIe bandwidth and MBA update related vars

#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define IIO_PCIE_1_PORT_0_BW_OUT 0x0B24 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)


uint64_t prev_rdtsc_mba = 0;
extern uint64_t cur_rdtsc_mba = 0;
uint64_t tsc_sample_mba = 0;
extern uint32_t latest_mba_val = 0;
extern uint64_t latest_time_delta_mba_ns = 0;
extern uint32_t latest_measured_avg_occ = 0;
extern uint32_t latest_measured_avg_occ_rd = 0;
extern uint32_t latest_avg_pcie_bw = 0;
extern uint32_t smoothed_avg_pcie_bw = 0;
uint64_t cur_cum_frc = 0;
uint64_t prev_cum_frc = 0;
uint64_t cum_frc_sample = 0;
extern uint32_t latest_avg_pcie_bw_rd = 0;
extern uint32_t smoothed_avg_pcie_bw_rd = 0;
uint64_t cur_cum_frc_rd = 0;
uint64_t prev_cum_frc_rd = 0;
uint64_t cum_frc_rd_sample = 0;
extern uint32_t app_pid = 0;
extern uint64_t last_reduced_tsc = 0;

void update_iio_rd_occ_ctl_reg(void);
void sample_iio_rd_occ_counter(int c);
void sample_iio_rd_time_counter(void);
void sample_counters_iio_rd(int c);
void update_iio_rd_occ(void);
// void poll_iio_rd_init(void);
// void poll_iio_rd_exit(void);
// void thread_fun_poll_iio_rd(struct work_struct *work);
void update_iio_occ_ctl_reg(void);
void sample_iio_occ_counter(int c);
void sample_iio_time_counter(void);
void sample_counters_iio(int c);
void update_iio_occ(void);
// void poll_iio_init(void);
// void poll_iio_exit(void);
// void thread_fun_poll_iio(struct work_struct *work);
void sample_pcie_bw_counter(int c);
void sample_mba_time_counter(void);
uint32_t PCI_cfg_index(unsigned int Bus, unsigned int Device, unsigned int Function, unsigned int Offset);
void sample_imc_counters(void);
void sample_counters_pcie_bw(int c);
void update_pcie_bw(void);
void update_imc_bw(void);
void update_imc_config(void);
void init_mmconfig(void);
// void poll_mba_init(void);
// void poll_mba_exit(void);
// void thread_fun_poll_mba(struct work_struct *work);

#endif