#include "hcc.h"

module_param(target_pid, int, 0);
MODULE_PARM_DESC(target_pid, "Target process ID");
MODULE_LICENSE("GPL");

//Netfilter logic to mark ECN bits
static void sample_counters_nf(int c){
  latest_measured_avg_occ_nf = smoothed_avg_occ;

	tsc_sample_nf = rdtsc();
	prev_rdtsc_nf = cur_rdtsc_nf;
	cur_rdtsc_nf = tsc_sample_nf;

  latest_time_delta_nf_ns = ((cur_rdtsc_nf - prev_rdtsc_nf) * 10) / 33;
	return;
}

static unsigned int nf_markecn_handler(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	if (!skb) {
		return NF_ACCEPT;
	} else {
		struct sk_buff *sb = NULL;
		struct iphdr *iph;
		sb = skb;
		iph = ip_hdr(sb);

		spin_lock(&etx_spinlock);
		latest_datagram_len = iph->tot_len;
		int cpu = get_cpu();
    sample_counters_nf(cpu);
    update_log_nf(cpu);
    #if !(NO_ECN_MARKING)
    if(latest_measured_avg_occ_nf > IIO_THRESHOLD){
        iph->tos = iph->tos | 0x03;
        iph->check = 0;
        ip_send_check(iph);
    }
    #endif
		spin_unlock(&etx_spinlock);

		return NF_ACCEPT;
	}
}

static int nf_init(void) {
	nf_markecn_ops = (struct nf_hook_ops*)kcalloc(1, sizeof(struct nf_hook_ops), GFP_KERNEL);
	if (nf_markecn_ops != NULL) {
		nf_markecn_ops->hook = (nf_hookfn*)nf_markecn_handler;
		nf_markecn_ops->hooknum = NF_INET_PRE_ROUTING;
		nf_markecn_ops->pf = NFPROTO_IPV4;
		nf_markecn_ops->priority = NF_IP_PRI_FIRST + 1;

		init_nf_log();
		nf_register_net_hook(&init_net, nf_markecn_ops);
	}
	return 0;
}

static void nf_exit(void) {
	if (nf_markecn_ops  != NULL) {
		nf_unregister_net_hook(&init_net, nf_markecn_ops);
		kfree(nf_markecn_ops);

		dump_nf_log();
	}
	printk(KERN_INFO "Exit");
}

// IIO occupancy logic
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
  tsc_sample_iio = rdtsc();
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
            smoothed_avg_occ = ((7*smoothed_avg_occ) + latest_avg_occ) >> 3;
        }
	}
}

void poll_iio_init(void) {
    //initialize the log
    init_iio_log();
    update_iio_occ_ctl_reg();
}

void poll_iio_exit(void) {
    //dump log info
    dump_iio_log();
}

static int thread_fun_poll_iio(void *arg) {
  allow_signal(SIGKILL);
  int cpu = CORE_IIO;
  poll_iio_init();

  printk(KERN_INFO "Starting IIO Sampling");
  while (!kthread_should_stop()) {

    sample_counters_iio(cpu); //sample counters
    update_iio_occ();         //update occupancy value
    update_log_iio(cpu);      //update the log

    if(signal_pending(thread_iio)){
		  break;
    }
  }
  printk(KERN_INFO "Ending IIO Sampling");
  poll_iio_exit();
  do_exit(0);
  return 0;
}

// PCIe bandwidth sampling and MBA update logic

static void sample_pcie_bw_counter(int c){
  uint32_t low = 0;
	uint32_t high = 0;
	uint64_t msr_num = IIO_PCIE_1_PORT_0_BW_IN;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_frc_sample = ((uint64_t)high << 32) | low;
	prev_cum_frc = cur_cum_frc;
	cur_cum_frc = cum_frc_sample;
}

