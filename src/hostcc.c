#include "hostcc.h"
#include "hostcc-signals.h"
#include "hostcc-local-response.h"
#include "hostcc-network-response.h"
#include "hostcc-logging.h"

module_param(target_pid, int, 0);
module_param(target_pcie_thresh, int, 0);
module_param(target_iio_wr_thresh, int, 0);
module_param(target_iio_rd_thresh, int, 0);
module_param(mode, int, 0);
MODULE_PARM_DESC(target_pid, "Target process ID");
MODULE_PARM_DESC(mode, "Mode of operation (Rx: 0, or Tx: 1) -- needed for proper host resource allocation");
MODULE_LICENSE("GPL");

extern bool terminate_hcc;
extern bool terminate_hcc_logging;
struct workqueue_struct *poll_iio_queue, *poll_pcie_queue;
struct work_struct poll_iio, poll_pcie;
extern int mode;

void poll_iio_init(void) {
    //initialize the log
    printk(KERN_INFO "Starting IIO Sampling");
    if(mode == 0){
      init_iio_wr_log();
      update_iio_wr_occ_ctl_reg();
    }
    else{
      init_iio_rd_log();
      update_iio_rd_occ_ctl_reg();
    }
}

void poll_iio_exit(void) {
    //dump log info
    printk(KERN_INFO "Ending IIO Sampling");
    flush_workqueue(poll_iio_queue);
    flush_scheduled_work();
    destroy_workqueue(poll_iio_queue);
    if(mode == 0){
      //dump_iio_wr_log();
    }
    else{
      //dump_iio_rd_log();
    }
}

void thread_fun_poll_iio(struct work_struct *work) {
  int cpu = CORE_IIO;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    if(mode == 0){
      sample_counters_iio_wr(cpu); //sample counters
      update_iio_wr_occ();         //update occupancy value
      if(!terminate_hcc_logging){
        update_log_iio_wr(cpu);      //update the log
      }
    }
    else{
      sample_counters_iio_rd(cpu); //sample counters
      update_iio_rd_occ();         //update occupancy value
      if(!terminate_hcc_logging){
        update_log_iio_rd(cpu);      //update the log
      }
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

void poll_pcie_init(void) {
    #if USE_PROCESS_SCHEDULER
    init_mba_process_scheduler();
    init_mmconfig();
    update_imc_config();
    #endif
    //initialize the log
    printk(KERN_INFO "Starting PCIe Bandwidth Sampling");
    init_mba_log();
}

void poll_pcie_exit(void) {
    //dump log info
    printk(KERN_INFO "Ending PCIe Bandwidth Sampling");
    flush_workqueue(poll_pcie_queue);
    flush_scheduled_work();
    destroy_workqueue(poll_pcie_queue);
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


void thread_fun_poll_pcie(struct work_struct *work) {  
  int cpu = NUMA0_CORE;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_pcie_bw(cpu);
    update_pcie_bw();
    // update_imc_bw();
    if(mode == 0){
    latest_measured_avg_occ_wr = smoothed_avg_occ_wr >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    } else{
    latest_measured_avg_occ_rd = smoothed_avg_occ_rd >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    }
    host_local_response();
    if(!terminate_hcc_logging){
      update_log_mba(cpu);
    }
    budget--;
  }
  if(!terminate_hcc){
    queue_work_on(cpu,poll_pcie_queue, &poll_pcie);
  }
  else{
    return;
  }
  
}

static int __init hostcc_init(void) {
  printk("Initializing hostcc");
  //Start IIO occupancy measurement
  poll_iio_queue = alloc_workqueue("poll_iio_queue",  WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
  if (!poll_iio_queue) {
      printk(KERN_ERR "Failed to create IIO workqueue\n");
      return -ENOMEM;
  }
  INIT_WORK(&poll_iio, thread_fun_poll_iio);
  poll_iio_init();
  queue_work_on(CORE_IIO,poll_iio_queue, &poll_iio);

  //Start PCIe bandwidth measurement
  poll_pcie_queue = alloc_workqueue("poll_pcie_queue", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
  if (!poll_pcie_queue) {
      printk(KERN_ERR "Failed to create PCIe workqueue\n");
      return -ENOMEM;
  }
  INIT_WORK(&poll_pcie, thread_fun_poll_pcie);
  poll_pcie_init();
  queue_work_on(NUMA0_CORE,poll_pcie_queue, &poll_pcie);

  //Start ECN marking
  nf_init();

  return 0;
}

static void __exit hostcc_exit(void) {
  terminate_hcc_logging = true;
  msleep(5000);
  terminate_hcc = true;
  nf_exit();
  poll_iio_exit();
  poll_pcie_exit();
}

module_init(hostcc_init);
module_exit(hostcc_exit);
