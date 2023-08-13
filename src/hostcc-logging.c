#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/threads.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
// #include "hostcc-signals.h"
// #include "hostcc-network-response.h"
// #include "hostcc-local-response.h"
#include "hostcc-logging.h"

extern uint64_t smoothed_avg_occ_rd;
extern uint64_t smoothed_avg_occ;

extern u64 cur_rdtsc_nf;
extern u64 latest_time_delta_nf_ns;
extern u32 latest_datagram_len;
extern u64 latest_measured_avg_occ_nf;
extern u64 latest_measured_avg_occ_rd_nf;
extern uint64_t latest_time_delta_iio_rd_ns;
extern uint64_t cur_rdtsc_iio;
extern uint64_t latest_avg_occ_rd;
extern uint64_t cur_rdtsc_iio_rd;
extern uint64_t latest_avg_occ;
extern uint64_t latest_time_delta_iio_ns;

extern uint64_t cur_rdtsc_mba;
extern uint64_t latest_time_delta_mba_ns;
extern uint32_t latest_measured_avg_occ;
extern uint32_t latest_measured_avg_occ_rd;
extern uint32_t latest_mba_val;
extern uint32_t smoothed_avg_pcie_bw;
extern uint32_t latest_avg_pcie_bw;

extern uint64_t latest_avg_rd_bw;
extern uint64_t latest_avg_wr_bw;
extern uint32_t smoothed_avg_pcie_bw_rd;
extern uint32_t latest_avg_pcie_bw_rd;

extern struct task_struct *app_pid_task;

struct log_entry_iio LOG_IIO[LOG_SIZE];
struct log_entry_iio_rd LOG_IIO_RD[LOG_SIZE];
struct log_entry_mba LOG_MBA[LOG_SIZE];
struct log_entry_nf LOG_NF[LOG_SIZE];
uint32_t log_index_iio = 0;
uint32_t log_index_iio_rd = 0;
uint32_t log_index_mba = 0;
uint32_t log_index_nf = 0;

void update_log_nf(int c){
	LOG_NF[log_index_nf % LOG_SIZE].l_tsc = cur_rdtsc_nf;
	LOG_NF[log_index_nf % LOG_SIZE].td_ns = latest_time_delta_nf_ns;
	LOG_NF[log_index_nf % LOG_SIZE].m_avg_occ = latest_measured_avg_occ_nf;
	LOG_NF[log_index_nf % LOG_SIZE].cpu = c;
	LOG_NF[log_index_nf % LOG_SIZE].dat_len = latest_datagram_len;
	log_index_nf++;
}

unsigned long long seconds;
unsigned long microseconds;
unsigned long nanoseconds;

void init_nf_log(void){
  int i=0;
  while(i<LOG_SIZE){
    LOG_NF[i].l_tsc = 0;
    LOG_NF[i].td_ns = 0;
    LOG_NF[i].m_avg_occ = 0;
    LOG_NF[i].cpu = 65;
    LOG_NF[i].dat_len = 0;
    i++;
  }
}

void dump_nf_log(void) {
  int i=0;
  while(i<LOG_SIZE){
    printk("NF:%d,%lld,%lld,%lld,%d,%d\n",
    i,
    LOG_NF[i].l_tsc,
    LOG_NF[i].td_ns,
    LOG_NF[i].m_avg_occ,
    LOG_NF[i].cpu,
    LOG_NF[i].dat_len);
    i++;
  }
}

void update_log_iio_rd(int c){
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].l_tsc = cur_rdtsc_iio_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].td_ns = latest_time_delta_iio_rd_ns;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].avg_occ_rd = latest_avg_occ_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].s_avg_occ_rd = (smoothed_avg_occ_rd >> 10);
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].cpu = c;
	log_index_iio_rd++;
}

void init_iio_rd_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_IIO_RD[i].l_tsc = 0;
      LOG_IIO_RD[i].td_ns = 0;
      LOG_IIO_RD[i].avg_occ_rd = 0;
      LOG_IIO_RD[i].s_avg_occ_rd = 0;
      LOG_IIO_RD[i].cpu = 65;
      i++;
  }
}

void dump_iio_rd_log(void){
  int i=0;
  // printk("index,latest_tsc,time_delta_ns,avg_occ,s_avg_occ,cpu\n");
  while(i<LOG_SIZE){
      printk("IIORD:%d,%lld,%lld,%lld,%lld,%d\n",
      i,
      LOG_IIO_RD[i].l_tsc,
      LOG_IIO_RD[i].td_ns,
      LOG_IIO_RD[i].avg_occ_rd,
      LOG_IIO_RD[i].s_avg_occ_rd,
      LOG_IIO_RD[i].cpu);
      i++;
  }
}

void update_log_iio(int c){
	LOG_IIO[log_index_iio % LOG_SIZE].l_tsc = cur_rdtsc_iio;
	LOG_IIO[log_index_iio % LOG_SIZE].td_ns = latest_time_delta_iio_ns;
	LOG_IIO[log_index_iio % LOG_SIZE].avg_occ = latest_avg_occ;
	LOG_IIO[log_index_iio % LOG_SIZE].s_avg_occ = (smoothed_avg_occ >> 10);
	LOG_IIO[log_index_iio % LOG_SIZE].cpu = c;
	log_index_iio++;
}

