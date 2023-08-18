#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include "hostcc.h"
// #include "hostcc-signals.h"
#include "hostcc-local-response.h"

extern uint32_t latest_mba_val;
extern uint64_t last_reduced_tsc;
extern uint32_t smoothed_avg_pcie_bw;
extern uint32_t smoothed_avg_pcie_bw_rd;
extern uint32_t latest_measured_avg_occ;
extern uint32_t latest_measured_avg_occ_rd;
extern struct task_struct *app_pid_task = NULL;
extern struct pid *app_pid_struct = NULL;
extern uint32_t app_pid;
extern int mode;
extern int target_pid;
extern int target_pcie_thresh;
extern int target_iio_thresh;
extern int target_iio_rd_thresh;
extern u64 last_changed_level_tsc;

u64 extra_time_needed;
u64 cur_tsc_sample_pre;
u64 cur_tsc_sample_post;


void update_mba_msr_register(void){
  uint32_t low = 0;
  uint32_t high = 0;
  uint64_t msr_num = PQOS_MSR_MBA_MASK_START + MBA_COS_ID;
  wrmsr_on_cpu(NUMA1_CORE,msr_num,low,high);
  wrmsr_on_cpu(NUMA2_CORE,msr_num,low,high);
  wrmsr_on_cpu(NUMA3_CORE,msr_num,low,high);
}

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

void update_mba_process_scheduler(void){
    WARN_ON(!(latest_mba_val <= 4));
    if(latest_mba_val == 4){
        send_signal_to_pid(app_pid,SIGSTOP);
        //wait until task is stopped
        // cur_tsc_sample_pre = rdtscp();
        // if(app_pid_task != NULL){
        //   while(app_pid_task->state != TASK_STOPPED){
        //     ;
        //   }
        // }
        // cur_tsc_sample_post = rdtscp();
        // extra_time_needed = (cur_tsc_sample_post - cur_tsc_sample_pre) * 10 /33;
        // printk("Extra time for stop: %ld",extra_time_needed);
        // if(extra_time_needed < 25000){
        //   while((rdtscp() - cur_tsc_sample_post) * 10 / 33 < extra_time_needed){
        //     ;
        //   }
        // }
        // else{
        //   while((rdtscp() - cur_tsc_sample_post) * 10 / 33 < 25000){
        //     ;
        //   }
        // }
        // u64 cur_tsc_sample = rdtscp();
        // while((rdtscp() - cur_tsc_sample) /33 < 5000){
        //   ;
        // }
    }
    else{
        send_signal_to_pid(app_pid,SIGCONT);
        // cur_tsc_sample_pre = rdtscp();
        // if(app_pid_task != NULL){
        //   while(app_pid_task->state == TASK_STOPPED){
        //     ;
        //   }
        // }
        // cur_tsc_sample_post = rdtscp();
        // extra_time_needed = (cur_tsc_sample_post - cur_tsc_sample_pre) * 10 /33;
        // printk("Extra time for cont: %ld",extra_time_needed);
        // while(rdtscp() - cur_tsc_sample_post < extra_time_needed){
        //   ;
        // }
        // u64 cur_tsc_sample = rdtscp();
        // while((rdtscp() - cur_tsc_sample) /33 < 5000){
        //   ;
        // }
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
                wrmsr_on_cpu(NUMA1_CORE,msr_num,low,high);
                break;
            case 2:
                wrmsr_on_cpu(NUMA2_CORE,msr_num,low,high);
                break;
            case 3:
                wrmsr_on_cpu(NUMA3_CORE,msr_num,low,high);
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
    if((cur_tsc_val - last_reduced_tsc) / 3300 < REDUCTION_TIMEOUT_US){
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
                wrmsr_on_cpu(NUMA1_CORE,msr_num,low,high);
                last_reduced_tsc = rdtscp();
                break;
            case 1:
                wrmsr_on_cpu(NUMA2_CORE,msr_num,low,high);
                last_reduced_tsc = rdtscp();
                break;
            case 2:
                wrmsr_on_cpu(NUMA3_CORE,msr_num,low,high);
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

void update_mba_val(void){
  if(mode == 0){
    //Rx side logic
    if((smoothed_avg_pcie_bw) < (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ > target_iio_thresh){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw) > (target_pcie_thresh << 10)){
        if(latest_measured_avg_occ < target_iio_thresh){
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

void mba_test(int threshold_us_up, int threshold_us_down, int cpu){
    // every threshold_us worth microseconds, switch MBA level between 3 and 4
    uint64_t cur_tsc_val = rdtscp();
    if(latest_mba_val == 4){
      if((cur_tsc_val - last_changed_level_tsc) / 3300 < threshold_us_down){
          return;
      }
    }
    else{
      if((cur_tsc_val - last_changed_level_tsc) / 3300 < threshold_us_up){
          return;
      }
    }
    if(latest_mba_val == 4){
      latest_mba_val = 3;
    }
    else{
      latest_mba_val = 4;
    }
    update_mba_process_scheduler();
    last_changed_level_tsc = rdtscp();
    // if(!terminate_hcc_logging){
    //   update_log_mba(cpu);
    // }
}