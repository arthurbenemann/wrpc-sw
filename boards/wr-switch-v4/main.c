/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2012,2015 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek <adam.wujek@cern.ch>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include <wrc.h>
#include "dev/console.h"
#include "softpll_ng.h"
#include "minipc.h"
#include "revision.h"
#include "system_checks.h"
#include "gpio-wrs.h"

#include "dev/si549.h"
#include "dev/hmc7044.h"
#include "dev/simple_spi.h"
#include "dev/8v54816.h"
#include "dev/tca9548.h"
#include "dev/tca9539.h"
#include "dev/lmx2594.h"

#define HW_NAME_LENGTH 5


#include <wrc_global.h>

struct wrc_global_link wrc_global_link = {
  .version = WRC_G_LINK_VERSION,
  .vlan = 0,
  .ip_state = IP_TRAINING,
};

const struct wrc_global wrc_global = {
  .magic = WRC_G_MAGIC,
  .version = WRC_G_VERSION,
  .global_link = &wrc_global_link,
  .task_list_max = WRC_MAX_TASKS,
  .task_list = tasks,
  .temp_group_list_max = 0,
  .temp_group_list = NULL,
  .softpll =NULL,
  .pll_fifo = NULL,
  .config = NULL,
  .sfp_info = NULL
};

struct lmx2594_config pll_ext_10mhz_cfg =
#include "configs/wrsv4_pll_ext_10mhz.h"

struct hmc7044_config hmc7044_cfg = 
#include "configs/wrsv4_hmc7044_config.h"

//extern struct spll_stats stats;

int scb_ljd_present = 0;

struct rts_10g_board {
  struct wr_si549_interface_device si549;
  struct hmc7044_device hmc7044;
  struct simple_spi_device hmc7044_spi;
  struct wr_8v54816_interface_device cp_8v5816;
  struct wr_tca9548_interface_device mux_tca9548;
  struct wr_tca9539_interface_device gpio_exp;
  struct lmx2594_device lmx2594;
  struct simple_spi_device lmx2594_spi; 
} board;

static uint16_t gpio_exp_gpi_pins[GPIO_EXP_NUM_GPI] = {GPIO_EXP_SI5341_INTR_n};
static uint16_t gpio_exp_gpo_pins[GPIO_EXP_NUM_GPO] = {GPIO_EXP_SI5341_SYNC, \
                                                        GPIO_EXP_IN_SEL0, \
                                                        GPIO_EXP_IN_SEL1, \
                                                        GPIO_EXP_SI53XX_RST, \
                                                        GPIO_EXP_SI57X_OE1, \
                                                        GPIO_EXP_CLK_SW_RST_n};

#define RTS_MBOX_SIZE 0x1000
#define RTS_MBOX_ADDR (DEV_BASE + 0)

volatile uint8_t *mbox_mem = (volatile uint8_t*)( RTS_MBOX_ADDR );

int rts_debug_command(int command, int value)
{
  return 0;
}

/* initialize functions to be called after reset in check_reset function */
void init_hw_after_reset(void)
{
  /* Ok, now init the devices so we can printf and delay */
  console_init();
}

