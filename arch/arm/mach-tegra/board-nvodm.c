/*
 * arch/arm/mach-tegra/board-nvodm.c
 *
 * Converts data from ODM query library into platform data
 *
 * Copyright (c) 2009-2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/machine.h>
#include <linux/lbee9qmb-rfkill.h>
#include <linux/gpio.h>
#include <linux/console.h>
#include <linux/reboot.h>

#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/pinmux.h>
#include <mach/usb-hcd.h>
#include <mach/usb-otg.h>
#include <mach/serial.h>
#include <mach/sdhci.h>
#include <mach/nand.h>
#include <mach/regulator.h>
#include <mach/kbc.h>
#include <mach/i2c.h>
#include <mach/spi.h>
#include <mach/w1.h>

#include <mach/nvrm_linux.h>

#include <asm/mach-types.h>

#include "nvrm_gpio.h"
#include "nvodm_query.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvrm_pinmux.h"
#include "nvrm_module.h"
#include "nvodm_kbc.h"
#include "nvodm_query_kbc.h"
#include "nvodm_kbc_keymapping.h"
#include "gpio-names.h"
#include "power.h"
#include "board.h"
#include "nvrm_pmu.h"
#include "hwrev.h"
#ifdef CONFIG_MACH_MOT
#include "board-mot.h"
#endif

# define BT_RESET 0
# define BT_SHUTDOWN 1

#if defined(CONFIG_KEYBOARD_GPIO)
#include "nvodm_query_gpio.h"
#include <linux/gpio_keys.h>
#include <linux/input.h>
#endif

#ifdef CONFIG_MACH_MOT
#include <linux/mmc/host.h>
#endif

NvRmGpioHandle s_hGpioGlobal;

struct debug_port_data {
	NvOdmDebugConsole port;
	const struct tegra_pingroup_config *pinmux;
	struct clk *clk_data;
	int nr_pins;
};

static u64 tegra_dma_mask = DMA_BIT_MASK(32);

static struct debug_port_data uart_debug_port = {
			.port = NvOdmDebugConsole_UartA,
};

extern const struct tegra_pingroup_config *tegra_pinmux_get(const char *dev_id,
	int config, int *len);


static struct plat_serial8250_port debug_uart_platform[] = {
	{
		.flags = UPF_BOOT_AUTOCONF,
		.iotype = UPIO_MEM,
		.regshift = 2,
	}, {
		.flags = 0,
	}
};
static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform,
	},
};

static void __init tegra_setup_debug_uart(void)
{
	NvOdmDebugConsole uart = NvOdmQueryDebugConsole();
	const struct tegra_pingroup_config *pinmux = NULL;
	const NvU32 *odm_table;
	struct clk *c = NULL;
	NvU32 odm_nr;
	int nr_pins;

	if (uart < NvOdmDebugConsole_UartA ||
	    uart > NvOdmDebugConsole_UartE)
		return;

	NvOdmQueryPinMux(NvOdmIoModule_Uart, &odm_table, &odm_nr);
	if (odm_nr <= (uart - NvOdmDebugConsole_UartA)) {
		pr_err("%s: ODM query configured improperly\n", __func__);
		WARN_ON(1);
		return;
	}

	odm_nr = odm_table[uart - NvOdmDebugConsole_UartA];

	if (uart == NvOdmDebugConsole_UartA) {
		printk(KERN_INFO "pICS_%s: pinmux = tegra_pinmux_get('tegra_uart.0', odm_nr, &nr_pins) = \n",__func__);
		pinmux = tegra_pinmux_get("tegra_uart.0", odm_nr, &nr_pins);
		c = clk_get_sys("uart.0", NULL);
		printk(KERN_INFO "pICS_%s: clk_get_sys('uart.0', NULL)\n",__func__);
		debug_uart_platform[0].membase = IO_ADDRESS(TEGRA_UARTA_BASE);
		debug_uart_platform[0].mapbase = TEGRA_UARTA_BASE;
		debug_uart_platform[0].irq = INT_UARTA;
	} else if (uart == NvOdmDebugConsole_UartB) {
		printk(KERN_INFO "pICS_%s: pinmux = tegra_pinmux_get('tegra_uart.1', odm_nr, &nr_pins)\n",__func__);
		pinmux = tegra_pinmux_get("tegra_uart.1", odm_nr, &nr_pins);
		c = clk_get_sys("uart.1", NULL);
		printk(KERN_INFO "pICS_%s: clk_get_sys('uart.1', NULL)\n",__func__);
		debug_uart_platform[0].membase = IO_ADDRESS(TEGRA_UARTB_BASE);
		debug_uart_platform[0].mapbase = TEGRA_UARTB_BASE;
		debug_uart_platform[0].irq = INT_UARTB;
	} else if (uart == NvOdmDebugConsole_UartC) {
		printk(KERN_INFO "pICS_%s: pinmux = tegra_pinmux_get('tegra_uart.2', odm_nr, &nr_pins)\n",__func__);
		pinmux = tegra_pinmux_get("tegra_uart.2", odm_nr, &nr_pins);
		c = clk_get_sys("uart.2", NULL);
		printk(KERN_INFO "pICS_%s: clk_get_sys('uart.2', NULL)\n",__func__);
		debug_uart_platform[0].membase = IO_ADDRESS(TEGRA_UARTC_BASE);
		debug_uart_platform[0].mapbase = TEGRA_UARTC_BASE;
		debug_uart_platform[0].irq = INT_UARTC;
	} else if (uart == NvOdmDebugConsole_UartD) {
		printk(KERN_INFO "pICS_%s: pinmux = tegra_pinmux_get('tegra_uart.3', odm_nr, &nr_pins)\n",__func__);
		pinmux = tegra_pinmux_get("tegra_uart.3", odm_nr, &nr_pins);
		c = clk_get_sys("uart.3", NULL);
		printk(KERN_INFO "pICS_%s: clk_get_sys('uart.3', NULL)\n",__func__);
		debug_uart_platform[0].membase = IO_ADDRESS(TEGRA_UARTD_BASE);
		debug_uart_platform[0].mapbase = TEGRA_UARTD_BASE;
		debug_uart_platform[0].irq = INT_UARTD;
	} else if (uart == NvOdmDebugConsole_UartE) {
		printk(KERN_INFO "pICS_%s: pinmux = tegra_pinmux_get('tegra_uart.4', odm_nr, &nr_pins)\n",__func__);
		pinmux = tegra_pinmux_get("tegra_uart.4", odm_nr, &nr_pins);
		c = clk_get_sys("uart.4", NULL);
		printk(KERN_INFO "pICS_%s: clk_get_sys('uart.4', NULL)\n",__func__);
		debug_uart_platform[0].membase = IO_ADDRESS(TEGRA_UARTE_BASE);
		debug_uart_platform[0].mapbase = TEGRA_UARTE_BASE;
		debug_uart_platform[0].irq = INT_UARTE;
	}

	if (!c || !pinmux || !nr_pins) {
		if (c)
			clk_put(c);
		return;
	}

	tegra_pinmux_config_tristate_table(pinmux, nr_pins, TEGRA_TRI_NORMAL);
	clk_set_rate(c, 115200*16);
	clk_enable(c);
	debug_uart_platform[0].uartclk = clk_get_rate(c);

	platform_device_register(&debug_uart);

	uart_debug_port.port = uart;
	uart_debug_port.pinmux = pinmux;
	uart_debug_port.nr_pins = nr_pins;
	uart_debug_port.clk_data = c;
}

static void tegra_debug_port_suspend(void)
{
	if (uart_debug_port.port == NvOdmDebugConsole_None)
		return;
	clk_disable(uart_debug_port.clk_data);
	tegra_pinmux_config_tristate_table(uart_debug_port.pinmux,
				uart_debug_port.nr_pins, TEGRA_TRI_TRISTATE);
}

static void tegra_debug_port_resume(void)
{
	if (uart_debug_port.port == NvOdmDebugConsole_None)
		return;
	clk_enable(uart_debug_port.clk_data);
	tegra_pinmux_config_tristate_table(uart_debug_port.pinmux,
				uart_debug_port.nr_pins, TEGRA_TRI_NORMAL);
}

#ifdef CONFIG_MMC_SDHCI_TEGRA
extern struct tegra_nand_platform tegra_nand_plat;

static struct tegra_sdhci_platform_data tegra_sdhci_platform[] = {
	[0] = {
		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,
	},
	[1] = {
		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,
	},
	[2] = {
		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,
	},
	[3] = {
		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,
	},
};
static struct resource tegra_sdhci_resources[][2] = {
	[0] = {
		[0] = {
			.start = TEGRA_SDMMC1_BASE,
			.end = TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC1,
			.end = INT_SDMMC1,
			.flags = IORESOURCE_IRQ,
		},
	},
	[1] = {
		[0] = {
			.start = TEGRA_SDMMC2_BASE,
			.end = TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC2,
			.end = INT_SDMMC2,
			.flags = IORESOURCE_IRQ,
		},
	},
	[2] = {
		[0] = {
			.start = TEGRA_SDMMC3_BASE,
			.end = TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC3,
			.end = INT_SDMMC3,
			.flags = IORESOURCE_IRQ,
		},
	},
	[3] = {
		[0] = {
			.start = TEGRA_SDMMC4_BASE,
			.end = TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC4,
			.end = INT_SDMMC4,
			.flags = IORESOURCE_IRQ,
		},
	},
};
struct platform_device tegra_sdhci_devices[] = {
	[0] = {
		.id = 0,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[0],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[0]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[0],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[1] = {
		.id = 1,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[1],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[1]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[1],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[2] = {
		.id = 2,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[2],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[2]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[2],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[3] = {
		.id = 3,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[3],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[3]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[3],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
};

static const char tegra_sdio_ext_reg_str[] = "vsdio_ext";
int tegra_sdhci_boot_device = -1;

#define active_high(_pin) ((_pin)->activeState == NvOdmGpioPinActiveState_High ? 1 : 0)

static void __init tegra_setup_sdhci(void) {

	int i;

	printk(KERN_INFO "pICS_%s: Starting...",__func__);
	
	tegra_sdhci_platform[0].is_removable = 0;
	tegra_sdhci_platform[0].is_always_on = 1;
	tegra_sdhci_platform[0].gpio_nr_wp = -1;
	tegra_sdhci_platform[0].gpio_nr_cd = -1;
	tegra_sdhci_platform[0].bus_width = 4;
	tegra_sdhci_platform[0].max_clk = 50000000;
	tegra_sdhci_platform[0].pinmux = tegra_pinmux_get("tegra-sdhci.0", 1, &tegra_sdhci_platform[0].nr_pins);

	tegra_sdhci_platform[2].is_removable = 1;
	tegra_sdhci_platform[2].is_always_on = 0;
	tegra_sdhci_platform[2].gpio_nr_wp = -1;
	tegra_sdhci_platform[2].gpio_nr_cd = 69;
	tegra_sdhci_platform[2].gpio_polarity_cd = 0;
	tegra_sdhci_platform[2].bus_width = 4;
	tegra_sdhci_platform[2].max_clk = 50000000;

	/* Olympus P3+, Etna P2+, Etna S3+, Daytona and Sunfire
	   can handle shutting down the external SD card. */
	if ( (HWREV_TYPE_IS_FINAL(system_rev) || (HWREV_TYPE_IS_PORTABLE(system_rev) && (HWREV_REV(system_rev) >= HWREV_REV_3)))) {
		tegra_sdhci_platform[2].regulator_str = (char *)tegra_sdio_ext_reg_str;
	}
	tegra_sdhci_platform[2].pinmux = tegra_pinmux_get("tegra-sdhci.2", 2, &tegra_sdhci_platform[2].nr_pins);

	tegra_sdhci_platform[3].is_removable = 0;
	tegra_sdhci_platform[3].is_always_on = 1;
	tegra_sdhci_platform[3].gpio_nr_wp = -1;
	tegra_sdhci_platform[3].gpio_nr_cd = -1;
	tegra_sdhci_platform[3].bus_width = 8;
	tegra_sdhci_platform[3].max_clk = 50000000;
