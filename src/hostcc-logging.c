#include "hostcc-logging.h"

uint32_t log_index_iio_wr = 0;
uint32_t log_index_iio_rd = 0;
uint32_t log_index_pcie = 0;
uint32_t log_index_nf = 0;

struct log_entry_iio_wr LOG_IIO_WR[LOG_SIZE];
struct log_entry_iio_rd LOG_IIO_RD[LOG_SIZE];
struct log_entry_pcie LOG_PCIE[LOG_SIZE];
struct log_entry_nf LOG_NF[LOG_SIZE];

unsigned long long seconds;
unsigned long microseconds;
unsigned long nanoseconds;

void update_log_nf(int c){
	LOG_NF[log_index_nf % LOG_SIZE].l_tsc = cur_rdtsc_nf;
	LOG_NF[log_index_nf % LOG_SIZE].td_ns = latest_time_delta_nf_ns;
	LOG_NF[log_index_nf % LOG_SIZE].m_avg_occ = latest_measured_avg_occ_wr_nf;
	LOG_NF[log_index_nf % LOG_SIZE].cpu = c;
	LOG_NF[log_index_nf % LOG_SIZE].dat_len = latest_datagram_len;
	log_index_nf++;
}


void init_nf_log(void){
  int i=0;
  while(i<LOG_SIZE){
    LOG_NF[i].l_tsc = 0;
    LOG_NF[i].td_ns = 0;
    LOG_NF[i].m_avg_occ = 0;
    LOG_NF[i].cpu = 65;
    LOG_NF[i].dat_len = 0;
    i++;
  }
}

void dump_nf_log(void) {
  int i=0;
  while(i<LOG_SIZE){
    printk("NF:%d,%lld,%lld,%lld,%d,%d\n",
    i,
    LOG_NF[i].l_tsc,
    LOG_NF[i].td_ns,
    LOG_NF[i].m_avg_occ,
    LOG_NF[i].cpu,
    LOG_NF[i].dat_len);
    i++;
  }
}

void update_log_iio_rd(int c){
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].l_tsc = cur_rdtsc_iio_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].td_ns = latest_time_delta_iio_rd_ns;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].avg_occ_rd = latest_avg_occ_rd;
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].s_avg_occ_rd = (smoothed_avg_occ_rd >> 10);
	LOG_IIO_RD[log_index_iio_rd % LOG_SIZE].cpu = c;
	log_index_iio_rd++;
}

void init_iio_rd_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_IIO_RD[i].l_tsc = 0;
      LOG_IIO_RD[i].td_ns = 0;
      LOG_IIO_RD[i].avg_occ_rd = 0;
      LOG_IIO_RD[i].s_avg_occ_rd = 0;
      LOG_IIO_RD[i].cpu = 65;
      i++;
  }
}

void dump_iio_rd_log(void){
  int i=0;
  while(i<LOG_SIZE){
      printk("IIORD:%d,%lld,%lld,%lld,%lld,%d\n",
      i,
      LOG_IIO_RD[i].l_tsc,
      LOG_IIO_RD[i].td_ns,
      LOG_IIO_RD[i].avg_occ_rd,
      LOG_IIO_RD[i].s_avg_occ_rd,
      LOG_IIO_RD[i].cpu);
      i++;
  }
}

void update_log_iio_wr(int c){
	LOG_IIO_WR[log_index_iio_wr % LOG_SIZE].l_tsc = cur_rdtsc_iio_wr;
	LOG_IIO_WR[log_index_iio_wr % LOG_SIZE].td_ns = latest_time_delta_iio_wr_ns;
	LOG_IIO_WR[log_index_iio_wr % LOG_SIZE].avg_occ = latest_avg_occ_wr;
	LOG_IIO_WR[log_index_iio_wr % LOG_SIZE].s_avg_occ = (smoothed_avg_occ_wr >> 10);
	LOG_IIO_WR[log_index_iio_wr % LOG_SIZE].cpu = c;
	log_index_iio_wr++;
}

void init_iio_wr_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_IIO_WR[i].l_tsc = 0;
      LOG_IIO_WR[i].td_ns = 0;
      LOG_IIO_WR[i].avg_occ = 0;
      LOG_IIO_WR[i].s_avg_occ = 0;
      LOG_IIO_WR[i].cpu = 65;
      i++;
  }
}

void dump_iio_wr_log(void){
  int i=0;
  while(i<LOG_SIZE){
      printk("IIO:%d,%lld,%lld,%lld,%lld,%d\n",
      i,
      LOG_IIO_WR[i].l_tsc,
      LOG_IIO_WR[i].td_ns,
      LOG_IIO_WR[i].avg_occ,
      LOG_IIO_WR[i].s_avg_occ,
      LOG_IIO_WR[i].cpu);
      i++;
  }
}

void update_log_pcie(int c){
	LOG_PCIE[log_index_pcie % LOG_SIZE].l_tsc = cur_rdtsc_mba;
	LOG_PCIE[log_index_pcie % LOG_SIZE].td_ns = latest_time_delta_mba_ns;
	LOG_PCIE[log_index_pcie % LOG_SIZE].cpu = c;
	LOG_PCIE[log_index_pcie % LOG_SIZE].mba_val = latest_mba_val;
	LOG_PCIE[log_index_pcie % LOG_SIZE].m_avg_occ = latest_measured_avg_occ_wr;
	LOG_PCIE[log_index_pcie % LOG_SIZE].m_avg_occ_rd = latest_measured_avg_occ_rd;
	LOG_PCIE[log_index_pcie % LOG_SIZE].s_avg_pcie_bw = (smoothed_avg_pcie_bw >> 10);
	LOG_PCIE[log_index_pcie % LOG_SIZE].avg_pcie_bw = latest_avg_pcie_bw;
  LOG_PCIE[log_index_pcie % LOG_SIZE].s_avg_pcie_bw_rd = (smoothed_avg_pcie_bw_rd >> 10);
	LOG_PCIE[log_index_pcie % LOG_SIZE].avg_pcie_bw_rd = latest_avg_pcie_bw_rd;
  if(app_pid_task != NULL){
	  LOG_PCIE[log_index_pcie % LOG_SIZE].task_state = app_pid_task->state;
  }
	log_index_pcie++;
}

void init_pcie_log(void){
  int i=0;
  while(i<LOG_SIZE){
      LOG_PCIE[i].l_tsc = 0;
      LOG_PCIE[i].td_ns = 0;
      LOG_PCIE[i].cpu = 65;
      LOG_PCIE[i].mba_val = 0;
      LOG_PCIE[i].m_avg_occ = 0;
      LOG_PCIE[i].m_avg_occ_rd = 0;
      LOG_PCIE[i].avg_pcie_bw = 0;
      LOG_PCIE[i].s_avg_pcie_bw = 0;
      LOG_PCIE[i].task_state = 0xFFFF;
      i++;
  }
}

void dump_pcie_log(void){
  int i=0;
  while(i<LOG_SIZE){
      printk("PCIE:%d,%lld,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%x\n",
      i,
      LOG_PCIE[i].l_tsc,
      LOG_PCIE[i].td_ns,
      LOG_PCIE[i].cpu,
      LOG_PCIE[i].mba_val,
      LOG_PCIE[i].m_avg_occ,
      LOG_PCIE[i].avg_pcie_bw,
      LOG_PCIE[i].s_avg_pcie_bw,
      LOG_PCIE[i].m_avg_occ_rd,
      LOG_PCIE[i].avg_pcie_bw_rd,
      LOG_PCIE[i].s_avg_pcie_bw_rd,
      LOG_PCIE[i].task_state);
      i++;
  }
}