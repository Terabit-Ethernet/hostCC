#include <stdio.h>				// printf, etc
#include <stdint.h>				// standard integer types, e.g., uint32_t
#include <signal.h>				// for signal handler
#include <stdlib.h>				// exit() and EXIT_FAILURE
#include <string.h>				// strerror() function converts errno to a text string for printing
#include <fcntl.h>				// for open()
#include <errno.h>				// errno support
#include <assert.h>				// assert() function
#include <unistd.h>				// sysconf() function, sleep() function
#include <sys/mman.h>			// support for mmap() function
#include <math.h>				// for pow() function used in RAPL computations
#include <time.h>
#include <sys/time.h>			// for gettimeofday
#include <sys/ipc.h>
#include <sys/shm.h>

#define LOG_FREQUENCY 1
#define LOG_PRINT_FREQUENCY 20
#define LOG_SIZE 100000
#define WEIGHT_FACTOR 8
#define WEIGHT_FACTOR_LONG_TERM 256
#define IIO_MSR_PMON_CTL_BASE 0x0A48L
#define IIO_MSR_PMON_CTR_BASE 0x0A41L
#define IIO_PCIE_1_PORT_0_BW_IN 0x0B20 //We're concerned with PCIe 1 stack on our machine (Table 1-11 in Intel Skylake Manual)
#define STACK 2 //We're concerned with stack #2 on our machine
#define VTD_OCC_VAL_L4_PAGE_HIT 0x400000400141
#define VTD_OCC_VAL_L1_MISS 0x400000400441
#define VTD_OCC_VAL_L2_MISS 0x400000400841
#define VTD_OCC_VAL_L3_MISS 0x400000401041
#define VTD_OCC_VAL_TLB_MISS 0x400000402041
#define CORE 28
#define NUM_LPROCS 64

int msr_fd[NUM_LPROCS];		// msr device driver files will be read from various functions, so make descriptors global

FILE *log_file;

struct log_entry{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t avg_occ; //latest measured avg IIO occupancy
	uint64_t s_avg_occ; //latest calculated smoothed occupancy
	uint64_t s_avg_occ_longterm; //latest calculated smoothed occupancy long term
	int cpu; //current cpu
};

struct log_entry LOG[LOG_SIZE];
uint32_t log_index = 0;
uint32_t counter = 0;
uint64_t prev_rdtsc = 0;
uint64_t cur_rdtsc = 0;
uint64_t prev_cum_occ = 0;
uint64_t cur_cum_occ = 0;
uint64_t prev_cum_frc = 0;
uint64_t cur_cum_frc = 0;
uint64_t tsc_sample = 0;
uint64_t msr_num;
uint64_t msr_val;
uint64_t rc64;
uint64_t cum_occ_sample = 0;
uint64_t cum_frc_sample = 0;
uint64_t latest_avg_occ = 0;
uint64_t latest_avg_pcie_bw = 0;
uint64_t latest_time_delta = 0;
uint64_t smoothed_avg_occ = 0;
uint64_t smoothed_avg_occ_longterm = 0;
uint64_t smoothed_avg_pcie_bw = 0;
float smoothed_avg_occ_f = 0.0;
float smoothed_avg_occ_longterm_f = 0.0;
float smoothed_avg_pcie_bw_f = 0.0;
uint64_t latest_time_delta_us = 0;
uint64_t latest_time_delta_ns = 0;

static inline __attribute__((always_inline)) unsigned long rdtsc()
{
   unsigned long a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return (a | (d << 32));
}


static inline __attribute__((always_inline)) unsigned long rdtscp()
{
   unsigned long a, d, c;

   __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

   return (a | (d << 32));
}

extern inline __attribute__((always_inline)) int get_core_number()
{
   unsigned long a, d, c;

   __asm__ volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));

   return ( c & 0xFFFUL );
}

void rdmsr_userspace(int core, uint64_t rd_msr, uint64_t *rd_val_addr){
    rc64 = pread(msr_fd[core],rd_val_addr,sizeof(rd_val_addr),rd_msr);
    if (rc64 != sizeof(rd_val_addr)) {
        fprintf(log_file,"ERROR: failed to read MSR %lx on Logical Processor %d", rd_msr, core);
        exit(-1);
    }
}

void wrmsr_userspace(int core, uint64_t wr_msr, uint64_t *wr_val_addr){
    rc64 = pwrite(msr_fd[core],wr_val_addr,sizeof(wr_val_addr),wr_msr);
    if (rc64 != 8) {
        fprintf(log_file,"ERROR writing to MSR device on core %d, write %ld bytes\n",core,rc64);
        exit(-1);
    }
}

static void update_log(int c){
	LOG[log_index % LOG_SIZE].l_tsc = cur_rdtsc;
	LOG[log_index % LOG_SIZE].td_ns = latest_time_delta_ns;
	LOG[log_index % LOG_SIZE].avg_occ = latest_avg_occ;
	LOG[log_index % LOG_SIZE].s_avg_occ = smoothed_avg_occ;
	LOG[log_index % LOG_SIZE].s_avg_occ_longterm = smoothed_avg_occ_longterm;
	LOG[log_index % LOG_SIZE].cpu = c;
	log_index++;
}