void init_iio_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_IIO[i].l_tsc = 0;
      LOG_IIO[i].td_ns = 0;
      LOG_IIO[i].avg_occ = 0;
      LOG_IIO[i].s_avg_occ = 0;
      LOG_IIO[i].cpu = 65;
      i++;
  }
}

void dump_iio_log(void){
  int i=0;
  // printk("index,latest_tsc,time_delta_ns,avg_occ,s_avg_occ,cpu\n");
  while(i<LOG_SIZE){
      printk("IIO:%d,%lld,%lld,%lld,%lld,%d\n",
      i,
      LOG_IIO[i].l_tsc,
      LOG_IIO[i].td_ns,
      LOG_IIO[i].avg_occ,
      LOG_IIO[i].s_avg_occ,
      LOG_IIO[i].cpu);
      i++;
  }
}

void update_log_mba(int c){
  // sched_time = ktime_get_ns();
  // seconds = sched_time / NSEC_PER_SEC;
  // microseconds = (sched_time % NSEC_PER_SEC) / NSEC_PER_USEC;
  // nanoseconds = (sched_time % NSEC_PER_SEC);
  // snprintf(LOG_MBA[log_index_mba % LOG_SIZE].ktime, sizeof(time_str), "%llu.%09lu", seconds, nanoseconds);
	LOG_MBA[log_index_mba % LOG_SIZE].l_tsc = cur_rdtsc_mba;
	LOG_MBA[log_index_mba % LOG_SIZE].td_ns = latest_time_delta_mba_ns;
	LOG_MBA[log_index_mba % LOG_SIZE].cpu = c;
	LOG_MBA[log_index_mba % LOG_SIZE].mba_val = latest_mba_val;
	LOG_MBA[log_index_mba % LOG_SIZE].m_avg_occ = latest_measured_avg_occ;
	LOG_MBA[log_index_mba % LOG_SIZE].m_avg_occ_rd = latest_measured_avg_occ_rd;
	LOG_MBA[log_index_mba % LOG_SIZE].s_avg_pcie_bw = (smoothed_avg_pcie_bw >> 10);
	LOG_MBA[log_index_mba % LOG_SIZE].avg_pcie_bw = latest_avg_pcie_bw;
  LOG_MBA[log_index_mba % LOG_SIZE].s_avg_pcie_bw_rd = (smoothed_avg_pcie_bw_rd >> 10);
	LOG_MBA[log_index_mba % LOG_SIZE].avg_pcie_bw_rd = latest_avg_pcie_bw_rd;
	LOG_MBA[log_index_mba % LOG_SIZE].avg_rd_mem_bw = latest_avg_rd_bw;
	LOG_MBA[log_index_mba % LOG_SIZE].avg_wr_mem_bw = latest_avg_wr_bw;
  if(app_pid_task != NULL){
	  LOG_MBA[log_index_mba % LOG_SIZE].task_state = app_pid_task->state;
  }
	log_index_mba++;
}

void init_mba_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_MBA[i].l_tsc = 0;
      LOG_MBA[i].td_ns = 0;
      LOG_MBA[i].cpu = 65;
      LOG_MBA[i].mba_val = 0;
      LOG_MBA[i].m_avg_occ = 0;
      LOG_MBA[i].m_avg_occ_rd = 0;
      LOG_MBA[i].avg_pcie_bw = 0;
      LOG_MBA[i].s_avg_pcie_bw = 0;
      LOG_MBA[i].avg_rd_mem_bw = 0;
      LOG_MBA[i].avg_wr_mem_bw = 0;
      LOG_MBA[i].task_state = 0xFFFF;
      i++;
  }
}

void dump_mba_log(void){
  int i=0;
  // printk("index,latest_tsc,time_delta_ns,cpu,latest_mba_val,latest_measured_avg_occ,avg_pcie_bw,smoothed_avg_pcie_bw\n");
  while(i<LOG_SIZE){
      printk("MBA:%d,%lld,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%x\n",
      i,
      LOG_MBA[i].l_tsc,
      LOG_MBA[i].td_ns,
      LOG_MBA[i].cpu,
      LOG_MBA[i].mba_val,
      LOG_MBA[i].m_avg_occ,
      LOG_MBA[i].avg_pcie_bw,
      LOG_MBA[i].s_avg_pcie_bw,
      LOG_MBA[i].m_avg_occ_rd,
      LOG_MBA[i].avg_pcie_bw_rd,
      LOG_MBA[i].s_avg_pcie_bw_rd,
      LOG_MBA[i].avg_rd_mem_bw,
      LOG_MBA[i].avg_wr_mem_bw,
      LOG_MBA[i].task_state);
      // LOG_MBA[i].ktime);
      i++;
  }
}