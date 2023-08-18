#ifndef HOSTCC_LOCAL_RESPONSE_H
#define HOSTCC_LOCAL_RESPONSE_H



#define MBA_COS_ID 1
#define PQOS_MSR_MBA_MASK_START 0xD50L
// #define PQOS_MSR_MBA_MASK_START 0x1A4 //TODO: check whether this value is for IceLake machines
// #define IIO_THRESHOLD 70
#define IIO_RD_THRESHOLD 190
// #define PCIE_BW_THRESHOLD 84
#define BW_TOLERANCE 0
#define USE_PROCESS_SCHEDULER 1
#define MBA_VAL_HIGH 90
#define MBA_VAL_LOW 0
#define REDUCTION_TIMEOUT_US 150
#define WORKER_BUDGET 1000000

extern struct task_struct *app_pid_task;

static struct sched_param {
  int sched_priority;
  int sched_policy;
};

// extern char *sh_mem;
extern struct pid *app_pid_struct;

void update_mba_msr_register(void);
static int send_signal_to_pid(int proc_pid, int signal);
void update_mba_process_scheduler(void);
void init_mba_process_scheduler(void);
void increase_mba_val(void);
void decrease_mba_val(void);
void host_local_response(void);
void mba_test(int threshold_us_up, int threshold_us_down, int cpu);

#endif