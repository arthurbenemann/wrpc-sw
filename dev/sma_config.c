#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <wrc.h>
#include <wrpc.h>
#include "hw/sma_config_regs.h"
#include "dev/sma_config.h"
#include "shell.h"


#define smacfg_write(reg, val) \
    *(volatile uint32_t *) (BASE_SMA_CFG + (offsetof(struct SMA_CONFIG_WB, reg))) = (val)

#define smacfg_read(reg) \
    *(volatile uint32_t *) (BASE_SMA_CFG + (offsetof(struct SMA_CONFIG_WB, reg)))


int sma_sel(uint32_t port, uint32_t source){
    // Selcte the output signal of SMA interface.
    //   Port : 0/1 
    // Source : 0 for PPS 
    //          1 for utc coding 
    //          2 for customed freq 
    if(port < 2 && source < 3){
        if(port == 0)
            smacfg_write(SMA0_MUX,SMA_CONFIG_SMA0_MUX_MUX_W(source));       
        else
            smacfg_write(SMA1_MUX,SMA_CONFIG_SMA1_MUX_MUX_W(source));
        return 0;
    }
    else
        return 1;
}

int cus_period_set(uint32_t prh, uint32_t prl){
    // Configure customed signal parameters.
    //    prh : High level signal duration (1/500M)
    //    prl : Low level signal duration (1/500M)
    if(prh<500000000 && prl<500000000){
        smacfg_write(CUS_PRH, prh);
        smacfg_write(CUS_PRL, prl);
        return 0;
    }
    else
        return 1;
}

int cus_coarse_delay(uint32_t csr){
    // Configure coarse delay parameters.
    //    csr : coarse shift (1/500M)
    if(csr < 500000000){
        smacfg_write(CUS_CSR, csr);
        return 0;
    }
    else
        return 1;
}

int pps_period_set(uint32_t prh){
    // Configure PPS high state width.
    //    prh : coarse shift (1/500M)
    if(prh < 500000000){
        smacfg_write(PPS_PRH, prh);
        return 0;
    }
    else
        return 1;
}

int pps_coarse_delay(uint32_t csr){
    // Configure coarse delay parameters.
    //    csr : coarse shift (1/500M)
    if(csr < 500000000){
        smacfg_write(PPS_CSR, csr);
        return 0;
    }
    else
        return 1;
}

int utc_coarse_delay(uint32_t csr){
    // Configure coarse delay parameters.
    //    csr : coarse shift (1/500M)
    if(csr < 500000000){
        smacfg_write(UTC_CSR, csr);
        return 0;
    }
    else
        return 1;
}

int sma_fdly_set(uint32_t port, uint32_t step){
    // Set fine delay of two sma channel.
    //   port : 0/1 
    //   step : 11ps/step     
    if((port == 0) && (step < 512)){
        smacfg_write(SMA0_FDLY, step);
        return 0;
    }
    else if((port == 1) && (step < 512)){
        smacfg_write(SMA1_FDLY, step);
        return 0;
    }
    else{
        return 1;
    }
}

void sma_cfg_help(){
    pp_printf("Usage: \n");
    pp_printf("auxcfg [type] [cdly] [hcnt] [lcnt]\n");
    pp_printf("type : [0]-PPS; [1]-TAI; [2]-PWM\n");    
    pp_printf(" PPS : [cdly x 2ns] [hcnt x 16ns]\n");
    pp_printf(" TAI : [cdly x 2ns]\n");
    pp_printf(" PWM : [cdly x 2ns] [hcnt x 2ns] [lcnt x 2ns]\n");
}

static int cmd_smacfg(const char *args[]){
    int type, cdly, high, low;
    if (!args[0] || !args[1]) {
        sma_cfg_help();
        return -1;
    }

    type = atoi(args[0]);
    cdly = atoi(args[1]);

    if(type == 0){
        pps_coarse_delay(cdly);
        if (!args[2]) {
            sma_cfg_help();
            return -1;
        }
        high = atoi(args[2]);
        pps_period_set(high);
        return 0;

    }else if(type == 1){
        utc_coarse_delay(cdly);
        return 0;

    }else if(type == 2){
        cus_coarse_delay(cdly);
        if(!args[2] || !args[3]){
            sma_cfg_help();
            return -1;
        }
        high = atoi(args[2]);
        low  = atoi(args[3]);
        if(high+low >= 8){
            cus_period_set(high, low);
            return 0;
        }
    }
    sma_cfg_help();
    return -1;
}

DEFINE_WRC_COMMAND(auxcfg) = {
    .name = "auxcfg",
    .exec = cmd_smacfg,
};

void fine_delay_help(){
    pp_printf("Usage: \n");
    pp_printf("auxfdly [CH] [Step]\n");
    pp_printf("   CH   : [0/1]\n");
    pp_printf("  Steps : [0:511] x 11ps\n");
}

