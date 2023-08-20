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
extern int target_pid;
extern int target_pcie_thresh;
extern int target_iio_wr_thresh;
extern int target_iio_rd_thresh;
extern int mode; //mode = 0 => Rx; mode = 1 => Tx
extern uint64_t cur_rdtsc_iio_rd;
extern uint64_t latest_avg_occ_rd;
extern uint64_t smoothed_avg_occ_rd;
extern uint64_t latest_time_delta_iio_rd_ns;
extern uint64_t cur_rdtsc_iio_wr;
extern uint64_t latest_avg_occ_wr;
extern uint64_t smoothed_avg_occ_wr;
extern uint64_t latest_time_delta_iio_wr_ns;
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
void sample_counters_pcie_bw(int c);
void update_pcie_bw(void);

#endif