static void update_occ_ctl_reg(void){
	//program the desired CTL register to read the corresponding CTR value
	msr_num = IIO_MSR_PMON_CTL_BASE + (0x20 * STACK) + 0;
    uint64_t wr_val = VTD_OCC_VAL_L4_PAGE_HIT;
	wrmsr_userspace(CORE,msr_num,&wr_val);
}

static void sample_iio_occ_counter(int c){
    uint64_t rd_val = 0;
	msr_num = IIO_MSR_PMON_CTR_BASE + (0x20 * STACK) + 0;
	rdmsr_userspace(c,msr_num,&rd_val);
	cum_occ_sample = rd_val;
	prev_cum_occ = cur_cum_occ;
	cur_cum_occ = cum_occ_sample;
}

static void sample_time_counter(){
    tsc_sample = rdtscp();
	prev_rdtsc = cur_rdtsc;
	cur_rdtsc = tsc_sample;
}

static void sample_counters(int c){
	//first sample occupancy
	sample_iio_occ_counter(c);
	//sample time at the last
	sample_time_counter();
	return;
}

static void update_occ(void){
	// latest_time_delta_us = (cur_rdtsc - prev_rdtsc) / 3300;
	latest_time_delta_ns = ((cur_rdtsc - prev_rdtsc) * 10) / 33;
	if(latest_time_delta_ns > 0){
		latest_avg_occ = (cur_cum_occ - prev_cum_occ) / (latest_time_delta_ns >> 1);
        if(latest_avg_occ > 10){
            smoothed_avg_occ_f = ((((float) (WEIGHT_FACTOR-1))*smoothed_avg_occ_longterm_f) + latest_avg_occ) / ((float) WEIGHT_FACTOR);
            smoothed_avg_occ = (uint64_t) smoothed_avg_occ_f;

            smoothed_avg_occ_longterm_f = ((((float) (WEIGHT_FACTOR_LONG_TERM-1))*smoothed_avg_occ_longterm_f) + latest_avg_occ) / ((float) WEIGHT_FACTOR_LONG_TERM);
            smoothed_avg_occ_longterm = (uint64_t) smoothed_avg_occ_longterm_f;
        }
		// smoothed_avg_occ = ((7*smoothed_avg_occ) + latest_avg_occ) >> 3;
	}
	// (float(occ[i] - occ[i-1]) / ((float(time_us[i+1] - time_us[i])) * 1e-6 * freq)); 
}

void main_init() {
    //initialize the log
    int i=0;
    while(i<LOG_SIZE){
        LOG[i].l_tsc = 0;
        LOG[i].td_ns = 0;
        LOG[i].avg_occ = 0;
        LOG[i].s_avg_occ = 0;
        LOG[i].s_avg_occ_longterm = 0;
        LOG[i].cpu = 65;
        i++;
    }
    update_occ_ctl_reg();
}

void main_exit() {
    //dump log info
    int i=0;
    fprintf(log_file,
    "index,latest_tsc,time_delta_ns,avg_occ,s_avg_occ,s_avg_occ_long,cpu\n");
    while(i<LOG_SIZE){
        fprintf(log_file,"%d,%ld,%ld,%ld,%ld,%ld,%d\n",
        i,
        LOG[i].l_tsc,
        LOG[i].td_ns,
        LOG[i].avg_occ,
        LOG[i].s_avg_occ,
        LOG[i].s_avg_occ_longterm,
        LOG[i].cpu);
        i++;
    }
}

static void catch_function(int signal) {
	printf("Caught SIGCONT. Shutting down...\n");
    main_exit();
	exit(0);
}

int main(){
    if (signal(SIGINT, catch_function) == SIG_ERR) {
		fprintf(log_file, "An error occurred while setting the signal handler.\n");
		return EXIT_FAILURE;
	}

    char filename[100];
    sprintf(filename,"iio.log");
    log_file = fopen(filename,"w+");
    if (log_file == 0) {
        fprintf(stderr,"ERROR %s when trying to open log file %s\n",strerror(errno),filename);
        exit(-1);
    }

    int nr_cpus = NUM_LPROCS;
    int i;
    for (i=0; i<nr_cpus; i++) {
		sprintf(filename,"/dev/cpu/%d/msr",i);
		msr_fd[i] = open(filename, O_RDWR);
		// printf("   open command returns %d\n",msr_fd[i]);
		if (msr_fd[i] == -1) {
			fprintf(log_file,"ERROR %s when trying to open %s\n",strerror(errno),filename);
			exit(-1);
		}
	}

    // int cpu = get_core_number();
    int cpu = CORE;
    main_init();
    
    while(1){
        sample_counters(cpu);
        update_occ();
        update_log(cpu);
        counter++;
    }

    main_exit();
    return 0;
}