static int cmd_fdly(const char *args[]){
    
    int delay, channel;
    if (!args[0] || !args[1]) {
        fine_delay_help();
        return -1;
    }
    channel = atoi(args[0]);
    delay   = atoi(args[1]);
    if(channel<2 && delay<512 ) {
        sma_fdly_set(channel,delay);
        return 0;
    }    
    fine_delay_help();
    return -1;
}

DEFINE_WRC_COMMAND(auxfdly) = {
 .name = "auxfdly",
 .exec = cmd_fdly,
};

void sma_mux_help(){
    pp_printf("Usage: \n");
    pp_printf("auxmux [CH] [type]\n");
    pp_printf("   CH  : [0/1]\n");
    pp_printf("  type : [0]PPS; [1]TAI; [2]PWM\n");    
}

static int cmd_channelmux(const char *args[]){
    
    int channel, type;
    if (!args[0] || !args[1]) {
        sma_mux_help();
        return -1;
    }
    channel  = atoi(args[0]);
    type     = atoi(args[1]);
    if(channel<2) {
        sma_sel(channel, type);
        return 0;
    };

    sma_mux_help();
    return -1;
}

DEFINE_WRC_COMMAND(auxmux) = {
 .name = "auxmux",
 .exec = cmd_channelmux,
};

// void period_set_help(){
//     pp_printf("Usage: \n");
//     pp_printf(" periodset [channel] [high] [low]\n");
//     pp_printf(" channel : channel selection, 0/1\n");
//     pp_printf(" high    : period high, 0~1048575\n");
//     pp_printf(" low     : period low,  0~1048575\n");
// }

// static int cmd_periodset(const char *args[]){
    
//     int channel, high, low;
//     if (!args[0] || !args[1] || !args[2]) {
//         period_set_help();
//         return -1;
//     }
//     channel  = atoi(args[0]);
//     high     = atoi(args[1]);
//     low      = atoi(args[2]);
//     if(channel<2) {
//         sma_period_set(channel, high, low);
//         return 0;
//     };

//     period_set_help();
//     return -1;
// }

// DEFINE_WRC_COMMAND(periodset) = {
//  .name = "periodset",
//  .exec = cmd_periodset,
// };



// void coarse_delay_help(){
//     pp_printf("Usage: \n");
//     pp_printf("cdly [channel] [delay]\n");
//     pp_printf(" channel : channel selection, 0/1\n");
//     pp_printf(" delay   : actual delay is [delay]x16ns\n");
// }

// static int cmd_cdly(const char *args[])
// {
//     int delay,channel;
//     if (!args[0] || !args[1]) {
//       coarse_delay_help();
//       return -1;
//     }

//     channel = atoi(args[0]);
//     delay   = atoi(args[1]);
//     if(channel<2) {
//         sma_coarse_delay(channel, delay);
//         return 0;
//     };
//     coarse_delay_help();
//     return -1;
// }

// DEFINE_WRC_COMMAND(cdly) = {
//  .name = "cdly",
//  .exec = cmd_cdly,
// };


/* return current aux settings */
static int cmd_aux(const char *args[]){

    uint32_t rd_val1;
    uint32_t rd_val2;
    uint32_t rd_val3;

    if(!args[0]){

        pp_printf("** MUX: CH0 | CH1 \n");
        rd_val1 = smacfg_read(SMA0_MUX);
        rd_val2 = smacfg_read(SMA1_MUX);
        pp_printf(" *type: %3d | %3d \n", rd_val1, rd_val2);

        rd_val1 = smacfg_read(SMA0_FDLY);
        rd_val2 = smacfg_read(SMA1_FDLY);
        pp_printf(" *FDLY: %3d | %3d \n\n", rd_val1, rd_val2);

        pp_printf("** CFG:      cdly|      hcnt|      lcnt\n");
        rd_val1 = smacfg_read(PPS_PRH);
        rd_val2 = smacfg_read(PPS_CSR);
        pp_printf(" * PPS: %9d| %9d|          \n", rd_val2, rd_val1);

        rd_val1 = smacfg_read(UTC_CSR);
        pp_printf(" * TAI: %9d|          |          \n", rd_val1);

        rd_val1 = smacfg_read(CUS_PRH);
        rd_val2 = smacfg_read(CUS_PRL);
        rd_val3 = smacfg_read(CUS_CSR);
        pp_printf(" * PWM: %9d| %9d| %9d\n", rd_val3, rd_val1, rd_val2);


    } else {
		return -EINVAL;
	}
    return 0;
}

DEFINE_WRC_COMMAND(aux) = {
 .name = "aux",
 .exec = cmd_aux,
};

void sma_init(void){
    sma_sel(0,2);
    sma_sel(1,0);
    sma_fdly_set(0,0);
    sma_fdly_set(1,0);
    pps_period_set(8);
    cus_period_set(25,25);
    pps_coarse_delay(0);
    utc_coarse_delay(0);
    cus_coarse_delay(0);
}