/*	tegra_sdhci_platform[3].offset = 0x680000; */ /*IMPORTANT this is tegrapart related this one is for d00*/
	tegra_sdhci_platform[3].pinmux = tegra_pinmux_get("tegra-sdhci.3", 2, &tegra_sdhci_platform[3].nr_pins);	

#ifdef CONFIG_EMBEDDED_MMC_START_OFFSET
		/* check if an "MBR" partition was parsed from the tegra partition
		 * command line, and store it in sdhci.3's offset field */
		for (i=0; i<tegra_nand_plat.nr_parts; i++) {
			printk(KERN_INFO "pICS_%s: tegra_nand_plat.parts[%d].name = %s ",__func__, i, tegra_nand_plat.parts[i].name);
			if (strcmp("mbr", tegra_nand_plat.parts[i].name))
				continue;
			tegra_sdhci_platform[3].offset = tegra_nand_plat.parts[i].offset;
			printk(KERN_INFO "pICS_%s: tegra_sdhci_boot_device plat->offset = 0x%llx ",__func__, tegra_nand_plat.parts[i].offset);
		}
#endif


	platform_device_register(&tegra_sdhci_devices[3]);
	platform_device_register(&tegra_sdhci_devices[0]);
	platform_device_register(&tegra_sdhci_devices[2]);

	printk(KERN_INFO "pICS_%s: Ending...",__func__);
}
#else
static void __init tegra_setup_sdhci(void) { }
#endif

