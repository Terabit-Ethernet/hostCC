#include "hostcc.h"
#include "hostcc-signals.h"
#include "hostcc-local-response.h"
#include "hostcc-network-response.h"
#include "hostcc-logging.h"

module_param(target_pid, int, 0);
module_param(target_pcie_thresh, int, 0644);
module_param(target_iio_wr_thresh, int, 0);
module_param(target_iio_rd_thresh, int, 0);
module_param(enable_network_response, int, 0);
module_param(enable_local_response, int, 0);
module_param(mode, int, 0);
MODULE_PARM_DESC(target_pid, "Target process ID");
MODULE_PARM_DESC(mode, "Mode of operation (Rx: 0, or Tx: 1)");
MODULE_LICENSE("GPL");

extern bool terminate_hcc;
extern bool terminate_hcc_logging;
struct workqueue_struct *poll_iio_queue, *poll_pcie_queue;
struct work_struct poll_iio, poll_pcie;
extern int mode;

int enable_local_response = 1;
int enable_network_response = 1;

static ssize_t target_pcie_thresh_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    sscanf(buf, "%d", &target_pcie_thresh);
    return count;
}

static ssize_t target_pcie_thresh_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", target_pcie_thresh);
}

static struct kobj_attribute target_pcie_thresh_attribute = __ATTR(target_pcie_thresh, 0644, target_pcie_thresh_show, target_pcie_thresh_store);

static struct attribute *attrs[] = {
    &target_pcie_thresh_attribute.attr,
    NULL,
};

static struct attribute_group attr_group = {
    .attrs = attrs,
};

static struct kobject *my_kobj;

void poll_iio_init(void) {
    //initialize the log
    printk(KERN_INFO "Starting IIO Occupancy measurement");
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
    printk(KERN_INFO "Ending IIO Occupancy measurement");
    flush_workqueue(poll_iio_queue);
    flush_scheduled_work();
    destroy_workqueue(poll_iio_queue);
    if(mode == 0){
      if(IIO_LOGGING){
        dump_iio_wr_log();
      }
    }
    else{
      if(IIO_LOGGING){
        dump_iio_rd_log();
      }
    }
}

void thread_fun_poll_iio(struct work_struct *work) {
  int cpu = IIO_CORE;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    if(mode == 0){
      sample_counters_iio_wr(cpu); //sample counters
      update_iio_wr_occ();         //update occupancy value
      if(!terminate_hcc_logging && IIO_LOGGING){
        update_log_iio_wr(cpu);      //update the log
      }
    }
    else{
      sample_counters_iio_rd(cpu); //sample counters
      update_iio_rd_occ();         //update occupancy value
      if(!terminate_hcc_logging && IIO_LOGGING){
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
    #endif
    //initialize the log
    printk(KERN_INFO "Starting PCIe Bandwidth Measurement");
    init_pcie_log();
}

void poll_pcie_exit(void) {
    //dump log info
    printk(KERN_INFO "Ending PCIe Bandwidth Measurement");
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
    if(PCIE_LOGGING){
      dump_pcie_log();
    }
}


void thread_fun_poll_pcie(struct work_struct *work) {  
  int cpu = PCIE_CORE;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_pcie_bw(cpu);
    update_pcie_bw();
    if(mode == 0){
    latest_measured_avg_occ_wr = smoothed_avg_occ_wr >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    } else{
    latest_measured_avg_occ_rd = smoothed_avg_occ_rd >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    }
    if(enable_local_response){
      host_local_response();
    }
    if(!terminate_hcc_logging && PCIE_LOGGING){
      update_log_pcie(cpu);
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
  // create sysfs interface for taget_pcie_thresh
  my_kobj = kobject_create_and_add("hostcc", kernel_kobj);
  if (!my_kobj)
      return -ENOMEM;

  if (sysfs_create_group(my_kobj, &attr_group) < 0) {
      kobject_put(my_kobj);
      return -ENOMEM;
  }
  //Start IIO occupancy measurement
  poll_iio_queue = alloc_workqueue("poll_iio_queue",  WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
  if (!poll_iio_queue) {
      printk(KERN_ERR "Failed to create IIO workqueue\n");
      return -ENOMEM;
  }
  INIT_WORK(&poll_iio, thread_fun_poll_iio);
  poll_iio_init();
  queue_work_on(IIO_CORE,poll_iio_queue, &poll_iio);

  //Start PCIe bandwidth measurement
  poll_pcie_queue = alloc_workqueue("poll_pcie_queue", WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
  if (!poll_pcie_queue) {
      printk(KERN_ERR "Failed to create PCIe workqueue\n");
      return -ENOMEM;
  }
  INIT_WORK(&poll_pcie, thread_fun_poll_pcie);
  poll_pcie_init();
  queue_work_on(PCIE_CORE,poll_pcie_queue, &poll_pcie);

  //Start ECN marking
  nf_init();

  return 0;
}

static void __exit hostcc_exit(void) {
  sysfs_remove_group(my_kobj, &attr_group);
  kobject_put(my_kobj);
  terminate_hcc_logging = true;
  msleep(5000);
  terminate_hcc = true;
  nf_exit();
  poll_iio_exit();
  poll_pcie_exit();
}

module_init(hostcc_init);
module_exit(hostcc_exit);
