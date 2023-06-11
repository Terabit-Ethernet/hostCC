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

//netfilter vars
static struct nf_hook_ops *nf_markecn_ops = NULL;
DEFINE_SPINLOCK(etx_spinlock);

// kthread scheduling vars
static int thread_fun_poll_iio(void *arg);
static int thread_fun_poll_mba(void *arg);

static struct task_struct *thread_iio;
static struct task_struct *thread_mba;

static struct sched_param {
  int sched_priority;
  int sched_policy;
};

// logging vars
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
	int cpu; //current cpu
	uint32_t mba_val; // latest MBA value (from 0 to 4, increasing value denotes lower CPU-Memory Bandwidth)
  uint32_t m_avg_occ; //latest measured avg IIO occupancy
  uint32_t s_avg_pcie_bw; //smoothed average PCIe bandwidth
  uint32_t avg_pcie_bw;  //latest PCIe bandwidth sample
};

struct log_entry_nf{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t m_avg_occ; //latest measured avg IIO occupancy
	int cpu; //current cpu
	uint32_t dat_len; //IP datagram length at last sample
};

#define LOG_SIZE 100
struct log_entry_iio LOG_IIO[LOG_SIZE];
struct log_entry_mba LOG_MBA[LOG_SIZE];
struct log_entry_nf LOG_NF[LOG_SIZE];
uint32_t log_index_iio = 0;
uint32_t log_index_mba = 0;
uint32_t log_index_nf = 0;

// IIO occupancy related vars
#define IRP_MSR_PMON_CTL_BASE 0x0A5BL
#define IRP_MSR_PMON_CTR_BASE 0x0A59L
#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define STACK 2 //We're concerned with stack #2 on our machine
#define IRP_OCC_VAL 0x0040040F
#define CORE_IIO 24
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
#define NUMA0_CORE 28
#define NUMA1_CORE 29
#define NUMA2_CORE 30
#define NUMA3_CORE 31
#define MBA_COS_ID 1
#define IIO_THRESHOLD 70
#define PCIE_BW_THRESHOLD 84
#define BW_TOLERANCE 0
#define USE_PROCESS_SCHEDULER 1
#define MBA_VAL_HIGH 90
#define MBA_VAL_LOW 0
#define REDUCTION_TIMEOUT_US 150

uint64_t prev_rdtsc_mba = 0;
uint64_t cur_rdtsc_mba = 0;
uint64_t tsc_sample_mba = 0;
uint32_t latest_mba_val = 0;
uint64_t latest_time_delta_mba_ns = 0;
uint32_t latest_measured_avg_occ = 0;
uint32_t latest_avg_pcie_bw = 0;
uint32_t smoothed_avg_pcie_bw = 0;
uint64_t cur_cum_frc = 0;
uint64_t prev_cum_frc = 0;
uint64_t cum_frc_sample = 0;
uint32_t app_pid = 0;
uint64_t last_reduced_tsc = 0;
static int target_pid = 0;

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
u64 latest_time_delta_nf_ns = 0;
u32 latest_datagram_len = 0;


//helper function to send SIGCONT/SIGSTOP signals to processes
static int send_signal_to_pid(int proc_pid, int signal)
{
    struct pid *pid_struct;
    struct task_struct *task;

    if (proc_pid == -1) {
        pr_err("No target PID specified\n");
        return -EINVAL;
    }

    pid_struct = find_get_pid(proc_pid);
    if (!pid_struct) {
        pr_err("Invalid PID: %d\n", proc_pid);
        return -EINVAL;
    }

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task) {
        pr_err("Failed to find task with PID: %d\n", proc_pid);
        return -EINVAL;
    }
    send_sig(signal, task, 0);

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

static void update_log_iio(int c){
	LOG_IIO[log_index_iio % LOG_SIZE].l_tsc = cur_rdtsc_iio;
	LOG_IIO[log_index_iio % LOG_SIZE].td_ns = latest_time_delta_iio_ns;
	LOG_IIO[log_index_iio % LOG_SIZE].avg_occ = latest_avg_occ;
	LOG_IIO[log_index_iio % LOG_SIZE].s_avg_occ = smoothed_avg_occ;
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
	LOG_MBA[log_index_mba % LOG_SIZE].l_tsc = cur_rdtsc_mba;
	LOG_MBA[log_index_mba % LOG_SIZE].td_ns = latest_time_delta_mba_ns;
	LOG_MBA[log_index_mba % LOG_SIZE].cpu = c;
	LOG_MBA[log_index_mba % LOG_SIZE].mba_val = latest_mba_val;
	LOG_MBA[log_index_mba % LOG_SIZE].m_avg_occ = latest_measured_avg_occ;
	LOG_MBA[log_index_mba % LOG_SIZE].s_avg_pcie_bw = (smoothed_avg_pcie_bw >> 10);
	LOG_MBA[log_index_mba % LOG_SIZE].avg_pcie_bw = latest_avg_pcie_bw;
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
      LOG_MBA[i].avg_pcie_bw = 0;
      LOG_MBA[i].s_avg_pcie_bw = 0;
      i++;
  }
}

static void dump_mba_log(void){
  int i=0;
  // printk("index,latest_tsc,time_delta_ns,cpu,latest_mba_val,latest_measured_avg_occ,avg_pcie_bw,smoothed_avg_pcie_bw\n");
  while(i<LOG_SIZE){
      printk("MBA:%d,%lld,%lld,%d,%d,%d,%d,%d\n",
      i,
      LOG_MBA[i].l_tsc,
      LOG_MBA[i].td_ns,
      LOG_MBA[i].cpu,
      LOG_MBA[i].mba_val,
      LOG_MBA[i].m_avg_occ,
      LOG_MBA[i].avg_pcie_bw,
      LOG_MBA[i].s_avg_pcie_bw);
      i++;
  }
}