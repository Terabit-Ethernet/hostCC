#ifndef HOSTCC_H
#define HOSTCC_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/threads.h>
#include <linux/delay.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/string.h>
#include <net/ip.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/io.h>
#include <linux/random.h>
#include "vars.h"
#include "intel-cascadelake-params.h"

static inline __attribute__((always_inline)) unsigned long rdtscp(void)
{
   unsigned long a, d, c;

   __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

   return (a | (d << 32));
}

#endif





