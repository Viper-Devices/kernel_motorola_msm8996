/*
 * MV-643XX ethernet platform device data definition file.
 */

#ifndef __LINUX_MV643XX_ETH_H
#define __LINUX_MV643XX_ETH_H

#include <linux/mbus.h>

#define MV643XX_ETH_SHARED_NAME		"mv643xx_eth"
#define MV643XX_ETH_NAME		"mv643xx_eth_port"
#define MV643XX_ETH_SHARED_REGS		0x2000
#define MV643XX_ETH_SHARED_REGS_SIZE	0x2000
#define MV643XX_ETH_BAR_4		0x2220
#define MV643XX_ETH_SIZE_REG_4		0x2224
#define MV643XX_ETH_BASE_ADDR_ENABLE_REG	0x2290

struct mv643xx_eth_shared_platform_data {
	struct mbus_dram_target_info	*dram;
	unsigned int		t_clk;
};

struct mv643xx_eth_platform_data {
	/*
	 * Pointer back to our parent instance, and our port number.
	 */
	struct platform_device	*shared;
	int			port_number;

	/*
	 * Whether a PHY is present, and if yes, at which address.
	 */
	struct platform_device	*shared_smi;
	int			force_phy_addr;
	int			phy_addr;

	/*
	 * Use this MAC address if it is valid, overriding the
	 * address that is already in the hardware.
	 */
	u8			mac_addr[6];

	/*
	 * If speed is 0, autonegotiation is enabled.
	 *   Valid values for speed: 0, SPEED_10, SPEED_100, SPEED_1000.
	 *   Valid values for duplex: DUPLEX_HALF, DUPLEX_FULL.
	 */
	int			speed;
	int			duplex;

	/*
	 * Which RX/TX queues to use.
	 */
	int			rx_queue_mask;
	int			tx_queue_mask;

	/*
	 * Override default RX/TX queue sizes if nonzero.
	 */
	int			rx_queue_size;
	int			tx_queue_size;

	/*
	 * Use on-chip SRAM for RX/TX descriptors if size is nonzero
	 * and sufficient to contain all descriptors for the requested
	 * ring sizes.
	 */
	unsigned long		rx_sram_addr;
	int			rx_sram_size;
	unsigned long		tx_sram_addr;
	int			tx_sram_size;
};


#endif