static void sample_mba_time_counter(void){
  tsc_sample_mba = rdtsc();
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
		latest_avg_pcie_bw = (cur_cum_frc - prev_cum_frc) / (latest_time_delta_mba_ns >> 5);
        if(latest_avg_pcie_bw < 150){
            smoothed_avg_pcie_bw = ((255*smoothed_avg_pcie_bw) + (latest_avg_pcie_bw << 10)) >> 8;
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
    uint64_t cur_tsc_val = rdtsc();
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
                last_reduced_tsc = rdtsc();
                break;
            case 1:
                wrmsr_on_cpu(NUMA2_CORE,msr_num,low,high);
                last_reduced_tsc = rdtsc();
                break;
            case 2:
                wrmsr_on_cpu(NUMA3_CORE,msr_num,low,high);
                last_reduced_tsc = rdtsc();
                break;
            case 3:
                #if !(USE_PROCESS_SCHEDULER)
                WARN_ON(!(false));
                #endif
                #if USE_PROCESS_SCHEDULER
                update_mba_process_scheduler();
                last_reduced_tsc = rdtsc();
                #endif
                break;
            default:
                WARN_ON(!(false));
                break;
        }
    }
}

static void update_mba_val(void){
    if((smoothed_avg_pcie_bw >> 10) < (PCIE_BW_THRESHOLD - BW_TOLERANCE)){
        if(latest_measured_avg_occ > IIO_THRESHOLD){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw >> 10) > (PCIE_BW_THRESHOLD + BW_TOLERANCE)){
        if(latest_measured_avg_occ < IIO_THRESHOLD){
            decrease_mba_val();
        }
    }
}

void poll_mba_init(void) {
    //initialize the log
    init_mba_log();
}

void poll_mba_exit(void) {
    //dump log info
    dump_mba_log();
    if(latest_mba_val > 0){
        latest_mba_val = 0;
        update_mba_msr_register();
        #if USE_PROCESS_SCHEDULER
        update_mba_process_scheduler();
        #endif
    }
}


static int thread_fun_poll_mba(void *arg) {
  allow_signal(SIGKILL);

  #if USE_PROCESS_SCHEDULER
  app_pid = target_pid;
  printk("MLC PID: %ld\n",app_pid);
  #endif
  
  int cpu = NUMA0_CORE;
  poll_mba_init();
  printk(KERN_INFO "Starting PCIe Bandwidth Sampling");
  while (!kthread_should_stop()) {
   
    sample_counters_pcie_bw(cpu);
    update_pcie_bw();
    latest_measured_avg_occ = smoothed_avg_occ; //to reflect a consistent IIO occupancy value in log and MBA update logic
    update_mba_val();
    update_log_mba(cpu);

    if(signal_pending(thread_mba)){
		  break;
    }
  }
  printk(KERN_INFO "Ending PCIe Bandwidth Sampling");
  poll_mba_exit();
  do_exit(0);
  return 0;
}

static int __init hcc_init(void) {
  struct sched_param task_sched_params_iio;
  struct sched_param task_sched_params_mba;
  task_sched_params_iio.sched_priority = MAX_RT_PRIO;
  task_sched_params_iio.sched_policy = SCHED_FIFO;
  task_sched_params_mba.sched_priority = MAX_RT_PRIO;
  task_sched_params_mba.sched_policy = SCHED_FIFO;

  /* Create threads. */
  thread_iio = kthread_create(thread_fun_poll_iio, NULL, "poll_iio");
  if(thread_iio == NULL) {
    return -ENOMEM;
  }
  thread_mba = kthread_create(thread_fun_poll_mba, NULL, "poll_mba");
  if(thread_mba == NULL) {
    return -ENOMEM;
  }  

  /* Set the thread's affinity to a dedicated core. */
  // IIO occupancy sampling thread
  kthread_bind(thread_iio, CORE_IIO);
  sched_setscheduler_nocheck(thread_iio, SCHED_FIFO, &task_sched_params_iio);
  wake_up_process(thread_iio);

  // PCIe bandwidth sampling MBA update thread
  kthread_bind(thread_mba, NUMA0_CORE); //assuming the NIC is attached to NUMA0
  sched_setscheduler_nocheck(thread_mba, SCHED_FIFO, &task_sched_params_mba);
  wake_up_process(thread_mba);

  //Start ECN marking
  nf_init();

  return 0;
}

static void __exit hcc_exit(void) {
  nf_exit();
  kthread_stop(thread_mba);
  kthread_stop(thread_iio);
}

module_init(hcc_init);
module_exit(hcc_exit);