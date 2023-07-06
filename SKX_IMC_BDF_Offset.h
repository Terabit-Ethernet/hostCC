// ================ Machine-Dependent Uncore Performance Monitor Locations =================
// These are the most common bus/device/function locations for the IMC counters on 
// Xeon Platinum 8160 (Skylake, SKX)
// These are set up to work on the TACC Stampede2 SKX nodes....
//
// Note that the PmonCtl offsets are for programmable counters 0-3, plus the fixed counter.
//     (The fixed counter only needs bit 22 enabled, most other bits are ignored)
// Note that the PmonCtr offsets are for the bottom 32 bits of a 48 bit counter in a
//     64-bit field.  The first four offsets are for the programmable counters 0-3,
//     and the final value is for the Fixed-Functin (DCLK) counter that should
//     always increment at the DCLK frequency (1/2 the DDR transfer frequency).

#define NUM_IMC_CHANNELS 6
#define NUM_IMC_COUNTERS 2
#define SOCKET 0

int IMC_BUS_Socket[4] = {0x24, 0x5a, 0x9a, 0xda};
int IMC_Device_Channel[6] = {0x0a, 0x0a, 0x0b, 0x0c, 0x0c, 0x0d};
int IMC_Function_Channel[6] = {0x02, 0x06, 0x02, 0x02, 0x06, 0x02};
int IMC_PmonCtl_Offset[5] = {0xd8, 0xdc, 0xe0, 0xe4, 0xf0}; 
int IMC_PmonCtr_Offset[5] = {0xa0, 0xa8, 0xb0, 0xb8, 0xd0};

#define IMC_RD_COUNTER 0x00400304
#define IMC_WR_COUNTER 0x00400C04

// ================ End of Machine-Dependent Uncore Performance Monitor Locations =================