#ifdef CONFIG_SERIAL_TEGRA
struct tegra_serial_platform_data tegra_uart_platform[] = {
	{
		.p = {
			.membase = IO_ADDRESS(TEGRA_UARTA_BASE),
			.mapbase = TEGRA_UARTA_BASE,
			.irq = INT_UARTA,
		},
		.use_pio = false,
	},
	{
		.p = {
			.membase = IO_ADDRESS(TEGRA_UARTB_BASE),
			.mapbase = TEGRA_UARTB_BASE,
			.irq = INT_UARTB,
		},
		.use_pio = false,
	},
	{
		.p = {
			.membase = IO_ADDRESS(TEGRA_UARTC_BASE),
			.mapbase = TEGRA_UARTC_BASE,
			.irq = INT_UARTC,
		},
		.use_pio = false,
	},
	{
		.p = {
			.membase = IO_ADDRESS(TEGRA_UARTD_BASE),
			.mapbase = TEGRA_UARTD_BASE,
			.irq = INT_UARTD,
		},
		.use_pio = false,
	},
	{
		.p = {
			.membase = IO_ADDRESS(TEGRA_UARTE_BASE),
			.mapbase = TEGRA_UARTE_BASE,
			.irq = INT_UARTE,
		},
		.use_pio = false,
	},
};
static struct platform_device tegra_uart[] = {
	{
		.name = "tegra_uart",
		.id = 0,
		.dev = {
			.platform_data = &tegra_uart_platform[0],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	{
		.name = "tegra_uart",
		.id = 1,
		.dev = {
			.platform_data = &tegra_uart_platform[1],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	{
		.name = "tegra_uart",
		.id = 2,
		.dev = {
			.platform_data = &tegra_uart_platform[2],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	{
		.name = "tegra_uart",
		.id = 3,
		.dev = {
			.platform_data = &tegra_uart_platform[3],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	{
		.name = "tegra_uart",
		.id = 4,
		.dev = {
			.platform_data = &tegra_uart_platform[4],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},

};
static void __init tegra_setup_hsuart(void)
{
	printk(KERN_INFO "pICS_%s: Starting...",__func__);

	/*NvOdmDebugConsole uart = NvOdmQueryDebugConsole();
	int dbg_id = (int)uart - (int)NvOdmDebugConsole_UartA;
	const NvU32 *odm_table;
	NvU32 odm_nr;
	int i;

	NvOdmQueryPinMux(NvOdmIoModule_Uart, &odm_table, &odm_nr);

	for (i=0; i<ARRAY_SIZE(tegra_uart); i++) {
		struct tegra_serial_platform_data *plat;
		char name[16];

		if (i==dbg_id)
			continue;

		plat = &tegra_uart_platform[i];

		printk(KERN_INFO "pICS_%s: tegra_uart[%d]",__func__, i);
		snprintf(name, sizeof(name), "%s.%d",
			 tegra_uart[i].name, tegra_uart[i].id);

		if (i < odm_nr) {
			plat->pinmux = tegra_pinmux_get(name,
				odm_table[i], &plat->nr_pins);
			printk(KERN_INFO "pICS_%s: tegra_uart[%d] pinmux = ^^^",__func__, i);
			
		} else {
			printk(KERN_INFO "pICS_%s: tegra_uart[%d] pinmux = NULL, nr_pins = 0",__func__, i);
			plat->pinmux = NULL;
			plat->nr_pins = 0;
		}

		if (platform_device_register(&tegra_uart[i])) {
			pr_err("%s: failed to register %s.%d\n",
			       __func__, tegra_uart[i].name, tegra_uart[i].id);
		}
	}*/
	tegra_uart_platform[0].pinmux = tegra_pinmux_get("tegra_uart.0", 4, &tegra_uart_platform[0].nr_pins);
	/* set if no debug on UARTB */
	/* tegra_uart_platform[1].pinmux = tegra_pinmux_get("tegra_uart.1", TO_FILL, &tegra_uart_platform[1].nr_pins);*/
	tegra_uart_platform[2].pinmux = tegra_pinmux_get("tegra_uart.2", 1, &tegra_uart_platform[2].nr_pins);
	tegra_uart_platform[3].pinmux = tegra_pinmux_get("tegra_uart.3", 2, &tegra_uart_platform[3].nr_pins);
	tegra_uart_platform[4].pinmux = tegra_pinmux_get("tegra_uart.4", 4, &tegra_uart_platform[3].nr_pins);

	if (platform_device_register(&tegra_uart[0])) 
			pr_err("%s: failed to register %s.%d\n",
			       __func__, tegra_uart[0].name, tegra_uart[0].id);
	if (platform_device_register(&tegra_uart[2])) 
			pr_err("%s: failed to register %s.%d\n",
			       __func__, tegra_uart[2].name, tegra_uart[2].id);
	if (platform_device_register(&tegra_uart[3])) 
			pr_err("%s: failed to register %s.%d\n",
			       __func__, tegra_uart[3].name, tegra_uart[3].id);
	if (platform_device_register(&tegra_uart[4])) 
			pr_err("%s: failed to register %s.%d\n",
			       __func__, tegra_uart[4].name, tegra_uart[4].id);

	printk(KERN_INFO "pICS_%s: Ending...",__func__);

}
#else
static void __init tegra_setup_hsuart(void) { }
#endif

#ifdef CONFIG_USB_TEGRA_HCD
static struct tegra_hcd_platform_data tegra_hcd_platform[] = {
	[0] = {
		.instance = 0,
	},
	[1] = {
		.instance = 1,
	},
	[2] = {
		.instance = 2,
	},
};
static struct resource tegra_hcd_resources[][2] = {
	[0] = {
		[0] = {
			.flags = IORESOURCE_MEM,
			.start = TEGRA_USB_BASE,
			.end = TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		},
		[1] = {
			.flags = IORESOURCE_IRQ,
			.start = INT_USB,
			.end = INT_USB,
		},
	},
	[1] = {
		[0] = {
			.flags = IORESOURCE_MEM,
			.start = TEGRA_USB1_BASE,
			.end = TEGRA_USB1_BASE + TEGRA_USB1_SIZE - 1,
		},
		[1] = {
			.flags = IORESOURCE_IRQ,
			.start = INT_USB2,
			.end = INT_USB2,
		},
	},
	[2] = {
		[0] = {
			.flags = IORESOURCE_MEM,
			.start = TEGRA_USB2_BASE,
			.end = TEGRA_USB2_BASE + TEGRA_USB2_SIZE - 1,
		},
		[1] = {
			.flags = IORESOURCE_IRQ,
			.start = INT_USB3,
			.end = INT_USB3,
		},
	},
};
/* EHCI transfers must be 32B aligned */
static u64 tegra_ehci_dma_mask = DMA_BIT_MASK(32) & ~0x1f;
static struct platform_device tegra_hcd[] = {
	[0] = {
		.name = "tegra-ehci",
		.id = 0,
		.dev = {
			.platform_data = &tegra_hcd_platform[0],
			.coherent_dma_mask = DMA_BIT_MASK(32) & ~0x1f,
			.dma_mask = &tegra_ehci_dma_mask,
		},
		.resource = tegra_hcd_resources[0],
		.num_resources = ARRAY_SIZE(tegra_hcd_resources[0]),
	},
	[1] = {
		.name = "tegra-ehci",
		.id = 1,
		.dev = {
			.platform_data = &tegra_hcd_platform[1],
			.coherent_dma_mask = DMA_BIT_MASK(32) & ~0x1f,
			.dma_mask = &tegra_ehci_dma_mask,
		},
		.resource = tegra_hcd_resources[1],
		.num_resources = ARRAY_SIZE(tegra_hcd_resources[1]),
	},
	[2] = {
		.name = "tegra-ehci",
		.id = 2,
		.dev = {
			.platform_data = &tegra_hcd_platform[2],
			.coherent_dma_mask = DMA_BIT_MASK(32) & ~0x1f,
			.dma_mask = &tegra_ehci_dma_mask,
		},
		.resource = tegra_hcd_resources[2],
		.num_resources = ARRAY_SIZE(tegra_hcd_resources[2]),
	},
};

#ifdef CONFIG_USB_TEGRA_OTG
#define otg_is_okay(_instance) ((_instance)==0)
static struct tegra_otg_platform_data tegra_otg_platform = {
	.instance = 0,
};
static struct resource tegra_otg_resources[] = {
	[0] = {
		.start = TEGRA_USB_BASE,
		.end = TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_USB,
		.end = INT_USB,
		.flags = IORESOURCE_IRQ,
	},
};
static struct platform_device tegra_otg = {
	.name = "tegra-otg",
	.id = 0,
	.resource = tegra_otg_resources,
	.num_resources = ARRAY_SIZE(tegra_otg_resources),
	.dev = {
		.platform_data = &tegra_otg_platform,
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.dma_mask = &tegra_dma_mask,
	},
};
#else
#define otg_is_okay(_instance) (0)
#endif

static void __init tegra_setup_hcd(void)
{
		
		
#if 0
	int i;
 
	for (i=0; i<ARRAY_SIZE(tegra_hcd_platform); i++) {
		const NvOdmUsbProperty *p;
		struct tegra_hcd_platform_data *plat = &tegra_hcd_platform[i];
		printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] NvOdmQueryGetUsbProperty",__func__, i);
		

		if ((p->UsbMode == NvOdmUsbModeType_Device) ||
		    (p->UsbMode == NvOdmUsbModeType_None))
			continue;

		plat->otg_mode = (p->UsbMode == NvOdmUsbModeType_OTG);
		printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] otg_mode  = %d",__func__, i, plat->otg_mode);
		if (plat->otg_mode && !otg_is_okay(i)) {
			pr_err("%s: OTG not enabled in kernel for USB "
			       "controller %d, but ODM kit specifes OTG\n",
			       __func__, i);
			continue;
		}
#ifdef CONFIG_USB_TEGRA_OTG
		if (plat->otg_mode && otg_is_okay(i)) {
			tegra_otg_platform.usb_property = p;
			platform_device_register(&tegra_otg);
		}
#endif
		if (p->IdPinDetectionType == NvOdmUsbIdPinType_Gpio) {
			const NvOdmGpioPinInfo *gpio;
			NvU32 count;

			gpio = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_Usb,
						    i, &count);
			if (!gpio || (count<=NvOdmGpioPin_UsbCableId)) {
				pr_err("%s: invalid ODM query for controller "
				       "%d\n", __func__, i);
				WARN_ON(1);
				continue;
			}
			plat->id_detect = ID_PIN_GPIO;
			printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] id_detect = %u",__func__, i, plat->id_detect);
			gpio += NvOdmGpioPin_UsbCableId;
			plat->gpio_nr = gpio->Port*8 + gpio->Pin;
			printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] gpio_nr = %u",__func__, i, plat->gpio_nr);
		} else if (p->IdPinDetectionType == NvOdmUsbIdPinType_CableId) {
			plat->id_detect = ID_PIN_CABLE_ID;
			printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] id_detect = %u",__func__, i, plat->id_detect);
		}

		/* Some connected devices such as Wrigley expect VBUS to stay
		 * on even when the AP20 is in LP0. */
		if (p->vbus_regulator != NULL) {
			plat->regulator_str = p->vbus_regulator;
			printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] regulator_str = %s",__func__, i, plat->regulator_str);
		}
		/* Enable fast wakeup by default to use resume signaling on
		 * wakeup from LP0 instead of a bus reset.
		 */
		if (p->UsbMode == NvOdmUsbModeType_Host) {
			plat->fast_wakeup = 1;
			printk(KERN_INFO "pICS_%s: tegra_hcd_platform[%d] fast_wakeup = %d",__func__, i, plat->fast_wakeup);
		}
		platform_device_register(&tegra_hcd[i]);
	}
#endif

/*	const NvOdmUsbProperty *p;
	p = NvOdmQueryGetUsbProperty(NvOdmIoModule_Usb, i);*/

	

	static const NvOdmUsbProperty Usb1Property =
   	{
    /* Specifies the USB controller is connected to a standard UTMI interface
     (only valid for ::NvOdmIoModule_Usb).
     NvOdmUsbInterfaceType_Utmi = 1,*/
		1,
		/* /// Specifies charger type 0, USB compliant charger, when D+ and D- are at low voltage.
    		NvOdmUsbChargerType_SE0 = 1,
		/// Specifies charger type 2, when D+ is low and D- is high.
    		NvOdmUsbChargerType_SK = 4,
    		/// Specifies charger type 3, when D+ and D- are at high voltage.
    		NvOdmUsbChargerType_SE1 = 8,

	        (NvOdmUsbChargerType_SE0 | NvOdmUsbChargerType_SE1 | NvOdmUsbChargerType_SK),*/
		(1 | 8 | 4),
	        20,
        	0, /*NV_FALSE,*/
		/// Specifies the instance as USB OTG.
    		4, /* NvOdmUsbModeType_OTG */
	        0, /*NvOdmUsbIdPinType_None,*/
	        0, /*NvOdmUsbConnectorsMuxType_None,*/
        	0 /*NV_FALSE*/
   	};

	printk(KERN_INFO "pICS_%s: Starting...",__func__);

	tegra_otg_platform.usb_property = &Usb1Property;
	platform_device_register(&tegra_otg);

	tegra_hcd_platform[0].otg_mode = 1;

	tegra_hcd_platform[2].otg_mode = 0;
	tegra_hcd_platform[2].fast_wakeup = 1;

	platform_device_register(&tegra_hcd[0]);
	platform_device_register(&tegra_hcd[2]);

	printk(KERN_INFO "pICS_%s: Ending...",__func__);

}
#else
static inline void tegra_setup_hcd(void) { }
#endif

#ifdef CONFIG_KEYBOARD_TEGRA
struct tegra_kbc_plat tegra_kbc_platform;

static noinline void __init tegra_setup_kbc(void)
{

	struct tegra_kbc_plat *pdata = &tegra_kbc_platform;
	const NvOdmPeripheralConnectivity *conn;
	NvOdmPeripheralSearch srch_attr = NvOdmPeripheralSearch_IoModule;
	const struct NvOdmKeyVirtTableDetail **vkeys;
	NvU32 srch_val = NvOdmIoModule_Kbd;
	NvU32 temp;
	NvU64 guid;
	NvU32 i, j, k;
	NvU32 cols=0;
	NvU32 rows=0;
	NvU32 *wake_row;
	NvU32 *wake_col;
	NvU32 wake_num;
	NvU32 vnum;

	pdata->keymap = kzalloc(sizeof(*pdata->keymap)*KBC_MAX_KEY, GFP_KERNEL);
	printk(KERN_INFO "pICS_%s: pdata->keymap pointer to memory where key mapping is stored",__func__);
	if (!pdata->keymap) {
		pr_err("%s: out of memory for key mapping\n", __func__);
		return;
	}
	pdata->wake_cnt = 0; /* 0:wake on any key >1:wake on wake_cfg */
	printk(KERN_INFO "pICS_%s: pdata->wake_cnt = %d",__func__, pdata->wake_cnt);
	if (NvOdmKbcIsSelectKeysWkUpEnabled(&wake_row, &wake_col, &wake_num)) {
		for (i=0;i<wake_num;i++) printk(KERN_INFO "pICS_%s: wake_row[%i] = %i",__func__, i, wake_row[i]);
		for (i=0;i<wake_num;i++) printk(KERN_INFO "pICS_%s: wake_col[%i] = %i",__func__, i, wake_col[i]);
		printk(KERN_INFO "pICS_%s: wake_num = %d",__func__, wake_num);
		BUG_ON(wake_num >= KBC_MAX_KEY);
		if (wake_num) {
		pdata->wake_cfg = kzalloc(sizeof(*pdata->wake_cfg)*wake_num,
			GFP_KERNEL);
		printk(KERN_INFO "pICS_%s: pdata->wake_cfg pointer to memory where key wake config is stored",__func__);
		if (pdata->wake_cfg) {
			pdata->wake_cnt = (int)wake_num;
			printk(KERN_INFO "pICS_%s: pdata->wake_cnt = %d",__func__, pdata->wake_cnt);
			for (i=0; i<wake_num; i++) {
				pdata->wake_cfg[i].row=wake_row[i];
				printk(KERN_INFO "pICS_%s: pdata->wake_cfg[%d].row = %d",__func__, i, wake_row[i]);
				pdata->wake_cfg[i].col=wake_col[i];
				printk(KERN_INFO "pICS_%s: pdata->wake_cfg[%d].col = %d",__func__, i, wake_col[i]);
			}
		} else
			pr_err("disabling wakeup key filtering due to "
				"out-of-memory error\n");
		} else
			pr_warning("no wakeup keys are configured \n");

	}

	NvOdmKbcGetParameter(NvOdmKbcParameter_DebounceTime, 1, &temp);

	/* debounce time is reported from ODM in terms of clock ticks. */
	pdata->debounce_cnt = temp;
	printk(KERN_INFO "pICS_%s: pdata->debounce_cnt = %u",__func__, pdata->debounce_cnt);

	/* repeat cycle is reported from ODM in milliseconds,
	 * but needs to be specified in 32KHz ticks */
	NvOdmKbcGetParameter(NvOdmKbcParameter_RepeatCycleTime, 1, &temp);
	pdata->repeat_cnt = temp * 32;
	printk(KERN_INFO "pICS_%s: pdata->repeat_cnt = %u",__func__, pdata->repeat_cnt);	

	temp = NvOdmPeripheralEnumerate(&srch_attr, &srch_val, 1, &guid, 1);
	if (!temp) {
		kfree(pdata->keymap);
		pr_err("%s: failed to find keyboard module\n", __func__);
		return;
	}
	conn = NvOdmPeripheralGetGuid(guid);
	if (!conn) {
		kfree(pdata->keymap);
		pr_err("%s: failed to find keyboard\n", __func__);
		return;
	}

	for (i=0; i<conn->NumAddress; i++) {
		NvU32 addr = conn->AddressList[i].Address;

		if (conn->AddressList[i].Interface!=NvOdmIoModule_Kbd) continue;

		if (conn->AddressList[i].Instance) {
			pdata->pin_cfg[addr].num = cols++;
			printk(KERN_INFO "pICS_%s: pdata->pin_cfg[%d].num = %u",__func__, addr, pdata->pin_cfg[addr].num);
			pdata->pin_cfg[addr].is_col = true;
			printk(KERN_INFO "pICS_%s: pdata->pin_cfg[%d].is_col = true",__func__, addr);
		} else {
			pdata->pin_cfg[addr].num = rows++;
			printk(KERN_INFO "pICS_%s: pdata->pin_cfg[%d].num = %u",__func__, addr, pdata->pin_cfg[addr].num);
			pdata->pin_cfg[addr].is_row = true;
			printk(KERN_INFO "pICS_%s: pdata->pin_cfg[%d].is_row = true",__func__, addr);
		}
	}

	for (i=0; i<KBC_MAX_KEY; i++) {
		pdata->keymap[i] = -1;
		printk(KERN_INFO "pICS_%s: pdata->keymap[%d] = -1",__func__, i);
	}
	vnum = NvOdmKbcKeyMappingGetVirtualKeyMappingList(&vkeys);
	printk(KERN_INFO "pICS_%s: vnum = %d",__func__, vnum);
	for (i=0; i<rows; i++) {
		for (j=0; j<cols; j++) {
			NvU32 sc = NvOdmKbcGetKeyCode(i, j, rows, cols);
			printk(KERN_INFO "pICS_%s: sc = %lu",__func__, sc);
			for (k=0; k<vnum; k++) {
				if (sc >= vkeys[k]->StartScanCode &&
				    sc <= vkeys[k]->EndScanCode) {
					sc -= vkeys[k]->StartScanCode;
					sc = vkeys[k]->pVirtualKeyTable[sc];
					if (!sc) continue;
					pdata->keymap[kbc_indexof(i,j)]=sc;
					printk(KERN_INFO "pICS_%s: pdata->keymap[kbc_indexof(%d,%d)=%d] = %lu",__func__, i, j, kbc_indexof(i,j), sc);
				}

                        }
		}
	}
#if 0
	struct wake_row = {0,1,1,2,2};
	struct wake_col = {0,0,1,0,1};
	struct tegra_kbc_plat *pdata = &tegra_kbc_platform;
	const NvOdmPeripheralConnectivity *conn;
	NvOdmPeripheralSearch srch_attr = NvOdmPeripheralSearch_IoModule;
	const struct NvOdmKeyVirtTableDetail **vkeys;
	NvU32 srch_val = NvOdmIoModule_Kbd;
	NvU32 temp;
	NvU64 guid;
	NvU32 i, j, k;
	NvU32 cols=0;
	NvU32 rows=0;
	NvU32 *wake_row;
	NvU32 *wake_col;
	NvU32 wake_num;
	NvU32 vnum;

	pdata->keymap = kzalloc(sizeof(*pdata->keymap)*KBC_MAX_KEY, GFP_KERNEL);
	if (!pdata->keymap) {
		pr_err("%s: out of memory for key mapping\n", __func__);
		return;
	}
	pdata->wake_cnt = 5; /* 0:wake on any key >1:wake on wake_cfg */
	pdata->wake_cfg = kzalloc(sizeof(*pdata->wake_cfg)*pdata->wake_cnt,
			GFP_KERNEL);
	if (pdata->wake_cfg) {
			for (i=0; i<wake_num; i++) {
				pdata->wake_cfg[i].row=wake_row[i];
				printk(KERN_INFO "pICS_%s: pdata->wake_cfg[%d].row = %d",__func__, i, wake_row[i]);
				pdata->wake_cfg[i].col=wake_col[i];
				printk(KERN_INFO "pICS_%s: pdata->wake_cfg[%d].col = %d",__func__, i, wake_col[i]);
			}
		} else
			pr_err("disabling wakeup key filtering due to "
				"out-of-memory error\n");
		} else
			pr_warning("no wakeup keys are configured \n");

	}

