#ifndef HOSTCC_LOCAL_RESPONSE_H
#define HOSTCC_LOCAL_RESPONSE_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include "hostcc.h"

#define SLACK_TIME_US 150
#define WORKER_BUDGET 1000000

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

#endif