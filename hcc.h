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
#include "SKX_IMC_BDF_Offset.h"



// extern char *sh_mem;
struct pid *app_pid_struct;
struct timespec64 curr_time;
char time_str[32];
u64 sched_time;
unsigned long long seconds;
unsigned long microseconds;
unsigned long nanoseconds;
u64 last_changed_level_tsc = 0;

// memory bandwidth logging
unsigned int *mmconfig_ptr;         // must be pointer to 32-bit int so compiler will generate 32-bit loads and stores
uint64_t imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
uint64_t prev_imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
uint64_t cur_imc_counts[NUM_IMC_CHANNELS][NUM_IMC_COUNTERS];
uint64_t latest_avg_rd_bw = 0;
uint64_t latest_avg_wr_bw = 0;

//netfilter vars
static struct nf_hook_ops *nf_markecn_ops_rx = NULL;
static struct nf_hook_ops *nf_markecn_ops_tx = NULL;
DEFINE_SPINLOCK(etx_spinlock_rx);
DEFINE_SPINLOCK(etx_spinlock_tx);

// kthread scheduling vars
static void thread_fun_poll_iio(struct work_struct *work);
static void thread_fun_poll_iio_rd(struct work_struct *work);
static void thread_fun_poll_mba(struct work_struct* work);

static struct task_struct *thread_iio;
static struct task_struct *thread_iio_rd;
static struct task_struct *thread_mba;

static struct task_struct *app_pid_task;
u64 cur_tsc_sample_pre;
u64 cur_tsc_sample_post;
u64 extra_time_needed;

static struct sched_param {
  int sched_priority;
  int sched_policy;
};

// logging vars
struct log_entry_iio_rd{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t avg_occ_rd; //latest measured avg IIO occupancy
	uint64_t s_avg_occ_rd; //latest calculated smoothed occupancy
	int cpu; //current cpu
};

struct log_entry_iio{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t avg_occ; //latest measured avg IIO occupancy
	uint64_t s_avg_occ; //latest calculated smoothed occupancy
	int cpu; //current cpu
};

struct log_entry_mba{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	char ktime[32]; //latest measured time delta in us
	int cpu; //current cpu
	uint32_t mba_val; // latest MBA value (from 0 to 4, increasing value denotes lower CPU-Memory Bandwidth)
  uint32_t m_avg_occ; //latest measured avg IIO occupancy
  uint32_t s_avg_pcie_bw; //smoothed average PCIe bandwidth
  uint32_t avg_pcie_bw;  //latest PCIe bandwidth sample
  uint32_t m_avg_occ_rd; //latest measured avg IIO Rd occupancy
  uint32_t s_avg_pcie_bw_rd; //smoothed average PCIe Rd bandwidth
  uint32_t avg_pcie_bw_rd;  //latest PCIe Rd bandwidth sample
  uint32_t avg_rd_mem_bw;  //Rd memory bandwidth
  uint32_t avg_wr_mem_bw;  //Wr memory bandwidth
  uint32_t task_state;
};

struct log_entry_nf{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t m_avg_occ; //latest measured avg IIO occupancy
	int cpu; //current cpu
	uint32_t dat_len; //IP datagram length at last sample
};

#define LOG_SIZE 500000
struct log_entry_iio LOG_IIO[LOG_SIZE];
struct log_entry_iio_rd LOG_IIO_RD[LOG_SIZE];
struct log_entry_mba LOG_MBA[LOG_SIZE];
struct log_entry_nf LOG_NF[LOG_SIZE];
uint32_t log_index_iio = 0;
uint32_t log_index_iio_rd = 0;
uint32_t log_index_mba = 0;
uint32_t log_index_nf = 0;