	/* debounce time is reported from ODM in terms of clock ticks. */
	pdata->debounce_cnt = 10;

	/* repeat cycle is reported from ODM in milliseconds,
	 * but needs to be specified in 32KHz ticks */
	pdata->repeat_cnt = 1024;

	pdata->pin_cfg[0].num = 0;
	pdata->pin_cfg[0].is_row = true;
	pdata->pin_cfg[1].num = 1;
	pdata->pin_cfg[1].is_row = true;
	pdata->pin_cfg[2].num = 2;
	pdata->pin_cfg[2].is_row = true;
	pdata->pin_cfg[16].num = 0;
	pdata->pin_cfg[16].is_col = true;
	pdata->pin_cfg[17].num = 1;
	pdata->pin_cfg[17].is_col = true;
	pdata->pin_cfg[18].num = 2;
	pdata->pin_cfg[18].is_col = true;

	for (i=0; i<KBC_MAX_KEY; i++) {
		pdata->keymap[i] = -1;
	}

	pdata->keymap[??]=115;
	pdata->keymap[??]=211;
	pdata->keymap[??]=139;
	pdata->keymap[??]=114;
	pdata->keymap[??]=212;
	pdata->keymap[??]=102;
	pdata->keymap[??]=152;
	pdata->keymap[??]=217;
	pdata->keymap[??]=158;
#endif
}
#else
static void tegra_setup_kbc(void) { }
#endif

