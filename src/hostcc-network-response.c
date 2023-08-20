#include "hostcc.h"
#include "hostcc-network-response.h"
#include "hostcc-logging.h"

extern bool terminate_hcc;
extern bool terminate_hcc_logging;
extern int mode;
extern int target_pid;
extern int target_pcie_thresh;
extern int target_iio_wr_thresh;
extern int target_iio_rd_thresh;

extern uint64_t smoothed_avg_occ_rd;
extern uint64_t smoothed_avg_occ_wr;

DEFINE_SPINLOCK(etx_spinlock_rx);
DEFINE_SPINLOCK(etx_spinlock_tx);

u64 latest_measured_avg_occ_wr_nf = 0;
u64 latest_measured_avg_occ_rd_nf = 0;
u64 latest_time_delta_nf_ns = 0;
u32 latest_datagram_len = 0;

u64 tsc_sample_nf = 0;
u64 cur_rdtsc_nf = 0;
u64 prev_rdtsc_nf = 0;

//Netfilter logic to mark ECN bits
void sample_counters_nf(int c){
  if(mode == 0){
    latest_measured_avg_occ_wr_nf = smoothed_avg_occ_wr >> 10;
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

unsigned int nf_markecn_handler_rx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
   struct net_device *indev = state->in;
   const char* interfaceName = indev->name;
   if(strcmp(interfaceName,NIC_INTERFACE) != 0){
    return NF_ACCEPT;
  }
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
    if(latest_measured_avg_occ_wr_nf > target_iio_wr_thresh){
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

unsigned int nf_markecn_handler_tx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
  struct net_device *outdev = state->out;
  const char* interfaceName = outdev->name;
  if(strcmp(interfaceName,NIC_INTERFACE) != 0){
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
    if(latest_measured_avg_occ_rd_nf > target_iio_rd_thresh){
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

int nf_init(void) {
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

void nf_exit(void) {
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
  if(ECN_LOGGING){
    dump_nf_log();
  }
}