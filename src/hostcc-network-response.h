#ifndef HOSTCC_NETWORK_RESPONSE_H
#define HOSTCC_NETWORK_RESPONSE_H

#include "hostcc.h"

static struct nf_hook_ops *nf_markecn_ops_rx = NULL;
static struct nf_hook_ops *nf_markecn_ops_tx = NULL;

enum {
	INET_ECN_NOT_ECT = 0,
	INET_ECN_ECT_1 = 1,
	INET_ECN_ECT_0 = 2,
	INET_ECN_CE = 3,
	INET_ECN_MASK = 3,
};


void sample_counters_nf(int c);
unsigned int nf_markecn_handler_rx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
unsigned int nf_markecn_handler_tx(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
int nf_init(void);
void nf_exit(void);

#endif