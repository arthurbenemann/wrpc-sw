#include <stdint.h>
#include <stdio.h>

#include "dev/gpio.h"
#include "dev/spi.h"
#include "dev/ad951x.h"

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

struct spi_bus spi_pll_main;
struct spi_bus spi_pll_ext;
struct ad951x_device ad9516_main;
struct ad951x_device ad9516_ext;

static struct ad951x_config pll_main_dot050_config =
#include "ertm_14_pll_main_dot050_config.h"

void ertm14_init()
{
    wb_gpio_create( &gpio_aux, BASE_AUXWB );

    gen_gpio_set_dir(&pin_main_xo_en_n, 1);
    gen_gpio_out(&pin_main_xo_en_n, 0);

    bb_spi_create ( &spi_pll_main,
        &pin_pll_main_cs_n,
        &pin_pll_main_sdi,
        &pin_pll_main_sdo,
        &pin_pll_main_sclk,
        AD951X_BIT_DELAY
        );

    bb_spi_create ( &spi_pll_ext,
        &pin_pll_ext_cs_n,
        &pin_pll_ext_sdi,
        &pin_pll_ext_sdo,
        &pin_pll_ext_sclk,
        AD951X_BIT_DELAY
        );

    ad951x_init(&ad9516_main, &spi_pll_main, &pin_pll_main_reset, &pin_pll_main_lock );
    ad951x_init(&ad9516_ext, &spi_pll_ext, &pin_pll_ext_reset, &pin_pll_ext_lock );

    ad951x_configure(&ad9516_main, &pll_main_dot050_config);
}




