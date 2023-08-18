#ifndef HOSTCC_NETWORK_RESPONSE_H
#define HOSTCC_NETWORK_RESPONSE_H

#include "hostcc.h"

//netfilter vars
static struct nf_hook_ops *nf_markecn_ops_rx = NULL;
static struct nf_hook_ops *nf_markecn_ops_tx = NULL;

//Netfilter related vars
enum {
	INET_ECN_NOT_ECT = 0,
	INET_ECN_ECT_1 = 1,
	INET_ECN_ECT_0 = 2,
	INET_ECN_CE = 3,
	INET_ECN_MASK = 3,
};

#define NO_ECN_MARKING 0
extern u64 cur_rdtsc_nf;
extern u64 latest_measured_avg_occ_nf;
extern u64 latest_measured_avg_occ_rd_nf;
extern u64 latest_time_delta_nf_ns;
extern u32 latest_datagram_len;

void sample_counters_nf(int c);
unsigned int nf_markecn_handler_rx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
unsigned int nf_markecn_handler_tx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
int nf_init(void);
void nf_exit(void);

#endif