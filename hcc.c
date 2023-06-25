#include "hcc.h"

module_param(target_pid, int, 0);
module_param(mode, int, 0);
MODULE_PARM_DESC(target_pid, "Target process ID");
MODULE_PARM_DESC(mode, "Mode of operation (Rx: 0, or Tx: 1) -- needed for proper host resource allocation");
MODULE_LICENSE("GPL");

static inline __attribute__((always_inline)) unsigned long rdtscp(void)
{
   unsigned long a, d, c;

   __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

   return (a | (d << 32));
}

static bool terminate_hcc = false;
static bool terminate_hcc_logging = false;
static struct workqueue_struct *poll_iio_queue, *poll_iio_rd_queue, *poll_mba_queue;
static struct work_struct poll_iio, poll_iio_rd, poll_mba;

//Netfilter logic to mark ECN bits
static void sample_counters_nf(int c){
  if(mode == 0){
    latest_measured_avg_occ_nf = smoothed_avg_occ >> 10;
  }
  else {
    latest_measured_avg_occ_rd_nf = smoothed_avg_occ_rd >> 10;
  }

	tsc_sample_nf = rdtscp();
	prev_rdtsc_nf = cur_rdtsc_nf;
	cur_rdtsc_nf = tsc_sample_nf;

  latest_time_delta_nf_ns = ((cur_rdtsc_nf - prev_rdtsc_nf) * 10) / 33;
	return;
}

static unsigned int nf_markecn_handler_rx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
   struct net_device *indev = state->in;
	if (!skb) {
		return NF_ACCEPT;
	} 
  else if (indev != NULL) { //if packet is Rx
		struct sk_buff *sb = NULL;
		struct iphdr *iph;
		sb = skb;
		iph = ip_hdr(sb);

		spin_lock(&etx_spinlock_rx);
		latest_datagram_len = iph->tot_len;
		int cpu = get_cpu();
    sample_counters_nf(cpu);
    if(!terminate_hcc_logging){
      update_log_nf(cpu);
    }
    #if !(NO_ECN_MARKING)
    if(latest_measured_avg_occ_nf > IIO_THRESHOLD){
        iph->tos = iph->tos | 0x03;
        iph->check = 0;
        ip_send_check(iph);
    }
    #endif
		spin_unlock(&etx_spinlock_rx);

		return NF_ACCEPT;
	}
  return NF_ACCEPT;
}

static unsigned int nf_markecn_handler_tx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
  struct net_device *outdev = state->out;
	if (!skb) {
		return NF_ACCEPT;
	} else if(outdev != NULL) { //if packet is Tx
		struct sk_buff *sb = NULL;
		struct iphdr *iph;
		sb = skb;
		iph = ip_hdr(sb);

		spin_lock(&etx_spinlock_tx);
		latest_datagram_len = iph->tot_len;
		int cpu = get_cpu();
    sample_counters_nf(cpu);
    if(!terminate_hcc_logging){
      update_log_nf(cpu);
    }
    #if !(NO_ECN_MARKING)
    if(latest_measured_avg_occ_rd_nf > IIO_RD_THRESHOLD){
        iph->tos = iph->tos | 0x03;
        iph->check = 0;
        ip_send_check(iph);
    }
    #endif
		spin_unlock(&etx_spinlock_tx);

		return NF_ACCEPT;
	}
  return NF_ACCEPT;
}

static int nf_init(void) {
  if(mode == 0){
    // Pre-routing hook for Rx datapath
    nf_markecn_ops_rx = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
    if (nf_markecn_ops_rx != NULL) {
      nf_markecn_ops_rx->hook = (nf_hookfn*)nf_markecn_handler_rx;
      nf_markecn_ops_rx->hooknum = NF_INET_PRE_ROUTING;
      nf_markecn_ops_rx->pf = NFPROTO_IPV4;
      nf_markecn_ops_rx->priority = NF_IP_PRI_FIRST + 1;
      nf_register_net_hook(&init_net, nf_markecn_ops_rx);
    }
  }
  else{
    // Post-routing hook for Tx datapath
    nf_markecn_ops_tx = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
    if (nf_markecn_ops_tx != NULL) {
      nf_markecn_ops_tx->hook = (nf_hookfn*)nf_markecn_handler_tx;
      nf_markecn_ops_tx->hooknum = NF_INET_POST_ROUTING;
      nf_markecn_ops_tx->pf = NFPROTO_IPV4;
      nf_markecn_ops_tx->priority = NF_IP_PRI_FIRST + 1;
      nf_register_net_hook(&init_net, nf_markecn_ops_tx);
    }
  }
  init_nf_log();
	return 0;
}

static void nf_exit(void) {
  if(mode == 0){
    if (nf_markecn_ops_rx  != NULL) {
      nf_unregister_net_hook(&init_net, nf_markecn_ops_rx);
      kfree(nf_markecn_ops_rx);
    }
  }
  else{
    if (nf_markecn_ops_tx  != NULL) {
      nf_unregister_net_hook(&init_net, nf_markecn_ops_tx);
      kfree(nf_markecn_ops_tx);
    }
  }
  printk(KERN_INFO "Ending ECN Marking");
  dump_nf_log();
}

