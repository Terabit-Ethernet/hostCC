#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include "hostcc.h"
#include "hostcc-local-response.h"

extern uint32_t latest_mba_val;
extern uint64_t last_reduced_tsc;
extern uint32_t smoothed_avg_pcie_bw;
extern uint32_t smoothed_avg_pcie_bw_rd;
extern uint32_t latest_measured_avg_occ_wr;
extern uint32_t latest_measured_avg_occ_rd;
extern struct task_struct *app_pid_task = NULL;
extern struct pid *app_pid_struct = NULL;
extern uint32_t app_pid;
extern int mode;
extern int target_pid;
extern int target_pcie_thresh;
extern int target_iio_wr_thresh;
extern int target_iio_rd_thresh;
extern u64 last_changed_level_tsc;

u64 extra_time_needed;
u64 cur_tsc_sample_pre;
u64 cur_tsc_sample_post;


void update_mba_msr_register(void){
  uint32_t low = 0;
  uint32_t high = 0;
  uint64_t msr_num = PQOS_MSR_MBA_MASK_START + MBA_COS_ID;
  wrmsr_on_cpu(MBA_LEVEL_1_CORE,msr_num,low,high);
  wrmsr_on_cpu(MBA_LEVEL_2_CORE,msr_num,low,high);
  wrmsr_on_cpu(MBA_LEVEL_3_CORE,msr_num,low,high);
}

//helper function to send SIGCONT/SIGSTOP signals to processes
static int send_signal_to_pid(int proc_pid, int signal)
{
    if(app_pid_struct != NULL){
      rcu_read_lock();
      kill_pid(app_pid_struct, signal, 1);
      rcu_read_unlock();
    }
    return 0;
}

void init_mba_process_scheduler(void){
    app_pid = target_pid;
    printk("MLC PID: %ld\n",app_pid);
    app_pid_task = pid_task(find_get_pid(app_pid), PIDTYPE_PID);
    app_pid_struct = find_vpid(app_pid);
    if (app_pid_task == NULL) {
        printk(KERN_INFO "Cannot find task");
    }
    else{
      printk(KERN_INFO "Found task");
      struct sched_param param;
      param.sched_priority = 99;
      int result = sched_setscheduler(app_pid_task, SCHED_FIFO, &param);
      if (result == -1) {
          printk(KERN_ALERT "Failed to set scheduling policy and priority\n");
      }
    }
}

void update_mba_process_scheduler(void){
    WARN_ON(!(latest_mba_val <= 4));
    if(latest_mba_val == 4){
        send_signal_to_pid(app_pid,SIGSTOP);
    }
    else{
        send_signal_to_pid(app_pid,SIGCONT);
    }
}

void increase_mba_val(void){
    uint64_t msr_num = PQOS_MSR_MBA_MASK_START + MBA_COS_ID;
    u32 low = MBA_VAL_HIGH & 0xFFFFFFFF;
	  u32 high = MBA_VAL_HIGH >> 32;

    // WARN_ON(!(latest_mba_val >= 0));
    #if !(USE_PROCESS_SCHEDULER)
    WARN_ON(!(latest_mba_val <= 3));
    #endif
    #if USE_PROCESS_SCHEDULER
    WARN_ON(!(latest_mba_val <= 4)); //level 4 means infinite latency by MBA -- essentially SIGSTOP
    #endif

    if(latest_mba_val < 3){
        latest_mba_val++;
        switch(latest_mba_val){
            case 0:
                WARN_ON(!(false));
                break;
            case 1:
                wrmsr_on_cpu(MBA_LEVEL_1_CORE,msr_num,low,high);
                break;
            case 2:
                wrmsr_on_cpu(MBA_LEVEL_2_CORE,msr_num,low,high);
                break;
            case 3:
                wrmsr_on_cpu(MBA_LEVEL_3_CORE,msr_num,low,high);
                break;
            default:
                WARN_ON(!(false));
                break;
        }
    }
    #if USE_PROCESS_SCHEDULER
    else if(latest_mba_val == 3){
        latest_mba_val++;
        update_mba_process_scheduler(); //should initiate SIGSTOP
    }
    #endif
}

void decrease_mba_val(void){
    uint64_t cur_tsc_val = rdtscp();
    if((cur_tsc_val - last_reduced_tsc) / 3300 < SLACK_TIME_US){
        return;
    }
    uint64_t msr_num = PQOS_MSR_MBA_MASK_START + MBA_COS_ID;
    uint32_t low = MBA_VAL_LOW & 0xFFFFFFFF;
	  uint32_t high = MBA_VAL_LOW >> 32;

    // WARN_ON(!(latest_mba_val >= 0));
    #if !(USE_PROCESS_SCHEDULER)
    WARN_ON(!(latest_mba_val <= 3));
    #endif
    #if USE_PROCESS_SCHEDULER
    WARN_ON(!(latest_mba_val <= 4));
    #endif

    if(latest_mba_val > 0){
        latest_mba_val--;
        switch(latest_mba_val){
            case 0:
                wrmsr_on_cpu(MBA_LEVEL_1_CORE,msr_num,low,high);
                last_reduced_tsc = rdtscp();
                break;
            case 1:
                wrmsr_on_cpu(MBA_LEVEL_2_CORE,msr_num,low,high);
                last_reduced_tsc = rdtscp();
                break;
            case 2:
                wrmsr_on_cpu(MBA_LEVEL_3_CORE,msr_num,low,high);
                last_reduced_tsc = rdtscp();
                break;
            case 3:
                #if !(USE_PROCESS_SCHEDULER)
                WARN_ON(!(false));
                #endif
                #if USE_PROCESS_SCHEDULER
                update_mba_process_scheduler();
                last_reduced_tsc = rdtscp();
                #endif
                break;
            default:
                WARN_ON(!(false));
                break;
        }
    }
}

void host_local_response(void){
  if(mode == 0){
    //Rx side logic
    if((smoothed_avg_pcie_bw) < (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ_wr > target_iio_wr_thresh){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw) > (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ_wr < target_iio_wr_thresh){
            decrease_mba_val();
        }
    }
  } else{
    //Tx side logic
    if((smoothed_avg_pcie_bw_rd) < (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ_rd > target_iio_rd_thresh){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw_rd ) > (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ_rd < target_iio_rd_thresh){
            decrease_mba_val();
        }
    }
  }
}