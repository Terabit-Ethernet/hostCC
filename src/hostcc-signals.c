#include "hostcc.h"
#include "hostcc-signals.h"
#include "hostcc-logging.h"
#include "hostcc-local-response.h"

extern bool terminate_hcc = false;
extern bool terminate_hcc_logging = false;

// IIO Read occupancy logic (obtained via CHA counters)
void update_iio_rd_occ_ctl_reg(void){
  //program the desired CTL register to read the corresponding CTR value
  uint64_t msr_num;
	msr_num = IIO_MSR_PMON_CTL_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  uint32_t low = IIO_OCC_VAL & 0xFFFFFFFF;
	uint32_t high = IIO_OCC_VAL >> 32;
  wrmsr_on_cpu(CORE_IIO_RD,msr_num,low,high);
}

void sample_iio_rd_occ_counter(int c){
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

void sample_counters_iio_rd(int c){
  //first sample occupancy
	sample_iio_rd_occ_counter(c);
	//sample time at the last
	sample_iio_rd_time_counter();
	return;
}

void update_iio_rd_occ(void){
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

// IIO Write occupancy logic
void update_iio_occ_ctl_reg(void){
	//program the desired CTL register to read the corresponding CTR value
  uint64_t msr_num;
	msr_num = IRP_MSR_PMON_CTL_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  uint32_t low = IRP_OCC_VAL & 0xFFFFFFFF;
	uint32_t high = IRP_OCC_VAL >> 32;
  wrmsr_on_cpu(CORE_IIO,msr_num,low,high);
}

void sample_iio_occ_counter(int c){
  uint64_t msr_num;
  uint32_t low = 0;
	uint32_t high = 0;
	msr_num = IRP_MSR_PMON_CTR_BASE + (0x20 * STACK) + IIO_COUNTER_OFFSET;
  rdmsr_on_cpu(c,msr_num,&low,&high);
  cum_occ_sample = ((uint64_t)high << 32) | low;
	prev_cum_occ = cur_cum_occ;
	cur_cum_occ = cum_occ_sample;
}

void sample_iio_time_counter(void){
  tsc_sample_iio = rdtscp();
	prev_rdtsc_iio = cur_rdtsc_iio;
	cur_rdtsc_iio = tsc_sample_iio;
}

void sample_counters_iio(int c){
	//first sample occupancy
	sample_iio_occ_counter(c);
	//sample time at the last
	sample_iio_time_counter();
	return;
}

void update_iio_occ(void){
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



// PCIe bandwidth sampling and MBA update logic

void sample_pcie_bw_counter(int c){
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

void sample_mba_time_counter(void){
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

void sample_imc_counters(void){
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

void sample_counters_pcie_bw(int c){
  //sample the memory bandwidth
  // sample_imc_counters();
	//first pcie bw
  sample_pcie_bw_counter(c);
	//sample time at the last
	sample_mba_time_counter();
	return;
}

void update_pcie_bw(void){
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

void update_imc_bw(void){
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

void update_imc_config(void){
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