// IIO Rd occupancy related vars
#define IIO_MSR_PMON_CTL_BASE 0x0A48L
#define IIO_MSR_PMON_CTR_BASE 0x0A41L
#define IIO_OCC_VAL 0x00004000004001D5
#define CORE_IIO_RD 24
uint64_t cum_occ_sample_rd;
uint64_t prev_cum_occ_rd;
uint64_t cur_cum_occ_rd;
uint64_t prev_rdtsc_iio_rd = 0;
uint64_t cur_rdtsc_iio_rd = 0;
uint64_t tsc_sample_iio_rd = 0;
uint64_t latest_avg_occ_rd = 0;
uint64_t smoothed_avg_occ_rd = 0;
uint64_t latest_time_delta_iio_rd_ns = 0;

// IIO occupancy related vars
#define IRP_MSR_PMON_CTL_BASE 0x0A5BL
#define IRP_MSR_PMON_CTR_BASE 0x0A59L
#define IRP_OCC_VAL 0x0040040F
#define STACK 2 //We're concerned with stack #2 on our machine
#define CORE_IIO 24
#define IIO_COUNTER_OFFSET 2
uint64_t prev_rdtsc_iio = 0;
uint64_t cur_rdtsc_iio = 0;
uint64_t prev_cum_occ = 0;
uint64_t cur_cum_occ = 0;
uint64_t tsc_sample_iio = 0;
uint64_t cum_occ_sample = 0;
uint64_t latest_avg_occ = 0;
uint64_t smoothed_avg_occ = 0;
uint64_t latest_time_delta_iio_ns = 0;

// PCIe bandwidth and MBA update related vars
#define PQOS_MSR_MBA_MASK_START 0xD50L
// #define PQOS_MSR_MBA_MASK_START 0x1A4 //TODO: check whether this value is for IceLake machines
#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define IIO_PCIE_1_PORT_0_BW_OUT 0x0B24 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define NUMA0_CORE 28
#define NUMA1_CORE 29
#define NUMA2_CORE 30
#define NUMA3_CORE 31
#define MBA_COS_ID 1
#define IIO_THRESHOLD 70
#define IIO_RD_THRESHOLD 190
#define PCIE_BW_THRESHOLD 84
#define BW_TOLERANCE 0
#define USE_PROCESS_SCHEDULER 1
#define MBA_VAL_HIGH 90
#define MBA_VAL_LOW 0
#define REDUCTION_TIMEOUT_US 150
#define WORKER_BUDGET 1000000

uint64_t prev_rdtsc_mba = 0;
uint64_t cur_rdtsc_mba = 0;
uint64_t tsc_sample_mba = 0;
uint32_t latest_mba_val = 0;
uint64_t latest_time_delta_mba_ns = 0;
uint32_t latest_measured_avg_occ = 0;
uint32_t latest_measured_avg_occ_rd = 0;
uint32_t latest_avg_pcie_bw = 0;
uint32_t smoothed_avg_pcie_bw = 0;
uint64_t cur_cum_frc = 0;
uint64_t prev_cum_frc = 0;
uint64_t cum_frc_sample = 0;
uint32_t latest_avg_pcie_bw_rd = 0;
uint32_t smoothed_avg_pcie_bw_rd = 0;
uint64_t cur_cum_frc_rd = 0;
uint64_t prev_cum_frc_rd = 0;
uint64_t cum_frc_rd_sample = 0;
uint32_t app_pid = 0;
uint64_t last_reduced_tsc = 0;
static int target_pid = 0;
static int mode = 0; //mode = 0 => Rx; mode = 1 => Tx

//Netfilter related vars
enum {
	INET_ECN_NOT_ECT = 0,
	INET_ECN_ECT_1 = 1,
	INET_ECN_ECT_0 = 2,
	INET_ECN_CE = 3,
	INET_ECN_MASK = 3,
};

#define NO_ECN_MARKING 0
u64 prev_rdtsc_nf = 0;
u64 cur_rdtsc_nf = 0;
u64 tsc_sample_nf = 0;
u64 latest_measured_avg_occ_nf = 0;
u64 latest_measured_avg_occ_rd_nf = 0;
u64 latest_time_delta_nf_ns = 0;
u32 latest_datagram_len = 0;