static void tegra_setup_gpio_key(void) { }

#ifdef CONFIG_LBEE9QMB_RFKILL
static struct lbee9qmb_platform_data lbee9qmb_platform;
static struct platform_device lbee9qmb_device = {
	.name = "lbee9qmb-rfkill",
	.dev = {
		.platform_data = &lbee9qmb_platform,
	},
};
static noinline void __init tegra_setup_rfkill(void)
{
	const NvOdmPeripheralConnectivity *con;
	unsigned int i;
	lbee9qmb_platform.delay=5;
	lbee9qmb_platform.gpio_pwr=-1;
	if ((con = NvOdmPeripheralGetGuid(NV_ODM_GUID('l','b','e','e','9','q','m','b'))))
	{
		printk(KERN_INFO "pICS_%s: lbee9qmb",__func__);
		for (i=0; i<con->NumAddress; i++) {
			if (con->AddressList[i].Interface == NvOdmIoModule_Gpio
					&& con->AddressList[i].Purpose == BT_RESET ){
				int nr_gpio = con->AddressList[i].Instance * 8 +
					con->AddressList[i].Address;
				lbee9qmb_platform.gpio_reset = nr_gpio;
				printk(KERN_INFO "pICS_%s: lbee9qmb_platform.gpio_reset = %d",__func__, lbee9qmb_platform.gpio_reset);
				if (platform_device_register(&lbee9qmb_device))
					pr_err("%s: registration failed\n", __func__);
				return;
			}
		}
	}
	else if ((con = NvOdmPeripheralGetGuid(NV_ODM_GUID('b','c','m','_','4','3','2','9'))))
	{
		int nr_gpio;
		printk(KERN_INFO "pICS_%s: bcm_4329",__func__);
		for (i=0; i<con->NumAddress; i++) {
                        if (con->AddressList[i].Interface == NvOdmIoModule_Gpio
						&& con->AddressList[i].Purpose == BT_RESET){
					nr_gpio = con->AddressList[i].Instance * 8 +
						con->AddressList[i].Address;
					lbee9qmb_platform.gpio_reset = nr_gpio;
					printk(KERN_INFO "pICS_%s: bcm_4329_platform.gpio_reset = %d",__func__, lbee9qmb_platform.gpio_reset);
				}
			else if (con->AddressList[i].Interface == NvOdmIoModule_Gpio
						&& con->AddressList[i].Purpose == BT_SHUTDOWN ){
					nr_gpio = con->AddressList[i].Instance * 8 +
						 con->AddressList[i].Address;
					lbee9qmb_platform.gpio_pwr = nr_gpio;
					printk(KERN_INFO "pICS_%s: bcm_4329_platform.gpio_pwr = %d",__func__, lbee9qmb_platform.gpio_pwr);
				}
		}
		lbee9qmb_platform.delay=200;
                if (platform_device_register(&lbee9qmb_device))
			pr_err("%s: registration failed\n", __func__);
                return;
        }
        return;
}
#else
static void tegra_setup_rfkill(void) { }
#endif


