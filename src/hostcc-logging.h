#ifndef HOSTCC_LOGGING_H
#define HOSTCC_LOGGING_H

// logging vars
struct log_entry_iio_rd{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t avg_occ_rd; //latest measured avg IIO occupancy
	uint64_t s_avg_occ_rd; //latest calculated smoothed occupancy
	int cpu; //current cpu
};

struct log_entry_iio_wr{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t avg_occ; //latest measured avg IIO occupancy
	uint64_t s_avg_occ; //latest calculated smoothed occupancy
	int cpu; //current cpu
};

struct log_entry_mba{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	char ktime[32]; //latest measured time delta in us
	int cpu; //current cpu
	uint32_t mba_val; // latest MBA value (from 0 to 4, increasing value denotes lower CPU-Memory Bandwidth)
	uint32_t m_avg_occ; //latest measured avg IIO occupancy
	uint32_t s_avg_pcie_bw; //smoothed average PCIe bandwidth
	uint32_t avg_pcie_bw;  //latest PCIe bandwidth sample
	uint32_t m_avg_occ_rd; //latest measured avg IIO Rd occupancy
	uint32_t s_avg_pcie_bw_rd; //smoothed average PCIe Rd bandwidth
	uint32_t avg_pcie_bw_rd;  //latest PCIe Rd bandwidth sample
	uint32_t avg_rd_mem_bw;  //Rd memory bandwidth
	uint32_t avg_wr_mem_bw;  //Wr memory bandwidth
	uint32_t task_state;
};

struct log_entry_nf{
	uint64_t l_tsc; //latest TSC
	uint64_t td_ns; //latest measured time delta in us
	uint64_t m_avg_occ; //latest measured avg IIO occupancy
	int cpu; //current cpu
	uint32_t dat_len; //IP datagram length at last sample
};

#define LOG_SIZE 500000

void update_log_nf(int c);
void init_nf_log(void);
void dump_nf_log(void);
void update_log_iio_rd(int c);
void init_iio_rd_log(void);
void dump_iio_rd_log(void);
void update_log_iio_wr(int c);
void init_iio_wr_log(void);
void dump_iio_wr_log(void);
void update_log_mba(int c);
void init_mba_log(void);
void dump_mba_log(void);

#endif