//helper function to send SIGCONT/SIGSTOP signals to processes
static int send_signal_to_pid(int proc_pid, int signal)
{
    // struct task_struct *task;

    // if (proc_pid == -1) {
    //     pr_err("No target PID specified\n");
    //     return -EINVAL;
    // }

    // pid_struct = find_get_pid(proc_pid);
    // if (!pid_struct) {
    //     pr_err("Invalid PID: %d\n", proc_pid);
    //     return -EINVAL;
    // }

    // task = pid_task(pid_struct, PIDTYPE_PID);
    // if (!task) {
    //     pr_err("Failed to find task with PID: %d\n", proc_pid);
    //     return -EINVAL;
    // }
    // send_sig(signal, task, 0);

    if(app_pid_struct != NULL){
      rcu_read_lock();
      kill_pid(app_pid_struct, signal, 1);
      rcu_read_unlock();
    }
    // kill_pid(find_vpid(proc_pid), signal, 1);
    // if(signal == SIGCONT){
    //   sprintf(sh_mem, "0003");  
    // }
    // else if(signal == SIGSTOP){
    //   sprintf(sh_mem, "0004");  
    // }

    return 0;
}

static void update_log_nf(int c){
	LOG_NF[log_index_nf % LOG_SIZE].l_tsc = cur_rdtsc_nf;
	LOG_NF[log_index_nf % LOG_SIZE].td_ns = latest_time_delta_nf_ns;
	LOG_NF[log_index_nf % LOG_SIZE].m_avg_occ = latest_measured_avg_occ_nf;
	LOG_NF[log_index_nf % LOG_SIZE].cpu = c;
	LOG_NF[log_index_nf % LOG_SIZE].dat_len = latest_datagram_len;
	log_index_nf++;
}

static void init_nf_log(void){
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

static void dump_nf_log(void) {
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

static void update_log_iio_rd(int c){
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].l_tsc = cur_rdtsc_iio_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].td_ns = latest_time_delta_iio_rd_ns;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].avg_occ_rd = latest_avg_occ_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].s_avg_occ_rd = (smoothed_avg_occ_rd >> 10);
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].cpu = c;
	log_index_iio_rd++;
}

static void init_iio_rd_log(void){
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

static void dump_iio_rd_log(void){
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

static void update_log_iio(int c){
	LOG_IIO[log_index_iio % LOG_SIZE].l_tsc = cur_rdtsc_iio;
	LOG_IIO[log_index_iio % LOG_SIZE].td_ns = latest_time_delta_iio_ns;
	LOG_IIO[log_index_iio % LOG_SIZE].avg_occ = latest_avg_occ;
	LOG_IIO[log_index_iio % LOG_SIZE].s_avg_occ = (smoothed_avg_occ >> 10);
	LOG_IIO[log_index_iio % LOG_SIZE].cpu = c;
	log_index_iio++;
}

static void init_iio_log(void){
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

static void dump_iio_log(void){
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

static void update_log_mba(int c){
  sched_time = ktime_get_ns();
  seconds = sched_time / NSEC_PER_SEC;
  // microseconds = (sched_time % NSEC_PER_SEC) / NSEC_PER_USEC;
  nanoseconds = (sched_time % NSEC_PER_SEC);
  snprintf(LOG_MBA[log_index_mba % LOG_SIZE].ktime, sizeof(time_str), "%llu.%09lu", seconds, nanoseconds);
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

static void init_mba_log(void){
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

static void dump_mba_log(void){
  int i=0;
  // printk("index,latest_tsc,time_delta_ns,cpu,latest_mba_val,latest_measured_avg_occ,avg_pcie_bw,smoothed_avg_pcie_bw\n");
  while(i<LOG_SIZE){
      printk("MBA:%d,%lld,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%x,%s\n",
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
      LOG_MBA[i].task_state,
      LOG_MBA[i].ktime);
      i++;
  }
}