// IIO Read occupancy logic (obtained via CHA counters)
static void update_iio_rd_occ_ctl_reg(void){
  int cha;
  uint64_t msr_num;
  uint32_t low, high;
  for(cha = 0; cha < NUM_CHA_BOXES; cha++) {
    msr_num = CHA_MSR_PMON_CTL_BASE + (0x10 * cha) + 0; // counter 0
    low = CHA_EVENT & 0xFFFFFFFF;
    high = CHA_EVENT >> 32;
    wrmsr_on_cpu(CORE_IIO_RD,msr_num,low,high);

    // Filters
    msr_num = CHA_MSR_PMON_FILTER0_BASE + (0x10 * cha); // Filter0
    low = CHA_FILTER0 & 0xFFFFFFFF;
    high = CHA_FILTER0 >> 32;
    wrmsr_on_cpu(CORE_IIO_RD,msr_num,low,high);

    msr_num = CHA_MSR_PMON_FILTER1_BASE + (0x10 * cha); // Filter1
    low = CHA_FILTER1 & 0xFFFFFFFF;
    high = CHA_FILTER1 >> 32;
    wrmsr_on_cpu(CORE_IIO_RD,msr_num,low,high);
  }
}

static void sample_iio_rd_occ_counter(int c){
  int cha;
  uint64_t msr_num;
  uint32_t low = 0, high = 0;
  cum_occ_sample_rd = 0;
  for (cha=0; cha<NUM_CHA_BOXES; cha++) {
    msr_num = CHA_MSR_PMON_CTR_BASE + (0x10 * cha) + 0;
    rdmsr_on_cpu(c,msr_num,&low,&high);
    cum_occ_sample_rd += ((uint64_t)high << 32) | low;
  }
  prev_cum_occ_rd = cur_cum_occ_rd;
  cur_cum_occ_rd = cum_occ_sample_rd;
}

void sample_iio_rd_time_counter(void){
  tsc_sample_iio_rd = rdtscp();
	prev_rdtsc_iio_rd = cur_rdtsc_iio_rd;
	cur_rdtsc_iio_rd = tsc_sample_iio_rd;
}

static void sample_counters_iio_rd(int c){
  //first sample occupancy
	sample_iio_rd_occ_counter(c);
	//sample time at the last
	sample_iio_rd_time_counter();
	return;
}

static void update_iio_rd_occ(void){
  latest_time_delta_iio_rd_ns = ((cur_rdtsc_iio_rd - prev_rdtsc_iio_rd) * 10) / 33;
	if(latest_time_delta_iio_rd_ns > 0){
		latest_avg_occ_rd = ((cur_cum_occ_rd - prev_cum_occ_rd) * 5) / (latest_time_delta_iio_rd_ns * 12);
    // ((occ[i] - occ[i-1]) / (((time_us[i+1] - time_us[i])) * 1e-6 * freq)); 
        if(latest_avg_occ_rd > 0){
            smoothed_avg_occ_rd = ((7*smoothed_avg_occ_rd) + (latest_avg_occ_rd << 10)) >> 3;
        }
	}
}

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
  dump_iio_rd_log();
}