static struct platform_device *nvodm_devices[] __initdata = {

};

#ifdef CONFIG_SPI_TEGRA
static struct tegra_spi_platform_data tegra_spi_platform[] = {
	[0] = {
		.is_slink = true,
	},
	[1] = {
		.is_slink = true,
	},
	[2] = {
		.is_slink = true,
	},
	[3] = {
		.is_slink = true,
	},
	[4] = {
		.is_slink = false,
	},
};
static struct platform_device tegra_spi_devices[] = {
	[0] = {
		.name = "tegra_spi_slave",
		.id = 0,
		.dev = {
			.platform_data = &tegra_spi_platform[0],
		},
	},
	[1] = {
		.name = "tegra_spi",
		.id = 1,
		.dev = {
			.platform_data = &tegra_spi_platform[1],
		},
	},
	[2] = {
		.name = "tegra_spi",
		.id = 2,
		.dev = {
			.platform_data = &tegra_spi_platform[2],
		},
	},
	[3] = {
		.name = "tegra_spi",
		.id = 3,
		.dev = {
			.platform_data = &tegra_spi_platform[3],
		},
	},
	[4] = {
		.name = "tegra_spi",
		.id = 4,
		.dev = {
			.platform_data = &tegra_spi_platform[4],
		},
	},
};
static noinline void __init tegra_setup_spi(void)
{

	int rc;

	printk(KERN_INFO "pICS_%s: Starting...",__func__);
#if 0
	const NvU32 *spi_mux;
	const NvU32 *sflash_mux;
	NvU32 spi_mux_nr;
	NvU32 sflash_mux_nr;
	int i;

	NvOdmQueryPinMux(NvOdmIoModule_Spi, &spi_mux, &spi_mux_nr);
	NvOdmQueryPinMux(NvOdmIoModule_Sflash, &sflash_mux, &sflash_mux_nr);

	for (i=0; i<ARRAY_SIZE(tegra_spi_devices); i++) {
		struct platform_device *pdev = &tegra_spi_devices[i];
		struct tegra_spi_platform_data *plat = &tegra_spi_platform[i];

		const NvOdmQuerySpiDeviceInfo *info = NULL;
		NvU32 mux = 0;
		

		if (plat->is_slink && pdev->id<spi_mux_nr)
			mux = spi_mux[pdev->id];
		else if (sflash_mux_nr && !plat->is_slink)
			mux = sflash_mux[0];

		printk(KERN_INFO "pICS_%s: mux = %lu",__func__, mux);

		if (!mux)
			continue;

		if (mux == NVODM_QUERY_PINMAP_MULTIPLEXED) {
			pr_err("%s: not registering multiplexed SPI master "
			       "%s.%d\n", __func__, pdev->name, pdev->id);
			WARN_ON(1);
			continue;
		}

		if (plat->is_slink) {
			info = NvOdmQuerySpiGetDeviceInfo(NvOdmIoModule_Spi,
							  pdev->id, 0);
		} else {
			info = NvOdmQuerySpiGetDeviceInfo(NvOdmIoModule_Sflash,
							  0, 0);
		}

		if (info && info->IsSlave) {
			pr_info("%s: registering SPI slave %s.%d\n",
				__func__, pdev->name, pdev->id);
			//continue;
		}
#endif
		
		printk(KERN_INFO "pICS_%s: registering SPI slave %s.%d\n",__func__, tegra_spi_devices[0].name, tegra_spi_devices[0].id);
		rc = platform_device_register(&tegra_spi_devices[0]);
		if (rc) {
			pr_err("%s: registration of %s.%d failed\n",
			       __func__, tegra_spi_devices[0].name, tegra_spi_devices[0].id);
		}
	/*}*/

	printk(KERN_INFO "pICS_%s: Ending...",__func__);
}
#else
static void tegra_setup_spi(void) { }
#endif

#ifdef CONFIG_I2C_TEGRA
static struct tegra_i2c_plat_parms tegra_i2c_platform[] = {
	[0] = {
		.adapter_nr = 0,
		.bus_count = 1,
		.bus_mux = { 0, 0 },
		.bus_clk = { 100000, 0 }, /* default to 100KHz */
		.is_dvc = false,
	},
	[1] = {
		.adapter_nr = 1,
		.bus_count = 1,
		.bus_mux = { 0, 0 },
		.bus_clk = { 100000, 0 },
		.is_dvc = false,
	},
	[2] = {
		.adapter_nr = 2,
		.bus_count = 1,
		.bus_mux = { 0, 0 },
		.bus_clk = { 100000, 0 },
		.is_dvc = false,
	},
	[3] = {
		.adapter_nr = 3,
		.bus_count = 1,
		.bus_mux = { 0, 0 },
		.bus_clk = { 100000, 0 },
		.is_dvc = true,
	},
};
static struct platform_device tegra_i2c_devices[] = {
	[0] = {
		.name = "tegra_i2c",
		.id = 0,
		.dev = {
			.platform_data = &tegra_i2c_platform[0],
		},
	},
	[1] = {
		.name = "tegra_i2c",
		.id = 1,
		.dev = {
			.platform_data = &tegra_i2c_platform[1],
		},
	},
	[2] = {
		.name = "tegra_i2c",
		.id = 2,
		.dev = {
			.platform_data = &tegra_i2c_platform[2],
		},
	},
	[3] = {
		.name = "tegra_i2c",
		.id = 3,
		.dev = {
			.platform_data = &tegra_i2c_platform[3],
		},
	},
};
static noinline void __init tegra_setup_i2c(void)
{
	const NvOdmPeripheralConnectivity *smbus;
	const NvOdmIoAddress *smbus_addr = NULL;
	const NvU32 *odm_mux_i2c = NULL;
	const NvU32 *odm_clk_i2c = NULL;
	const NvU32 *odm_mux_i2cp = NULL;
	const NvU32 *odm_clk_i2cp = NULL;
	NvU32 odm_mux_i2c_nr;
	NvU32 odm_clk_i2c_nr;
	NvU32 odm_mux_i2cp_nr;
	NvU32 odm_clk_i2cp_nr;
	int i;

	smbus = NvOdmPeripheralGetGuid(NV_ODM_GUID('I','2','c','S','m','B','u','s'));

	if (smbus) {
		unsigned int j;
		smbus_addr = smbus->AddressList;
		for (j=0; j<smbus->NumAddress; j++, smbus_addr++) {
			if ((smbus_addr->Interface == NvOdmIoModule_I2c) ||
			    (smbus_addr->Interface == NvOdmIoModule_I2c_Pmu))
				break;
		}
		if (j==smbus->NumAddress)
			smbus_addr = NULL;
	}

	NvOdmQueryPinMux(NvOdmIoModule_I2c, &odm_mux_i2c, &odm_mux_i2c_nr);
	NvOdmQueryPinMux(NvOdmIoModule_I2c_Pmu, &odm_mux_i2cp, &odm_mux_i2cp_nr);
	NvOdmQueryClockLimits(NvOdmIoModule_I2c, &odm_clk_i2c, &odm_clk_i2c_nr);
	NvOdmQueryClockLimits(NvOdmIoModule_I2c_Pmu, &odm_clk_i2cp, &odm_clk_i2cp_nr);

	for (i=0; i<ARRAY_SIZE(tegra_i2c_devices); i++) {

		struct platform_device *dev = &tegra_i2c_devices[i];
		struct tegra_i2c_plat_parms *plat = &tegra_i2c_platform[i];
		NvU32 mux, clk;

		if (smbus_addr) {
			if (smbus_addr->Interface == NvOdmIoModule_I2c &&
			    smbus_addr->Instance == dev->id && !plat->is_dvc) {
				pr_info("%s: skipping %s.%d (SMBUS)\n",
					__func__, dev->name, dev->id);
				continue;
			}
		}
		printk(KERN_INFO "pICS_%s: tegra_i2c_devices[%d].name = %s.%d",__func__, i, dev->name, dev->id);
		if (plat->is_dvc) {
			printk(KERN_INFO "pICS_%s: if (plat->is_dvc)", __func__);
			mux = (odm_mux_i2cp_nr) ? odm_mux_i2cp[0] : 0;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], mux = %lu",__func__, i, mux);
			clk = (odm_clk_i2cp_nr) ? odm_clk_i2cp[0] : 100;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], clk = %lu",__func__, i, clk);
		} else if (dev->id < odm_mux_i2c_nr) {
			printk(KERN_INFO "pICS_%s: if (dev->id < odm_mux_i2c_nr)", __func__);
			mux = odm_mux_i2c[dev->id];
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], mux = %lu",__func__, i, mux);
			clk = (dev->id < odm_clk_i2c_nr) ? odm_clk_i2c[dev->id] : 100;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], clk = %lu",__func__, i, clk);
		} else {
			mux = 0;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], mux = %lu",__func__, i, mux);
			clk = 0;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d], clk = %lu",__func__, i, clk);
		}

		if (!mux)
			continue;

		if (mux == NVODM_QUERY_PINMAP_MULTIPLEXED) {
			pr_err("%s: unable to register %s.%d (multiplexed)\n",
			       __func__, dev->name, dev->id);
			WARN_ON(1);
			continue;
		}

