#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"
#include "dev/ad951x.h"
#include "dev/ltc6950.h"
#include "dev/ad9910.h"


#define BASE_AUXWB 0x28000

struct gpio_device gpio_aux;

static const struct gpio_pin pin_pll_main_cs_n = { &gpio_aux, 0 };
static const struct gpio_pin pin_pll_main_sdi = { &gpio_aux, 1 };
static const struct gpio_pin pin_pll_main_sdo = { &gpio_aux, 2 };
static const struct gpio_pin pin_pll_main_sclk = { &gpio_aux, 3 };
static const struct gpio_pin pin_pll_main_reset = { &gpio_aux, 4 };
static const struct gpio_pin pin_pll_main_lock = { &gpio_aux, 5 };

static const struct gpio_pin pin_pll_ext_cs_n = { &gpio_aux, 6 };
static const struct gpio_pin pin_pll_ext_sdi = { &gpio_aux, 7 };
static const struct gpio_pin pin_pll_ext_sdo = { &gpio_aux, 8 };
static const struct gpio_pin pin_pll_ext_sclk = { &gpio_aux, 9 };
static const struct gpio_pin pin_pll_ext_reset = { &gpio_aux, 10 };
static const struct gpio_pin pin_pll_ext_lock = { &gpio_aux, 11 };

static const struct gpio_pin pin_main_xo_en_n = { &gpio_aux, 14 };

static const struct gpio_pin pin_ltc6950_sclk = { &gpio_aux, 15 };
static const struct gpio_pin pin_ltc6950_sdi = { &gpio_aux, 16 };
static const struct gpio_pin pin_ltc6950_sdo = { &gpio_aux, 17 };
static const struct gpio_pin pin_ltc6950_ce_gen = { &gpio_aux, 18 };
static const struct gpio_pin pin_ltc6950_ce_distr = { &gpio_aux, 19 };
static const struct gpio_pin pin_ltc6950_sync = { &gpio_aux, 20 };

static const struct gpio_pin pin_ad9910_sdio = { &gpio_aux, 4+21 };
static const struct gpio_pin pin_ad9910_sclk = { &gpio_aux, 5+21 };


struct spi_bus spi_pll_main;
struct spi_bus spi_pll_ext;
struct spi_bus spi_ltc6950;
struct spi_bus spi_ad9910_ref;

struct ad951x_device ad9516_main;
struct ad951x_device ad9516_ext;
struct ltc6950_device ltc6950_pll;
struct ad9910_device dds_ad9910_ref;

static struct ad951x_config pll_main_dot050_config =
#include "ertm_14_pll_main_dot050_config.h"

static struct ltc6950_config pll_ertm15_config =
#include "ertm_15_ltc6950_config.h"


void ertm14_init()
{
    pp_printf("1\n");
    wb_gpio_create( &gpio_aux, BASE_AUXWB );

    pp_printf("2\n");
    gen_gpio_set_dir(&pin_main_xo_en_n, 1);
    gen_gpio_out(&pin_main_xo_en_n, 0);


    pp_printf("3\n");
    bb_spi_create ( &spi_pll_main,
        &pin_pll_main_cs_n,
        &pin_pll_main_sdi,
        &pin_pll_main_sdo,
        &pin_pll_main_sclk,
        AD951X_BIT_DELAY
        );

    pp_printf("4\n");

    bb_spi_create ( &spi_pll_ext,
        &pin_pll_ext_cs_n,
        &pin_pll_ext_sdi,
        &pin_pll_ext_sdo,
        &pin_pll_ext_sclk,
        AD951X_BIT_DELAY
        );

    pp_printf("5\n");

    bb_spi_create( &spi_ltc6950,
        &pin_ltc6950_ce_gen,
        &pin_ltc6950_sdi,
        &pin_ltc6950_sdo,
        &pin_ltc6950_sclk,
        100 );

    pp_printf("6\n");

   bb_spi_create( &spi_ad9910_ref,
        NULL,
        &pin_ad9910_sdio,
        &pin_ad9910_sdio,
        &pin_ad9910_sclk,
        100 );


    pp_printf("7\n");

    ltc6950_pll.bus = &spi_ltc6950;

    pp_printf("LTC6950 RevID: 0x%x (expected 0x%x)\n" ,ltc6950_read( &ltc6950_pll, 0x16 ), 0x65 );

    ad951x_init(&ad9516_main, &spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock );
    ad951x_init(&ad9516_ext, &spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock );

    ad951x_configure(&ad9516_main, &pll_main_dot050_config);
    ltc6950_configure(&ltc6950_pll, &pll_ertm15_config);

    //ad9910_probe( &dds_ad9910_ref, &spi_ad9910_ref );
}