void thread_fun_poll_iio_rd(struct work_struct *work){
  int cpu = CORE_IIO_RD;
  uint32_t budget = WORKER_BUDGET_IIO_RD;
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

// IIO Write occupancy logic
static void update_iio_occ_ctl_reg(void){
	//program the desired CTL register to read the corresponding CTR value
  uint64_t msr_num;
	msr_num = IRP_MSR_PMON_CTL_BASE + (0x20 * STACK) + 0;
  uint32_t low = IRP_OCC_VAL & 0xFFFFFFFF;
	uint32_t high = IRP_OCC_VAL >> 32;
  wrmsr_on_cpu(CORE_IIO,msr_num,low,high);
}

static void sample_iio_occ_counter(int c){
  uint64_t msr_num;
  uint32_t low = 0;
	uint32_t high = 0;
	msr_num = IRP_MSR_PMON_CTR_BASE + (0x20 * STACK) + 0;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_occ_sample = ((uint64_t)high << 32) | low;
	prev_cum_occ = cur_cum_occ;
	cur_cum_occ = cum_occ_sample;
}

static void sample_iio_time_counter(void){
  tsc_sample_iio = rdtscp();
	prev_rdtsc_iio = cur_rdtsc_iio;
	cur_rdtsc_iio = tsc_sample_iio;
}

static void sample_counters_iio(int c){
	//first sample occupancy
	sample_iio_occ_counter(c);
	//sample time at the last
	sample_iio_time_counter();
	return;
}

static void update_iio_occ(void){
	latest_time_delta_iio_ns = ((cur_rdtsc_iio - prev_rdtsc_iio) * 10) / 33;
	if(latest_time_delta_iio_ns > 0){
		latest_avg_occ = (cur_cum_occ - prev_cum_occ) / (latest_time_delta_iio_ns >> 1);
    // ((occ[i] - occ[i-1]) / (((time_us[i+1] - time_us[i])) * 1e-6 * freq)); 
        if(latest_avg_occ > 10){
            smoothed_avg_occ = ((7*smoothed_avg_occ) + (latest_avg_occ << 10)) >> 3;
        }
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
    dump_iio_log();
}

static void thread_fun_poll_iio(struct work_struct *work) {
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

// PCIe bandwidth sampling and MBA update logic

static void sample_pcie_bw_counter(int c){
  uint32_t low = 0;
	uint32_t high = 0;
	uint64_t msr_num;
  if(mode == 0){
	msr_num = IIO_PCIE_1_PORT_0_BW_IN;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_frc_sample = ((uint64_t)high << 32) | low;
	prev_cum_frc = cur_cum_frc;
	cur_cum_frc = cum_frc_sample;
  } else {
  msr_num = IIO_PCIE_1_PORT_0_BW_OUT;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_frc_rd_sample = ((uint64_t)high << 32) | low;
	prev_cum_frc_rd = cur_cum_frc_rd;
	cur_cum_frc_rd = cum_frc_rd_sample;
  }
}

static void sample_mba_time_counter(void){
  tsc_sample_mba = rdtscp();
	prev_rdtsc_mba = cur_rdtsc_mba;
	cur_rdtsc_mba = tsc_sample_mba;
}

static void sample_counters_pcie_bw(int c){
	//first pcie bw
  sample_pcie_bw_counter(c);
	//sample time at the last
	sample_mba_time_counter();
	return;
}

static void update_pcie_bw(void){
  latest_time_delta_mba_ns = ((cur_rdtsc_mba - prev_rdtsc_mba) * 10) / 33;
	if(latest_time_delta_mba_ns > 0){
		// latest_measured_avg_pcie_bw = (uint32_t)((((float)(cur_cum_frc - prev_cum_frc)) / ((float)(latest_time_delta_ns)) ) * 32);
    if(mode == 0){
		latest_avg_pcie_bw = (cur_cum_frc - prev_cum_frc) / (latest_time_delta_mba_ns >> 5);
        if(latest_avg_pcie_bw < 150){
            smoothed_avg_pcie_bw = ((255*smoothed_avg_pcie_bw) + (latest_avg_pcie_bw << 10)) >> 8;
        }
    } else{
    latest_avg_pcie_bw_rd = (cur_cum_frc_rd - prev_cum_frc_rd) / (latest_time_delta_mba_ns >> 5);
        if(latest_avg_pcie_bw_rd < 150){
            smoothed_avg_pcie_bw_rd = ((255*smoothed_avg_pcie_bw_rd) + (latest_avg_pcie_bw_rd << 10)) >> 8;
        }
    }
	}
}

static void update_mba_msr_register(void){
  uint32_t low = 0;
	uint32_t high = 0;
  uint64_t msr_num = PQOS_MSR_MBA_MASK_START + MBA_COS_ID;
  wrmsr_on_cpu(NUMA1_CORE,msr_num,low,high);
  wrmsr_on_cpu(NUMA2_CORE,msr_num,low,high);
  wrmsr_on_cpu(NUMA3_CORE,msr_num,low,high);
}

static void update_mba_process_scheduler(void){
    WARN_ON(!(latest_mba_val <= 4));
    if(latest_mba_val == 4){
        send_signal_to_pid(app_pid,SIGSTOP);
    }
    else{
        send_signal_to_pid(app_pid,SIGCONT);
    }
}

static void increase_mba_val(void){
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

static void decrease_mba_val(void){
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

static void update_mba_val(void){
  if(mode == 0){
    //Rx side logic
    if((smoothed_avg_pcie_bw >> 10) < (PCIE_BW_THRESHOLD)){
        if(latest_measured_avg_occ > IIO_THRESHOLD){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw >> 10) > (PCIE_BW_THRESHOLD)){
        if(latest_measured_avg_occ < IIO_THRESHOLD){
            decrease_mba_val();
        }
    }
  } else{
    //Tx side logic
    if((smoothed_avg_pcie_bw_rd >> 10) < (PCIE_BW_THRESHOLD)){
        if(latest_measured_avg_occ_rd > IIO_RD_THRESHOLD){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw_rd >> 10) > (PCIE_BW_THRESHOLD)){
        if(latest_measured_avg_occ < IIO_THRESHOLD){
            decrease_mba_val();
        }
    }
  }
}

void poll_mba_init(void) {
    #if USE_PROCESS_SCHEDULER
    app_pid = target_pid;
    printk("MLC PID: %ld\n",app_pid);
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
}


static void thread_fun_poll_mba(struct work_struct *work) {  
  int cpu = NUMA0_CORE;
  uint32_t budget = WORKER_BUDGET;
  while (budget) {
    sample_counters_pcie_bw(cpu);
    update_pcie_bw();
    if(mode == 0){
    latest_measured_avg_occ = smoothed_avg_occ >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    } else{
    latest_measured_avg_occ_rd = smoothed_avg_occ_rd >> 10; //to reflect a consistent IIO occupancy value in log and MBA update logic
    }
    update_mba_val();
    if(!terminate_hcc_logging){
      update_log_mba(cpu);
    }
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