int board_init(void)
{
  wrs_gpio_init();
  uint32_t f_xtal;
  uint8_t data;
  uint16_t reg = 0x0017;
  int ret;
  //board_dbg("board_init()\n");

  wr_si549_interface_init(&board.si549, BASE_SI57X_INTERFACE, 0x67, BOARD_SI549_HELPER_GAIN);
  si549_reset(&board.si549);

  uint8_t id = si549_get_id(&board.si549);
  board_dbg("Si549: chip ID = %u\n", id);

  si549_get_frequency(&board.si549, &f_xtal);
  board_dbg("Si549: current freq = %u Hz\n", f_xtal);

  si549_set_frequency(&board.si549, 62500000+3814, 10);   //fref*((1+2^14)/(2^14))

  board_dbg("Si549: frequency set\n");

  si549_get_frequency(&board.si549, &f_xtal);
  board_dbg("Si549: current freq = %u Hz\n", f_xtal);

  board_dbg("Init HMC7044 SPI\n");
  hmc7044_init(&board.hmc7044, &board.hmc7044_spi, BASE_SPI, &gpio_pin_pll_reset_n, &gpio_pin_pll_clk_sel,
    &gpio_pin_pll_sync, &gpio_pin_pll_gpio1, &gpio_pin_pll_gpio2);
  
  reg = 0x0078;
  data = hmc7044_read(&board.hmc7044, reg);
  board_dbg("ID[0] = 0x%X\n", data);
  
  reg = 0x0079;
  data = hmc7044_read(&board.hmc7044, reg);
  board_dbg("ID[1] = 0x%X\n", data);

  reg = 0x007A;
  data = hmc7044_read(&board.hmc7044, reg);
  board_dbg("ID[2] = 0x%X\n", data);

  if(hmc7044_checkstatus(&board.hmc7044) == 0){
    board_dbg("HMC7044 already configured...\n");   
    //init lmx2594 gm pll
    ret = lmx2594_init(&board.lmx2594, &board.lmx2594_spi, BASE_SPI_LJD_BOARD, &gpio_pin_gm_pll_sync, &gpio_pin_gm_pll_muxout_ld);
    if(lmx2594_configure(&board.lmx2594, &pll_ext_10mhz_cfg) < 0){
      board_dbg("lmx2594_config error\n");
    }else{
      board_dbg("lmx2594 configured\n");
    }   
  }else{
    ret = hmc7044_configure(&board.hmc7044, &hmc7044_cfg);
    if(ret < 0){
      board_dbg("Failed to configure/lock HMC7044: %d\n", ret);
      return ret;
    }
    board_dbg("HMC7044 successfuly configured !!\n");
    //init lmx2594 gm pll
    ret = lmx2594_init(&board.lmx2594, &board.lmx2594_spi, BASE_SPI_LJD_BOARD, &gpio_pin_gm_pll_sync, &gpio_pin_gm_pll_muxout_ld);
    if(lmx2594_configure(&board.lmx2594, &pll_ext_10mhz_cfg) < 0){
      board_dbg("lmx2594_config error\n");
      return -1;
    }

    board_dbg("lmx2594 configured\n");
    
    board_dbg("switch sys clk\n");
    gen_gpio_out(&gpio_pin_sys_clk_sel, 1);

    board_dbg("enable endpoints\n");
    gen_gpio_out(&gpio_pin_ep_reset_n, 1);  
  }
  return 0;
}

int main(void)
{
  uint32_t start_tics = timer_get_tics();

  check_reset();

  mbox_mem[0] = 0xca;
  mbox_mem[1] = 0xfe;
  mbox_mem[2] = 0xba;
  mbox_mem[3] = 0xbe;

  //stats.start_cnt++;

  _endram = ENDRAM_MAGIC;
  console_init();
  wrs_gpio_init();

  pp_printf("\n");
  pp_printf("WR Switch v4 Real Time Subsystem (c) CERN 2011 - 2024\n");
  /*pp_printf("Revision: %s, built: %s %s.\n",
        build_revision, build_date, build_time);*/

  pp_printf("_endram @ 0x%X (= 0x%X)\n", &_endram, _endram);
  pp_printf("_fstack @ 0x%X (= 0x%X)\n", &_fstack, _fstack);
  pp_printf("mbox_mem @ 0x%X ([0..3]= 0x%X)\n", mbox_mem, *((uint32_t*)mbox_mem));
  
  pp_printf("un-reset the peripherals...\n");
  gen_gpio_out(&gpio_pin_periph_reset_n, 1);
  gen_gpio_out(&gpio_pin_pll_reset_n, 1);

  if(board_init() < 0){
    board_dbg("board_init error, abort\n");
    while(1);
  }
  
  rts_init();
  rtipc_init();
  spll_very_init();
  
  spll_init(1,0,1);

  for(;;)
  {
    uint32_t tics = timer_get_tics();

    if (time_after(tics, start_tics + TICS_PER_SECOND/5)) {
      spll_show_stats();
      start_tics = tics;
    }

    rts_update();
    rtipc_action();
    spll_update();
    check_stack();
  }

  return 0;
}