#warning Hardcoding i2c bus speeds to 400Khz due to nvidia bug 705733
		if (clk)
			plat->bus_clk[0] = 400*1000;
			printk(KERN_INFO "pICS_%s: tegra_i2c_platform[%d].bus_clk[0] = %lu",__func__, i, plat->bus_clk[0]);

		if (platform_device_register(dev))
			pr_err("%s: failed to register %s.%d\n",
			       __func__, dev->name, dev->id);
	}
}
#else
static void tegra_setup_i2c(void) { }
#endif

#ifdef CONFIG_W1_MASTER_TEGRA
static struct tegra_w1_platform_data tegra_w1_platform;
static struct platform_device tegra_w1_device = {
	.name = "tegra_w1",
	.id = 0,
	.dev = {
		.platform_data = &tegra_w1_platform,
	},
};
static noinline void __init tegra_setup_w1(void)
{
	const NvU32 *pinmux;
	NvU32 nr_pinmux;

	NvOdmQueryPinMux(NvOdmIoModule_OneWire, &pinmux, &nr_pinmux);
	if (!nr_pinmux || !pinmux[0]) {
		pr_info("%s: no one-wire device\n", __func__);
		return;
	}
	tegra_w1_platform.pinmux = pinmux[0];
	printk(KERN_INFO "pICS_%s: tegra_w1_platform.pinmux = %lu",__func__, pinmux[0]);
	if (platform_device_register(&tegra_w1_device)) {
		pr_err("%s: failed to register %s.%d\n",
		       __func__, tegra_w1_device.name, tegra_w1_device.id);
	}
}
#else
static void tegra_setup_w1(void) { }
#endif

static void tegra_system_power_off(void)
{
	struct regulator *regulator = regulator_get(NULL, "soc_main");

	if (!IS_ERR(regulator)) {
		int rc;
		regulator_enable(regulator);
		rc = regulator_disable(regulator);
		pr_err("%s: regulator_disable returned %d\n", __func__, rc);
	} else {
		pr_err("%s: regulator_get returned %ld\n", __func__,
		       PTR_ERR(regulator));
	}
	local_irq_disable();
	while (1) {
		dsb();
		__asm__ ("wfi");
	}
}

static struct tegra_suspend_platform_data tegra_suspend_platform = {
	.cpu_timer = 2000,
};

