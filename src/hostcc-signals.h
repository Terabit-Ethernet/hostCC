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
#include "hostcc-logging.h"
#include "hostcc-local-response.h"
#include "hostcc.h"

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