#include "hostcc.h"
#include "hostcc-signals.h"
#include "hostcc-local-response.h"
// #include "hostcc-network-response.h"
// #include "hostcc-logging.h"

module_param(target_pid, int, 0);
module_param(target_pcie_thresh, int, 0);
module_param(target_iio_thresh, int, 0);
module_param(target_iio_rd_thresh, int, 0);
module_param(mode, int, 0);
MODULE_PARM_DESC(target_pid, "Target process ID");
MODULE_PARM_DESC(mode, "Mode of operation (Rx: 0, or Tx: 1) -- needed for proper host resource allocation");
MODULE_LICENSE("GPL");

extern bool terminate_hcc;
extern bool terminate_hcc_logging;
extern struct task_struct *app_pid_task;
extern struct pid *app_pid_struct;
struct workqueue_struct *poll_iio_queue, *poll_iio_rd_queue, *poll_mba_queue;
struct work_struct poll_iio, poll_iio_rd, poll_mba;

void poll_iio_rd_init(void){
  //initialize the log
  printk(KERN_INFO "Starting IIO Rd Sampling");
  init_iio_rd_log();
  update_iio_rd_occ_ctl_reg();
}

void poll_iio_rd_exit(void){
  //dump log info
  printk(KERN_INFO "Ending IIO Rd Sampling");
  flush_workqueue(poll_iio_rd_queue);
  flush_scheduled_work();
  destroy_workqueue(poll_iio_rd_queue);
  // dump_iio_rd_log();
}

void thread_fun_poll_iio_rd(struct work_struct *work){
  int cpu = CORE_IIO_RD;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_iio_rd(cpu); //sample counters
    update_iio_rd_occ();         //update occupancy value
    if(!terminate_hcc_logging){
      update_log_iio_rd(cpu);      //update the log
    }
    budget--;
  }
  if(!terminate_hcc){
    queue_work_on(cpu,poll_iio_rd_queue, &poll_iio_rd);
  }
  else{
    return;
  }
}

void poll_iio_init(void) {
    //initialize the log
    printk(KERN_INFO "Starting IIO Sampling");
    init_iio_log();
    update_iio_occ_ctl_reg();
}

void poll_iio_exit(void) {
    //dump log info
    printk(KERN_INFO "Ending IIO Sampling");
    flush_workqueue(poll_iio_queue);
    flush_scheduled_work();
    destroy_workqueue(poll_iio_queue);
    //dump_iio_log();
}

void thread_fun_poll_iio(struct work_struct *work) {
  int cpu = CORE_IIO;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_iio(cpu); //sample counters
    update_iio_occ();         //update occupancy value
    if(!terminate_hcc_logging){
      update_log_iio(cpu);      //update the log
    }
    budget--;
  }
  if(!terminate_hcc){
    queue_work_on(cpu,poll_iio_queue, &poll_iio);
  }
  else{
    return;
  }
}

void poll_mba_init(void) {
    #if USE_PROCESS_SCHEDULER
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
      // struct task_struct *current_task = current; // Get the current task
      // Set the scheduling policy to SCHED_FIFO
      param.sched_priority = 99;  // Set the priority (99 is highest)
      // Change the scheduling policy and priority
      int result = sched_setscheduler(app_pid_task, SCHED_FIFO, &param);
      if (result == -1) {
          // Handle the error
          printk(KERN_ALERT "Failed to set scheduling policy and priority\n");
          // return -1;
      }
    }

    init_mmconfig();
    update_imc_config();
    #endif
    //initialize the log
    printk(KERN_INFO "Starting PCIe Bandwidth Sampling");
    init_mba_log();
}

void poll_mba_exit(void) {
    //dump log info
    printk(KERN_INFO "Ending PCIe Bandwidth Sampling");
    flush_workqueue(poll_mba_queue);
    flush_scheduled_work();
    destroy_workqueue(poll_mba_queue);
    if(latest_mba_val > 0){
        latest_mba_val = 0;
        update_mba_msr_register();
        #if USE_PROCESS_SCHEDULER
        update_mba_process_scheduler();
        #endif
    }
    dump_mba_log();
    if(mmconfig_ptr){
      iounmap(mmconfig_ptr);
    }
}


void thread_fun_poll_mba(struct work_struct *work) {  
  int cpu = NUMA0_CORE;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_pcie_bw(cpu);
    update_pcie_bw();
    // update_imc_bw();
    if(mode == 0){
    latest_measured_avg_occ = smoothed_avg_occ >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    } else{
    latest_measured_avg_occ_rd = smoothed_avg_occ_rd >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    }
    update_mba_val();
    if(!terminate_hcc_logging){
      update_log_mba(cpu);
    }
    // unsigned int min_value = 1;
    // unsigned int max_value = 10;
    // unsigned int random_number;

    // // Generate a random integer between min_value and max_value
    // random_number = get_random_int() % (max_value - min_value + 1) + min_value;
        // mba_test(random_number * 50,random_number * 50,cpu);
        // mba_test(500,100,cpu);
    budget--;
  }
  if(!terminate_hcc){
    queue_work_on(cpu,poll_mba_queue, &poll_mba);
  }
  else{
    return;
  }
  
}

static int __init hcc_init(void) {
  if(mode == 0){
    //Start IIO occupancy sampling
    poll_iio_queue = alloc_workqueue("poll_iio_queue",  WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!poll_iio_queue) {
        printk(KERN_ERR "Failed to create IIO workqueue\n");
        return -ENOMEM;
    }
    INIT_WORK(&poll_iio, thread_fun_poll_iio);
    poll_iio_init();
    queue_work_on(CORE_IIO,poll_iio_queue, &poll_iio);
  } else {
    //Start IIO Rd occupancy sampling
    poll_iio_rd_queue = alloc_workqueue("poll_iio_rd_queue",  WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!poll_iio_rd_queue) {
        printk(KERN_ERR "Failed to create IIO Rd workqueue\n");
        return -ENOMEM;
    }
    INIT_WORK(&poll_iio_rd, thread_fun_poll_iio_rd);
    poll_iio_rd_init();
    queue_work_on(CORE_IIO_RD,poll_iio_rd_queue, &poll_iio_rd);
  }

  //Start MBA allocation
  poll_mba_queue = alloc_workqueue("poll_mba_queue", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
  if (!poll_mba_queue) {
      printk(KERN_ERR "Failed to create MBA workqueue\n");
      return -ENOMEM;
  }
  INIT_WORK(&poll_mba, thread_fun_poll_mba);
  poll_mba_init();
  queue_work_on(NUMA0_CORE,poll_mba_queue, &poll_mba);

  //Start ECN marking
  nf_init();

  return 0;
}

static void __exit hcc_exit(void) {
  terminate_hcc_logging = true;
  msleep(5000);
  terminate_hcc = true;
  nf_exit();
  if(mode == 0){
  poll_iio_exit();
  } else {
  poll_iio_rd_exit();
  }
  poll_mba_exit();
}

module_init(hcc_init);
module_exit(hcc_exit);