static void __init tegra_setup_suspend(void)
{
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	const int wakepad_irq[] = {
		gpio_to_irq(TEGRA_GPIO_PO5), gpio_to_irq(TEGRA_GPIO_PV3),
		gpio_to_irq(TEGRA_GPIO_PL1), gpio_to_irq(TEGRA_GPIO_PB6),
		gpio_to_irq(TEGRA_GPIO_PN7), gpio_to_irq(TEGRA_GPIO_PA0),
		gpio_to_irq(TEGRA_GPIO_PU5), gpio_to_irq(TEGRA_GPIO_PU6),
		gpio_to_irq(TEGRA_GPIO_PC7), gpio_to_irq(TEGRA_GPIO_PS2),
		gpio_to_irq(TEGRA_GPIO_PAA1), gpio_to_irq(TEGRA_GPIO_PW3),
		gpio_to_irq(TEGRA_GPIO_PW2), gpio_to_irq(TEGRA_GPIO_PY6),
		gpio_to_irq(TEGRA_GPIO_PV6), gpio_to_irq(TEGRA_GPIO_PJ7),
		INT_RTC, INT_KBC, INT_EXTERNAL_PMU,
		/* FIXME: USB wake pad interrupt mapping may be wrong */
		INT_USB, INT_USB3, INT_USB, INT_USB3,
		gpio_to_irq(TEGRA_GPIO_PI5), gpio_to_irq(TEGRA_GPIO_PV2),
		gpio_to_irq(TEGRA_GPIO_PS4), gpio_to_irq(TEGRA_GPIO_PS5),
		gpio_to_irq(TEGRA_GPIO_PS0), gpio_to_irq(TEGRA_GPIO_PQ6),
		gpio_to_irq(TEGRA_GPIO_PQ7), gpio_to_irq(TEGRA_GPIO_PN2),
	};
#endif /* CONFIG_ARCH_TEGRA_2x_SOC */
	const NvOdmWakeupPadInfo *w;
	const NvOdmSocPowerStateInfo *lp;
	struct tegra_suspend_platform_data *plat = &tegra_suspend_platform;
	NvOdmPmuProperty pmu;
	NvBool has_pmu;
	NvU32 nr_wake;
	unsigned int pad;

	lp = NvOdmQueryLowestSocPowerState();
	w = NvOdmQueryGetWakeupPadTable(&nr_wake);
	has_pmu = NvOdmQueryGetPmuProperty(&pmu);

	if (!has_pmu) {
		pr_info("%s: no PMU property, ignoring all suspend state\n",
			__func__);
		goto do_register;
	}

	if (lp->LowestPowerState==NvOdmSocPowerState_Suspend) {
		printk(KERN_INFO "pICS_%s: if (lp->LowestPowerState==NvOdmSocPowerState_Suspend)",__func__);
		plat->dram_suspend = true;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.dram_suspend = true;",__func__);
		plat->core_off = false;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.core_off = false;",__func__);
	} else if (lp->LowestPowerState==NvOdmSocPowerState_DeepSleep) {
		printk(KERN_INFO "pICS_%s: if (lp->LowestPowerState==NvOdmSocPowerState_DeepSleep)",__func__);
		plat->dram_suspend = true;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.dram_suspend = true;",__func__);
		plat->core_off = true;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.core_off = true;",__func__);
	}

	if (has_pmu) {
		plat->cpu_timer = pmu.CpuPowerGoodUs;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.cpu_timer = [%lu]",__func__, plat->cpu_timer);
		plat->cpu_off_timer = pmu.CpuPowerOffUs;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.cpu_off_timer = [%lu]",__func__, plat->cpu_off_timer);
		plat->core_timer = pmu.PowerGoodCount;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.core_timer = [%lu]",__func__, plat->core_timer);
		plat->core_off_timer = pmu.PowerOffCount;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.core_off_timer = [%lu]",__func__, plat->core_off_timer);
		plat->separate_req = !pmu.CombinedPowerReq;
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.separate_req = [%d]",__func__, plat->separate_req);
		plat->corereq_high =
			(pmu.CorePowerReqPolarity ==
			 NvOdmCorePowerReqPolarity_High);
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.corereq_high = [%d]",__func__, plat->corereq_high);
		plat->sysclkreq_high =
			(pmu.SysClockReqPolarity ==
			 NvOdmCorePowerReqPolarity_High);
		printk(KERN_INFO "pICS_%s: tegra_suspend_platform.sysclkreq_high = [%d]",__func__, plat->sysclkreq_high);
	}

	if (!w || !nr_wake)
		goto do_register;

	printk(KERN_INFO "pICS_%s: plat->wake_enb = 0;...",__func__);

	plat->wake_enb = 0;
	plat->wake_low = 0;
	plat->wake_high = 0;
	plat->wake_any = 0;
	
	printk(KERN_INFO "pICS_%s: nr_wake = [%d]",__func__, nr_wake);
	while (nr_wake--) {
		printk(KERN_INFO "pICS_%s: w = %d)",__func__, w);
		pad = w->WakeupPadNumber;
		printk(KERN_INFO "pICS_%s: pad = %u)",__func__, pad); /* ICSPROBLEM here it might be */
		if (pad < ARRAY_SIZE(wakepad_irq) && w->enable) {
			enable_irq_wake(wakepad_irq[pad]);
			printk(KERN_INFO "pICS_%s: enable_irq_wake(wakepad_irq[pad]=%d)",__func__, wakepad_irq[pad]);
		}
		if (w->enable) {
			plat->wake_enb |= (1 << pad);
			printk(KERN_INFO "pICS_%s: tegra_suspend_platform.wake_enb = %lu",__func__, plat->wake_enb);
			if (w->Polarity == NvOdmWakeupPadPolarity_Low) {
				plat->wake_low |= (1 << pad);
				printk(KERN_INFO "pICS_%s: tegra_suspend_platform.wake_low = %lu",__func__, plat->wake_low);
			}
			else if (w->Polarity == NvOdmWakeupPadPolarity_High) {
				plat->wake_high |= (1 << pad);
				printk(KERN_INFO "pICS_%s: tegra_suspend_platform.wake_high = %lu",__func__, plat->wake_high);
			}
			else if (w->Polarity == NvOdmWakeupPadPolarity_AnyEdge) {
				plat->wake_any |= (1 << pad);
				printk(KERN_INFO "pICS_%s: tegra_suspend_platform.wake_any = %lu",__func__, plat->wake_any);
			}
		}
		w++;
	}

do_register:
	printk(KERN_INFO "pICS_%s: tegra_init_suspend(tegra_suspend_platform)",__func__);
	tegra_init_suspend(plat);
	tegra_init_idle(plat);
}

static int tegra_reboot_notify(struct notifier_block *nb,
				unsigned long event, void *data)
{
	printk(KERN_INFO "pICS_%s: event = [%lu]",__func__, event);
	switch (event) {
	case SYS_RESTART:
	case SYS_HALT:
	case SYS_POWER_OFF:
		/* USB power rail must be enabled during boot */
		NvOdmEnableUsbPhyPowerRail(NV_TRUE);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block tegra_reboot_nb = {
	.notifier_call = tegra_reboot_notify,
	.next = NULL,
	.priority = 0
};

static void __init tegra_setup_reboot(void)
{
	int rc = register_reboot_notifier(&tegra_reboot_nb);
	if (rc)
		pr_err("%s: failed to regsiter platform reboot notifier\n",
			__func__);
}

static int __init tegra_setup_data(void)
{
	printk(KERN_INFO "pICS_%s: empty list of devices",__func__);
	platform_add_devices(nvodm_devices, ARRAY_SIZE(nvodm_devices));
	return 0;
}
postcore_initcall(tegra_setup_data);

void __init tegra_setup_nvodm(bool standard_i2c, bool standard_spi)
{
	printk(KERN_INFO "pICS_%s: NvRmGpioOpen(s_hRmGlobal, &s_hGpioGlobal); \n",__func__);
	NvRmGpioOpen(s_hRmGlobal, &s_hGpioGlobal);
	tegra_setup_debug_uart();
	tegra_setup_hcd();
	tegra_setup_hsuart();
	tegra_setup_sdhci();
	tegra_setup_rfkill();
	tegra_setup_kbc();
	tegra_setup_gpio_key();
	if (standard_i2c)
		tegra_setup_i2c();
	if (standard_spi)
		tegra_setup_spi();
	tegra_setup_w1();
	pm_power_off = tegra_system_power_off;
	tegra_setup_suspend();
	tegra_setup_reboot();
}

void tegra_board_nvodm_suspend(void)
{
	printk(KERN_INFO "pICS_%s",__func__);
	if (console_suspend_enabled)
		tegra_debug_port_suspend();
}

void tegra_board_nvodm_resume(void)
{
	printk(KERN_INFO "pICS_%s",__func__);
	if (console_suspend_enabled)
		tegra_debug_port_resume();
}
