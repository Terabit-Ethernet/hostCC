#ifndef HOSTCC_NETWORK_RESPONSE_H
#define HOSTCC_NETWORK_RESPONSE_H

#include "hostcc.h"

//netfilter vars
static struct nf_hook_ops *nf_markecn_ops_rx = NULL;
static struct nf_hook_ops *nf_markecn_ops_tx = NULL;
DEFINE_SPINLOCK(etx_spinlock_rx);
DEFINE_SPINLOCK(etx_spinlock_tx);

//Netfilter related vars
enum {
	INET_ECN_NOT_ECT = 0,
	INET_ECN_ECT_1 = 1,
	INET_ECN_ECT_0 = 2,
	INET_ECN_CE = 3,
	INET_ECN_MASK = 3,
};

#define NO_ECN_MARKING 0
u64 prev_rdtsc_nf = 0;
extern u64 cur_rdtsc_nf = 0;
u64 tsc_sample_nf = 0;
extern u64 latest_measured_avg_occ_nf = 0;
extern u64 latest_measured_avg_occ_rd_nf = 0;
extern u64 latest_time_delta_nf_ns = 0;
extern u32 latest_datagram_len = 0;

static void sample_counters_nf(int c);
static unsigned int nf_markecn_handler_rx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static unsigned int nf_markecn_handler_tx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static int nf_init(void);
static void nf_exit(void);

#endif