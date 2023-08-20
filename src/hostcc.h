#ifndef HOSTCC_H
#define HOSTCC_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/threads.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <net/ip.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/random.h>
#include "vars.h"
#include "intel-cascadelake-params.h"

static inline __attribute__((always_inline)) unsigned long rdtscp(void)
{
   unsigned long a, d, c;

   __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

   return (a | (d << 32));
}

extern struct task_struct *app_pid_task;
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
extern uint64_t cur_rdtsc_nf;
extern uint64_t latest_measured_avg_occ_wr_nf;
extern uint64_t latest_measured_avg_occ_rd_nf;
extern uint64_t latest_time_delta_nf_ns;
extern uint32_t latest_datagram_len;
extern bool terminate_hcc;
extern bool terminate_hcc_logging;

#endif





