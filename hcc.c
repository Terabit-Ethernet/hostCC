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
  else { //if packet is Rx
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
  const char* interfaceName = outdev->name;
  // printk("Interface Name: %s\n", interfaceName);
  if(strcmp(interfaceName,"ens2f1") != 0){
    return NF_ACCEPT;
  }
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
  // dump_nf_log();
}

// IIO Read occupancy logic (obtained via CHA counters)
static void update_iio_rd_occ_ctl_reg(void){
  //program the desired CTL register to read the corresponding CTR value
  uint64_t msr_num;
	msr_num = IIO_MSR_PMON_CTL_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  uint32_t low = IIO_OCC_VAL & 0xFFFFFFFF;
	uint32_t high = IIO_OCC_VAL >> 32;
  wrmsr_on_cpu(CORE_IIO_RD,msr_num,low,high);
}

static void sample_iio_rd_occ_counter(int c){
  uint64_t msr_num;
  uint32_t low = 0;
	uint32_t high = 0;
	msr_num = IIO_MSR_PMON_CTR_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_occ_sample_rd = ((uint64_t)high << 32) | low;
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
		latest_avg_occ_rd = (cur_cum_occ_rd - prev_cum_occ_rd) / (latest_time_delta_iio_rd_ns);
    // ((occ[i] - occ[i-1]) / (((time_us[i+1] - time_us[i])) * 1e-6 * freq)); 
    // IIO counter operates at thee frequency of 1000MHz
        if(latest_avg_occ_rd >= 0){
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

// IIO Write occupancy logic
static void update_iio_occ_ctl_reg(void){
	//program the desired CTL register to read the corresponding CTR value
  uint64_t msr_num;
	msr_num = IRP_MSR_PMON_CTL_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  uint32_t low = IRP_OCC_VAL & 0xFFFFFFFF;
	uint32_t high = IRP_OCC_VAL >> 32;
  wrmsr_on_cpu(CORE_IIO,msr_num,low,high);
}

static void sample_iio_occ_counter(int c){
  uint64_t msr_num;
  uint32_t low = 0;
	uint32_t high = 0;
	msr_num = IRP_MSR_PMON_CTR_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
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
    // IRP counter operates at the frequency of 500MHz
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
    // dump_iio_log();
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

// Convert PCI(bus:device.function,offset) to uint32_t array index
uint32_t PCI_cfg_index(unsigned int Bus, unsigned int Device, unsigned int Function, unsigned int Offset)
{
    uint32_t byteaddress;
    uint32_t index;
    byteaddress = (Bus<<20) | (Device<<15) | (Function<<12) | Offset;
    index = byteaddress / 4;
    return ( index );
}

static void sample_imc_counters(void){
    int bus, device, function, offset, imc, channel, subchannel, counter;
    uint32_t index, low, high;
    uint64_t count;

    //first sample IMC counters
    bus = IMC_BUS_Socket[SOCKET];
    for (channel=0; channel<NUM_IMC_CHANNELS; channel++) {
        device = IMC_Device_Channel[channel];
        function = IMC_Function_Channel[channel];
        for (counter=0; counter<NUM_IMC_COUNTERS; counter++) {
            offset = IMC_PmonCtr_Offset[counter];
            index = PCI_cfg_index(bus, device, function, offset);
            low = mmconfig_ptr[index];
            high = mmconfig_ptr[index+1];
            count = ((uint64_t) high) << 32 | (uint64_t) low;
            imc_counts[channel][counter] = count;
            prev_imc_counts[channel][counter] = cur_imc_counts[channel][counter];
            cur_imc_counts[channel][counter] = count;
        }
    }
    // printk("Current count: %ld\n",count);
}

static void sample_counters_pcie_bw(int c){
  //sample the memory bandwidth
  // sample_imc_counters();
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

static void update_imc_bw(void){
  int channel;
	latest_time_delta_mba_ns = ((cur_rdtsc_mba - prev_rdtsc_mba) * 10) / 33;
	if(latest_time_delta_mba_ns > 0){
        latest_avg_rd_bw = 0;
        latest_avg_wr_bw = 0;
        for(channel=0;channel<NUM_IMC_CHANNELS;channel++){
            latest_avg_rd_bw += (cur_imc_counts[channel][0] - prev_imc_counts[channel][0]) * 64 / (latest_time_delta_mba_ns);
            latest_avg_wr_bw += (cur_imc_counts[channel][1] - prev_imc_counts[channel][1]) * 64 / (latest_time_delta_mba_ns);
        }
	}
	// float(log[i] - log[i-1])*cacheline / (float(times[i] - times[i-1]) * 1e-6);
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
    if((smoothed_avg_pcie_bw) < (PCIE_BW_THRESHOLD << 10)){
        if(latest_measured_avg_occ > IIO_THRESHOLD){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw) > (PCIE_BW_THRESHOLD << 10)){
        if(latest_measured_avg_occ < IIO_THRESHOLD){
            decrease_mba_val();
        }
    }
  } else{
    //Tx side logic
    if((smoothed_avg_pcie_bw_rd) < (PCIE_BW_THRESHOLD << 10)){
        if(latest_measured_avg_occ_rd > IIO_RD_THRESHOLD){
            increase_mba_val();
        }
    }

    if((smoothed_avg_pcie_bw_rd ) > (PCIE_BW_THRESHOLD << 10)){
        if(latest_measured_avg_occ_rd < IIO_RD_THRESHOLD){
            decrease_mba_val();
        }
    }
  }
}

static void mba_test(int threshold_us_up, int threshold_us_down, int cpu){
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

static void update_imc_config(void){
	//program the desired BDF values to measure IMC counters
  int bus, device, function, offset, imc, channel, subchannel, socket, counter;
  uint32_t index, value;
  for(channel=0;channel<NUM_IMC_CHANNELS;channel++){
    for(counter=0;counter<NUM_IMC_COUNTERS;counter++){
      bus = IMC_BUS_Socket[SOCKET];
      device = IMC_Device_Channel[channel];
      function = IMC_Function_Channel[channel];
      offset = IMC_PmonCtl_Offset[counter];
      index = PCI_cfg_index(bus, device, function, offset);
      if(counter==0){
        mmconfig_ptr[index] = IMC_RD_COUNTER;
      }
      else{
        mmconfig_ptr[index] = IMC_WR_COUNTER;
      }
    }
  }
}

void init_mmconfig(void){
  int mem_fd;
  unsigned long mmconfig_base=0x80000000;		// DOUBLE-CHECK THIS ON NEW SYSTEMS!!!!!   grep MMCONFIG /proc/iomem | awk -F- '{print $1}'
  unsigned long mmconfig_size=0x10000000;
  mmconfig_ptr = ioremap(mmconfig_base, mmconfig_size);
  printk("mmconfig virt address: %p\n", mmconfig_ptr);
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


static void thread_fun_poll_mba(struct work_struct *work) {  
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