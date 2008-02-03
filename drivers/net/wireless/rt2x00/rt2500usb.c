/*
	Copyright (C) 2004 - 2008 rt2x00 SourceForge Project
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2500usb
	Abstract: rt2500usb device specific routines.
	Supported chipsets: RT2570.
 */

#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>

#include "rt2x00.h"
#include "rt2x00usb.h"
#include "rt2500usb.h"

/*
 * Register access.
 * All access to the CSR registers will go through the methods
 * rt2500usb_register_read and rt2500usb_register_write.
 * BBP and RF register require indirect register access,
 * and use the CSR registers BBPCSR and RFCSR to achieve this.
 * These indirect registers work with busy bits,
 * and we will try maximal REGISTER_BUSY_COUNT times to access
 * the register while taking a REGISTER_BUSY_DELAY us delay
 * between each attampt. When the busy bit is still set at that time,
 * the access attempt is considered to have failed,
 * and we will print an error.
 * If the usb_cache_mutex is already held then the _lock variants must
 * be used instead.
 */
static inline void rt2500usb_register_read(struct rt2x00_dev *rt2x00dev,
					   const unsigned int offset,
					   u16 *value)
{
	__le16 reg;
	rt2x00usb_vendor_request_buff(rt2x00dev, USB_MULTI_READ,
				      USB_VENDOR_REQUEST_IN, offset,
				      &reg, sizeof(u16), REGISTER_TIMEOUT);
	*value = le16_to_cpu(reg);
}

static inline void rt2500usb_register_read_lock(struct rt2x00_dev *rt2x00dev,
						const unsigned int offset,
						u16 *value)
{
	__le16 reg;
	rt2x00usb_vendor_req_buff_lock(rt2x00dev, USB_MULTI_READ,
				       USB_VENDOR_REQUEST_IN, offset,
				       &reg, sizeof(u16), REGISTER_TIMEOUT);
	*value = le16_to_cpu(reg);
}

static inline void rt2500usb_register_multiread(struct rt2x00_dev *rt2x00dev,
						const unsigned int offset,
						void *value, const u16 length)
{
	int timeout = REGISTER_TIMEOUT * (length / sizeof(u16));
	rt2x00usb_vendor_request_buff(rt2x00dev, USB_MULTI_READ,
				      USB_VENDOR_REQUEST_IN, offset,
				      value, length, timeout);
}

static inline void rt2500usb_register_write(struct rt2x00_dev *rt2x00dev,
					    const unsigned int offset,
					    u16 value)
{
	__le16 reg = cpu_to_le16(value);
	rt2x00usb_vendor_request_buff(rt2x00dev, USB_MULTI_WRITE,
				      USB_VENDOR_REQUEST_OUT, offset,
				      &reg, sizeof(u16), REGISTER_TIMEOUT);
}

static inline void rt2500usb_register_write_lock(struct rt2x00_dev *rt2x00dev,
						 const unsigned int offset,
						 u16 value)
{
	__le16 reg = cpu_to_le16(value);
	rt2x00usb_vendor_req_buff_lock(rt2x00dev, USB_MULTI_WRITE,
				       USB_VENDOR_REQUEST_OUT, offset,
				       &reg, sizeof(u16), REGISTER_TIMEOUT);
}

static inline void rt2500usb_register_multiwrite(struct rt2x00_dev *rt2x00dev,
						 const unsigned int offset,
						 void *value, const u16 length)
{
	int timeout = REGISTER_TIMEOUT * (length / sizeof(u16));
	rt2x00usb_vendor_request_buff(rt2x00dev, USB_MULTI_WRITE,
				      USB_VENDOR_REQUEST_OUT, offset,
				      value, length, timeout);
}

static u16 rt2500usb_bbp_check(struct rt2x00_dev *rt2x00dev)
{
	u16 reg;
	unsigned int i;

	for (i = 0; i < REGISTER_BUSY_COUNT; i++) {
		rt2500usb_register_read_lock(rt2x00dev, PHY_CSR8, &reg);
		if (!rt2x00_get_field16(reg, PHY_CSR8_BUSY))
			break;
		udelay(REGISTER_BUSY_DELAY);
	}

	return reg;
}

static void rt2500usb_bbp_write(struct rt2x00_dev *rt2x00dev,
				const unsigned int word, const u8 value)
{
	u16 reg;

	mutex_lock(&rt2x00dev->usb_cache_mutex);

	/*
	 * Wait until the BBP becomes ready.
	 */
	reg = rt2500usb_bbp_check(rt2x00dev);
	if (rt2x00_get_field16(reg, PHY_CSR8_BUSY)) {
		ERROR(rt2x00dev, "PHY_CSR8 register busy. Write failed.\n");
		mutex_unlock(&rt2x00dev->usb_cache_mutex);
		return;
	}

	/*
	 * Write the data into the BBP.
	 */
	reg = 0;
	rt2x00_set_field16(&reg, PHY_CSR7_DATA, value);
	rt2x00_set_field16(&reg, PHY_CSR7_REG_ID, word);
	rt2x00_set_field16(&reg, PHY_CSR7_READ_CONTROL, 0);

	rt2500usb_register_write_lock(rt2x00dev, PHY_CSR7, reg);

	mutex_unlock(&rt2x00dev->usb_cache_mutex);
}

static void rt2500usb_bbp_read(struct rt2x00_dev *rt2x00dev,
			       const unsigned int word, u8 *value)
{
	u16 reg;

	mutex_lock(&rt2x00dev->usb_cache_mutex);

	/*
	 * Wait until the BBP becomes ready.
	 */
	reg = rt2500usb_bbp_check(rt2x00dev);
	if (rt2x00_get_field16(reg, PHY_CSR8_BUSY)) {
		ERROR(rt2x00dev, "PHY_CSR8 register busy. Read failed.\n");
		return;
	}

	/*
	 * Write the request into the BBP.
	 */
	reg = 0;
	rt2x00_set_field16(&reg, PHY_CSR7_REG_ID, word);
	rt2x00_set_field16(&reg, PHY_CSR7_READ_CONTROL, 1);

	rt2500usb_register_write_lock(rt2x00dev, PHY_CSR7, reg);

	/*
	 * Wait until the BBP becomes ready.
	 */
	reg = rt2500usb_bbp_check(rt2x00dev);
	if (rt2x00_get_field16(reg, PHY_CSR8_BUSY)) {
		ERROR(rt2x00dev, "PHY_CSR8 register busy. Read failed.\n");
		*value = 0xff;
		mutex_unlock(&rt2x00dev->usb_cache_mutex);
		return;
	}

	rt2500usb_register_read_lock(rt2x00dev, PHY_CSR7, &reg);
	*value = rt2x00_get_field16(reg, PHY_CSR7_DATA);

	mutex_unlock(&rt2x00dev->usb_cache_mutex);
}

static void rt2500usb_rf_write(struct rt2x00_dev *rt2x00dev,
			       const unsigned int word, const u32 value)
{
	u16 reg;
	unsigned int i;

	if (!word)
		return;

	mutex_lock(&rt2x00dev->usb_cache_mutex);

	for (i = 0; i < REGISTER_BUSY_COUNT; i++) {
		rt2500usb_register_read_lock(rt2x00dev, PHY_CSR10, &reg);
		if (!rt2x00_get_field16(reg, PHY_CSR10_RF_BUSY))
			goto rf_write;
		udelay(REGISTER_BUSY_DELAY);
	}

	mutex_unlock(&rt2x00dev->usb_cache_mutex);
	ERROR(rt2x00dev, "PHY_CSR10 register busy. Write failed.\n");
	return;

rf_write:
	reg = 0;
	rt2x00_set_field16(&reg, PHY_CSR9_RF_VALUE, value);
	rt2500usb_register_write_lock(rt2x00dev, PHY_CSR9, reg);

	reg = 0;
	rt2x00_set_field16(&reg, PHY_CSR10_RF_VALUE, value >> 16);
	rt2x00_set_field16(&reg, PHY_CSR10_RF_NUMBER_OF_BITS, 20);
	rt2x00_set_field16(&reg, PHY_CSR10_RF_IF_SELECT, 0);
	rt2x00_set_field16(&reg, PHY_CSR10_RF_BUSY, 1);

	rt2500usb_register_write_lock(rt2x00dev, PHY_CSR10, reg);
	rt2x00_rf_write(rt2x00dev, word, value);

	mutex_unlock(&rt2x00dev->usb_cache_mutex);
}

#ifdef CONFIG_RT2X00_LIB_DEBUGFS
#define CSR_OFFSET(__word)	( CSR_REG_BASE + ((__word) * sizeof(u16)) )

static void rt2500usb_read_csr(struct rt2x00_dev *rt2x00dev,
			       const unsigned int word, u32 *data)
{
	rt2500usb_register_read(rt2x00dev, CSR_OFFSET(word), (u16 *) data);
}

static void rt2500usb_write_csr(struct rt2x00_dev *rt2x00dev,
				const unsigned int word, u32 data)
{
	rt2500usb_register_write(rt2x00dev, CSR_OFFSET(word), data);
}

static const struct rt2x00debug rt2500usb_rt2x00debug = {
	.owner	= THIS_MODULE,
	.csr	= {
		.read		= rt2500usb_read_csr,
		.write		= rt2500usb_write_csr,
		.word_size	= sizeof(u16),
		.word_count	= CSR_REG_SIZE / sizeof(u16),
	},
	.eeprom	= {
		.read		= rt2x00_eeprom_read,
		.write		= rt2x00_eeprom_write,
		.word_size	= sizeof(u16),
		.word_count	= EEPROM_SIZE / sizeof(u16),
	},
	.bbp	= {
		.read		= rt2500usb_bbp_read,
		.write		= rt2500usb_bbp_write,
		.word_size	= sizeof(u8),
		.word_count	= BBP_SIZE / sizeof(u8),
	},
	.rf	= {
		.read		= rt2x00_rf_read,
		.write		= rt2500usb_rf_write,
		.word_size	= sizeof(u32),
		.word_count	= RF_SIZE / sizeof(u32),
	},
};
#endif /* CONFIG_RT2X00_LIB_DEBUGFS */

#ifdef CONFIG_RT2500USB_LEDS
static void rt2500usb_led_brightness(struct led_classdev *led_cdev,
				     enum led_brightness brightness)
{
	struct rt2x00_led *led =
	    container_of(led_cdev, struct rt2x00_led, led_dev);
	unsigned int enabled = brightness != LED_OFF;
	unsigned int activity =
	    led->rt2x00dev->led_flags & LED_SUPPORT_ACTIVITY;
	u16 reg;

	rt2500usb_register_read(led->rt2x00dev, MAC_CSR20, &reg);

	if (led->type == LED_TYPE_RADIO || led->type == LED_TYPE_ASSOC) {
		rt2x00_set_field16(&reg, MAC_CSR20_LINK, enabled);
		rt2x00_set_field16(&reg, MAC_CSR20_ACTIVITY,
				   enabled && activity);
	}

	rt2500usb_register_write(led->rt2x00dev, MAC_CSR20, reg);
}
#else
#define rt2500usb_led_brightness	NULL
#endif /* CONFIG_RT2500USB_LEDS */

/*
 * Configuration handlers.
 */
static void rt2500usb_config_intf(struct rt2x00_dev *rt2x00dev,
				  struct rt2x00_intf *intf,
				  struct rt2x00intf_conf *conf,
				  const unsigned int flags)
{
	unsigned int bcn_preload;
	u16 reg;

	if (flags & CONFIG_UPDATE_TYPE) {
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, 0);

		/*
		 * Enable beacon config
		 */
		bcn_preload = PREAMBLE + get_duration(IEEE80211_HEADER, 20);
		rt2500usb_register_read(rt2x00dev, TXRX_CSR20, &reg);
		rt2x00_set_field16(&reg, TXRX_CSR20_OFFSET, bcn_preload >> 6);
		rt2x00_set_field16(&reg, TXRX_CSR20_BCN_EXPECT_WINDOW,
				   2 * (conf->type != IEEE80211_IF_TYPE_STA));
		rt2500usb_register_write(rt2x00dev, TXRX_CSR20, reg);

		/*
		 * Enable synchronisation.
		 */
		rt2500usb_register_read(rt2x00dev, TXRX_CSR18, &reg);
		rt2x00_set_field16(&reg, TXRX_CSR18_OFFSET, 0);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR18, reg);

		rt2500usb_register_read(rt2x00dev, TXRX_CSR19, &reg);
		rt2x00_set_field16(&reg, TXRX_CSR19_TSF_COUNT, 1);
		rt2x00_set_field16(&reg, TXRX_CSR19_TBCN,
				   (conf->sync == TSF_SYNC_BEACON));
		rt2x00_set_field16(&reg, TXRX_CSR19_BEACON_GEN, 0);
		rt2x00_set_field16(&reg, TXRX_CSR19_TSF_SYNC, conf->sync);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, reg);
	}

	if (flags & CONFIG_UPDATE_MAC)
		rt2500usb_register_multiwrite(rt2x00dev, MAC_CSR2, conf->mac,
					      (3 * sizeof(__le16)));

	if (flags & CONFIG_UPDATE_BSSID)
		rt2500usb_register_multiwrite(rt2x00dev, MAC_CSR5, conf->bssid,
					      (3 * sizeof(__le16)));
}

static int rt2500usb_config_preamble(struct rt2x00_dev *rt2x00dev,
				     const int short_preamble,
				     const int ack_timeout,
				     const int ack_consume_time)
{
	u16 reg;

	/*
	 * When in atomic context, we should let rt2x00lib
	 * try this configuration again later.
	 */
	if (in_atomic())
		return -EAGAIN;

	rt2500usb_register_read(rt2x00dev, TXRX_CSR1, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR1_ACK_TIMEOUT, ack_timeout);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR1, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR10, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR10_AUTORESPOND_PREAMBLE,
			   !!short_preamble);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR10, reg);

	return 0;
}

static void rt2500usb_config_phymode(struct rt2x00_dev *rt2x00dev,
				     const int basic_rate_mask)
{
	rt2500usb_register_write(rt2x00dev, TXRX_CSR11, basic_rate_mask);
}

static void rt2500usb_config_channel(struct rt2x00_dev *rt2x00dev,
				     struct rf_channel *rf, const int txpower)
{
	/*
	 * Set TXpower.
	 */
	rt2x00_set_field32(&rf->rf3, RF3_TXPOWER, TXPOWER_TO_DEV(txpower));

	/*
	 * For RT2525E we should first set the channel to half band higher.
	 */
	if (rt2x00_rf(&rt2x00dev->chip, RF2525E)) {
		static const u32 vals[] = {
			0x000008aa, 0x000008ae, 0x000008ae, 0x000008b2,
			0x000008b2, 0x000008b6, 0x000008b6, 0x000008ba,
			0x000008ba, 0x000008be, 0x000008b7, 0x00000902,
			0x00000902, 0x00000906
		};

		rt2500usb_rf_write(rt2x00dev, 2, vals[rf->channel - 1]);
		if (rf->rf4)
			rt2500usb_rf_write(rt2x00dev, 4, rf->rf4);
	}

	rt2500usb_rf_write(rt2x00dev, 1, rf->rf1);
	rt2500usb_rf_write(rt2x00dev, 2, rf->rf2);
	rt2500usb_rf_write(rt2x00dev, 3, rf->rf3);
	if (rf->rf4)
		rt2500usb_rf_write(rt2x00dev, 4, rf->rf4);
}

static void rt2500usb_config_txpower(struct rt2x00_dev *rt2x00dev,
				     const int txpower)
{
	u32 rf3;

	rt2x00_rf_read(rt2x00dev, 3, &rf3);
	rt2x00_set_field32(&rf3, RF3_TXPOWER, TXPOWER_TO_DEV(txpower));
	rt2500usb_rf_write(rt2x00dev, 3, rf3);
}

static void rt2500usb_config_antenna(struct rt2x00_dev *rt2x00dev,
				     struct antenna_setup *ant)
{
	u8 r2;
	u8 r14;
	u16 csr5;
	u16 csr6;

	rt2500usb_bbp_read(rt2x00dev, 2, &r2);
	rt2500usb_bbp_read(rt2x00dev, 14, &r14);
	rt2500usb_register_read(rt2x00dev, PHY_CSR5, &csr5);
	rt2500usb_register_read(rt2x00dev, PHY_CSR6, &csr6);

	/*
	 * Configure the TX antenna.
	 */
	switch (ant->tx) {
	case ANTENNA_HW_DIVERSITY:
		rt2x00_set_field8(&r2, BBP_R2_TX_ANTENNA, 1);
		rt2x00_set_field16(&csr5, PHY_CSR5_CCK, 1);
		rt2x00_set_field16(&csr6, PHY_CSR6_OFDM, 1);
		break;
	case ANTENNA_A:
		rt2x00_set_field8(&r2, BBP_R2_TX_ANTENNA, 0);
		rt2x00_set_field16(&csr5, PHY_CSR5_CCK, 0);
		rt2x00_set_field16(&csr6, PHY_CSR6_OFDM, 0);
		break;
	case ANTENNA_SW_DIVERSITY:
		/*
		 * NOTE: We should never come here because rt2x00lib is
		 * supposed to catch this and send us the correct antenna
		 * explicitely. However we are nog going to bug about this.
		 * Instead, just default to antenna B.
		 */
	case ANTENNA_B:
		rt2x00_set_field8(&r2, BBP_R2_TX_ANTENNA, 2);
		rt2x00_set_field16(&csr5, PHY_CSR5_CCK, 2);
		rt2x00_set_field16(&csr6, PHY_CSR6_OFDM, 2);
		break;
	}

	/*
	 * Configure the RX antenna.
	 */
	switch (ant->rx) {
	case ANTENNA_HW_DIVERSITY:
		rt2x00_set_field8(&r14, BBP_R14_RX_ANTENNA, 1);
		break;
	case ANTENNA_A:
		rt2x00_set_field8(&r14, BBP_R14_RX_ANTENNA, 0);
		break;
	case ANTENNA_SW_DIVERSITY:
		/*
		 * NOTE: We should never come here because rt2x00lib is
		 * supposed to catch this and send us the correct antenna
		 * explicitely. However we are nog going to bug about this.
		 * Instead, just default to antenna B.
		 */
	case ANTENNA_B:
		rt2x00_set_field8(&r14, BBP_R14_RX_ANTENNA, 2);
		break;
	}

	/*
	 * RT2525E and RT5222 need to flip TX I/Q
	 */
	if (rt2x00_rf(&rt2x00dev->chip, RF2525E) ||
	    rt2x00_rf(&rt2x00dev->chip, RF5222)) {
		rt2x00_set_field8(&r2, BBP_R2_TX_IQ_FLIP, 1);
		rt2x00_set_field16(&csr5, PHY_CSR5_CCK_FLIP, 1);
		rt2x00_set_field16(&csr6, PHY_CSR6_OFDM_FLIP, 1);

		/*
		 * RT2525E does not need RX I/Q Flip.
		 */
		if (rt2x00_rf(&rt2x00dev->chip, RF2525E))
			rt2x00_set_field8(&r14, BBP_R14_RX_IQ_FLIP, 0);
	} else {
		rt2x00_set_field16(&csr5, PHY_CSR5_CCK_FLIP, 0);
		rt2x00_set_field16(&csr6, PHY_CSR6_OFDM_FLIP, 0);
	}

	rt2500usb_bbp_write(rt2x00dev, 2, r2);
	rt2500usb_bbp_write(rt2x00dev, 14, r14);
	rt2500usb_register_write(rt2x00dev, PHY_CSR5, csr5);
	rt2500usb_register_write(rt2x00dev, PHY_CSR6, csr6);
}

static void rt2500usb_config_duration(struct rt2x00_dev *rt2x00dev,
				      struct rt2x00lib_conf *libconf)
{
	u16 reg;

	rt2500usb_register_write(rt2x00dev, MAC_CSR10, libconf->slot_time);
	rt2500usb_register_write(rt2x00dev, MAC_CSR11, libconf->sifs);
	rt2500usb_register_write(rt2x00dev, MAC_CSR12, libconf->eifs);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR18, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR18_INTERVAL,
			   libconf->conf->beacon_int * 4);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR18, reg);
}

static void rt2500usb_config(struct rt2x00_dev *rt2x00dev,
			     struct rt2x00lib_conf *libconf,
			     const unsigned int flags)
{
	if (flags & CONFIG_UPDATE_PHYMODE)
		rt2500usb_config_phymode(rt2x00dev, libconf->basic_rates);
	if (flags & CONFIG_UPDATE_CHANNEL)
		rt2500usb_config_channel(rt2x00dev, &libconf->rf,
					 libconf->conf->power_level);
	if ((flags & CONFIG_UPDATE_TXPOWER) && !(flags & CONFIG_UPDATE_CHANNEL))
		rt2500usb_config_txpower(rt2x00dev,
					 libconf->conf->power_level);
	if (flags & CONFIG_UPDATE_ANTENNA)
		rt2500usb_config_antenna(rt2x00dev, &libconf->ant);
	if (flags & (CONFIG_UPDATE_SLOT_TIME | CONFIG_UPDATE_BEACON_INT))
		rt2500usb_config_duration(rt2x00dev, libconf);
}

/*
 * Link tuning
 */
static void rt2500usb_link_stats(struct rt2x00_dev *rt2x00dev,
				 struct link_qual *qual)
{
	u16 reg;

	/*
	 * Update FCS error count from register.
	 */
	rt2500usb_register_read(rt2x00dev, STA_CSR0, &reg);
	qual->rx_failed = rt2x00_get_field16(reg, STA_CSR0_FCS_ERROR);

	/*
	 * Update False CCA count from register.
	 */
	rt2500usb_register_read(rt2x00dev, STA_CSR3, &reg);
	qual->false_cca = rt2x00_get_field16(reg, STA_CSR3_FALSE_CCA_ERROR);
}

static void rt2500usb_reset_tuner(struct rt2x00_dev *rt2x00dev)
{
	u16 eeprom;
	u16 value;

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R24, &eeprom);
	value = rt2x00_get_field16(eeprom, EEPROM_BBPTUNE_R24_LOW);
	rt2500usb_bbp_write(rt2x00dev, 24, value);

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R25, &eeprom);
	value = rt2x00_get_field16(eeprom, EEPROM_BBPTUNE_R25_LOW);
	rt2500usb_bbp_write(rt2x00dev, 25, value);

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R61, &eeprom);
	value = rt2x00_get_field16(eeprom, EEPROM_BBPTUNE_R61_LOW);
	rt2500usb_bbp_write(rt2x00dev, 61, value);

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_VGC, &eeprom);
	value = rt2x00_get_field16(eeprom, EEPROM_BBPTUNE_VGCUPPER);
	rt2500usb_bbp_write(rt2x00dev, 17, value);

	rt2x00dev->link.vgc_level = value;
}

static void rt2500usb_link_tuner(struct rt2x00_dev *rt2x00dev)
{
	int rssi = rt2x00_get_link_rssi(&rt2x00dev->link);
	u16 bbp_thresh;
	u16 vgc_bound;
	u16 sens;
	u16 r24;
	u16 r25;
	u16 r61;
	u16 r17_sens;
	u8 r17;
	u8 up_bound;
	u8 low_bound;

	/*
	 * Read current r17 value, as well as the sensitivity values
	 * for the r17 register.
	 */
	rt2500usb_bbp_read(rt2x00dev, 17, &r17);
	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R17, &r17_sens);

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_VGC, &vgc_bound);
	up_bound = rt2x00_get_field16(vgc_bound, EEPROM_BBPTUNE_VGCUPPER);
	low_bound = rt2x00_get_field16(vgc_bound, EEPROM_BBPTUNE_VGCLOWER);

	/*
	 * If we are not associated, we should go straight to the
	 * dynamic CCA tuning.
	 */
	if (!rt2x00dev->intf_associated)
		goto dynamic_cca_tune;

	/*
	 * Determine the BBP tuning threshold and correctly
	 * set BBP 24, 25 and 61.
	 */
	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE, &bbp_thresh);
	bbp_thresh = rt2x00_get_field16(bbp_thresh, EEPROM_BBPTUNE_THRESHOLD);

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R24, &r24);
	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R25, &r25);
	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R61, &r61);

	if ((rssi + bbp_thresh) > 0) {
		r24 = rt2x00_get_field16(r24, EEPROM_BBPTUNE_R24_HIGH);
		r25 = rt2x00_get_field16(r25, EEPROM_BBPTUNE_R25_HIGH);
		r61 = rt2x00_get_field16(r61, EEPROM_BBPTUNE_R61_HIGH);
	} else {
		r24 = rt2x00_get_field16(r24, EEPROM_BBPTUNE_R24_LOW);
		r25 = rt2x00_get_field16(r25, EEPROM_BBPTUNE_R25_LOW);
		r61 = rt2x00_get_field16(r61, EEPROM_BBPTUNE_R61_LOW);
	}

	rt2500usb_bbp_write(rt2x00dev, 24, r24);
	rt2500usb_bbp_write(rt2x00dev, 25, r25);
	rt2500usb_bbp_write(rt2x00dev, 61, r61);

	/*
	 * A too low RSSI will cause too much false CCA which will
	 * then corrupt the R17 tuning. To remidy this the tuning should
	 * be stopped (While making sure the R17 value will not exceed limits)
	 */
	if (rssi >= -40) {
		if (r17 != 0x60)
			rt2500usb_bbp_write(rt2x00dev, 17, 0x60);
		return;
	}

	/*
	 * Special big-R17 for short distance
	 */
	if (rssi >= -58) {
		sens = rt2x00_get_field16(r17_sens, EEPROM_BBPTUNE_R17_LOW);
		if (r17 != sens)
			rt2500usb_bbp_write(rt2x00dev, 17, sens);
		return;
	}

	/*
	 * Special mid-R17 for middle distance
	 */
	if (rssi >= -74) {
		sens = rt2x00_get_field16(r17_sens, EEPROM_BBPTUNE_R17_HIGH);
		if (r17 != sens)
			rt2500usb_bbp_write(rt2x00dev, 17, sens);
		return;
	}

	/*
	 * Leave short or middle distance condition, restore r17
	 * to the dynamic tuning range.
	 */
	low_bound = 0x32;
	if (rssi < -77)
		up_bound -= (-77 - rssi);

	if (up_bound < low_bound)
		up_bound = low_bound;

	if (r17 > up_bound) {
		rt2500usb_bbp_write(rt2x00dev, 17, up_bound);
		rt2x00dev->link.vgc_level = up_bound;
		return;
	}

dynamic_cca_tune:

	/*
	 * R17 is inside the dynamic tuning range,
	 * start tuning the link based on the false cca counter.
	 */
	if (rt2x00dev->link.qual.false_cca > 512 && r17 < up_bound) {
		rt2500usb_bbp_write(rt2x00dev, 17, ++r17);
		rt2x00dev->link.vgc_level = r17;
	} else if (rt2x00dev->link.qual.false_cca < 100 && r17 > low_bound) {
		rt2500usb_bbp_write(rt2x00dev, 17, --r17);
		rt2x00dev->link.vgc_level = r17;
	}
}

/*
 * Initialization functions.
 */
static int rt2500usb_init_registers(struct rt2x00_dev *rt2x00dev)
{
	u16 reg;

	rt2x00usb_vendor_request_sw(rt2x00dev, USB_DEVICE_MODE, 0x0001,
				    USB_MODE_TEST, REGISTER_TIMEOUT);
	rt2x00usb_vendor_request_sw(rt2x00dev, USB_SINGLE_WRITE, 0x0308,
				    0x00f0, REGISTER_TIMEOUT);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR2, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR2_DISABLE_RX, 1);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR2, reg);

	rt2500usb_register_write(rt2x00dev, MAC_CSR13, 0x1111);
	rt2500usb_register_write(rt2x00dev, MAC_CSR14, 0x1e11);

	rt2500usb_register_read(rt2x00dev, MAC_CSR1, &reg);
	rt2x00_set_field16(&reg, MAC_CSR1_SOFT_RESET, 1);
	rt2x00_set_field16(&reg, MAC_CSR1_BBP_RESET, 1);
	rt2x00_set_field16(&reg, MAC_CSR1_HOST_READY, 0);
	rt2500usb_register_write(rt2x00dev, MAC_CSR1, reg);

	rt2500usb_register_read(rt2x00dev, MAC_CSR1, &reg);
	rt2x00_set_field16(&reg, MAC_CSR1_SOFT_RESET, 0);
	rt2x00_set_field16(&reg, MAC_CSR1_BBP_RESET, 0);
	rt2x00_set_field16(&reg, MAC_CSR1_HOST_READY, 0);
	rt2500usb_register_write(rt2x00dev, MAC_CSR1, reg);

	rt2500usb_register_read(rt2x00dev, MAC_CSR21, &reg);
	rt2x00_set_field16(&reg, MAC_CSR21_ON_PERIOD, 70);
	rt2x00_set_field16(&reg, MAC_CSR21_OFF_PERIOD, 30);
	rt2500usb_register_write(rt2x00dev, MAC_CSR21, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR5, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR5_BBP_ID0, 13);
	rt2x00_set_field16(&reg, TXRX_CSR5_BBP_ID0_VALID, 1);
	rt2x00_set_field16(&reg, TXRX_CSR5_BBP_ID1, 12);
	rt2x00_set_field16(&reg, TXRX_CSR5_BBP_ID1_VALID, 1);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR5, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR6, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR6_BBP_ID0, 10);
	rt2x00_set_field16(&reg, TXRX_CSR6_BBP_ID0_VALID, 1);
	rt2x00_set_field16(&reg, TXRX_CSR6_BBP_ID1, 11);
	rt2x00_set_field16(&reg, TXRX_CSR6_BBP_ID1_VALID, 1);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR6, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR7, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR7_BBP_ID0, 7);
	rt2x00_set_field16(&reg, TXRX_CSR7_BBP_ID0_VALID, 1);
	rt2x00_set_field16(&reg, TXRX_CSR7_BBP_ID1, 6);
	rt2x00_set_field16(&reg, TXRX_CSR7_BBP_ID1_VALID, 1);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR7, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR8, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR8_BBP_ID0, 5);
	rt2x00_set_field16(&reg, TXRX_CSR8_BBP_ID0_VALID, 1);
	rt2x00_set_field16(&reg, TXRX_CSR8_BBP_ID1, 0);
	rt2x00_set_field16(&reg, TXRX_CSR8_BBP_ID1_VALID, 0);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR8, reg);

	rt2500usb_register_write(rt2x00dev, TXRX_CSR21, 0xe78f);
	rt2500usb_register_write(rt2x00dev, MAC_CSR9, 0xff1d);

	if (rt2x00dev->ops->lib->set_device_state(rt2x00dev, STATE_AWAKE))
		return -EBUSY;

	rt2500usb_register_read(rt2x00dev, MAC_CSR1, &reg);
	rt2x00_set_field16(&reg, MAC_CSR1_SOFT_RESET, 0);
	rt2x00_set_field16(&reg, MAC_CSR1_BBP_RESET, 0);
	rt2x00_set_field16(&reg, MAC_CSR1_HOST_READY, 1);
	rt2500usb_register_write(rt2x00dev, MAC_CSR1, reg);

	if (rt2x00_rev(&rt2x00dev->chip) >= RT2570_VERSION_C) {
		rt2500usb_register_read(rt2x00dev, PHY_CSR2, &reg);
		rt2x00_set_field16(&reg, PHY_CSR2_LNA, 0);
	} else {
		reg = 0;
		rt2x00_set_field16(&reg, PHY_CSR2_LNA, 1);
		rt2x00_set_field16(&reg, PHY_CSR2_LNA_MODE, 3);
	}
	rt2500usb_register_write(rt2x00dev, PHY_CSR2, reg);

	rt2500usb_register_write(rt2x00dev, MAC_CSR11, 0x0002);
	rt2500usb_register_write(rt2x00dev, MAC_CSR22, 0x0053);
	rt2500usb_register_write(rt2x00dev, MAC_CSR15, 0x01ee);
	rt2500usb_register_write(rt2x00dev, MAC_CSR16, 0x0000);

	rt2500usb_register_read(rt2x00dev, MAC_CSR8, &reg);
	rt2x00_set_field16(&reg, MAC_CSR8_MAX_FRAME_UNIT,
			   rt2x00dev->rx->data_size);
	rt2500usb_register_write(rt2x00dev, MAC_CSR8, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR0, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR0_IV_OFFSET, IEEE80211_HEADER);
	rt2x00_set_field16(&reg, TXRX_CSR0_KEY_ID, 0xff);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR0, reg);

	rt2500usb_register_read(rt2x00dev, MAC_CSR18, &reg);
	rt2x00_set_field16(&reg, MAC_CSR18_DELAY_AFTER_BEACON, 90);
	rt2500usb_register_write(rt2x00dev, MAC_CSR18, reg);

	rt2500usb_register_read(rt2x00dev, PHY_CSR4, &reg);
	rt2x00_set_field16(&reg, PHY_CSR4_LOW_RF_LE, 1);
	rt2500usb_register_write(rt2x00dev, PHY_CSR4, reg);

	rt2500usb_register_read(rt2x00dev, TXRX_CSR1, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR1_AUTO_SEQUENCE, 1);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR1, reg);

	return 0;
}

static int rt2500usb_init_bbp(struct rt2x00_dev *rt2x00dev)
{
	unsigned int i;
	u16 eeprom;
	u8 value;
	u8 reg_id;

	for (i = 0; i < REGISTER_BUSY_COUNT; i++) {
		rt2500usb_bbp_read(rt2x00dev, 0, &value);
		if ((value != 0xff) && (value != 0x00))
			goto continue_csr_init;
		NOTICE(rt2x00dev, "Waiting for BBP register.\n");
		udelay(REGISTER_BUSY_DELAY);
	}

	ERROR(rt2x00dev, "BBP register access failed, aborting.\n");
	return -EACCES;

continue_csr_init:
	rt2500usb_bbp_write(rt2x00dev, 3, 0x02);
	rt2500usb_bbp_write(rt2x00dev, 4, 0x19);
	rt2500usb_bbp_write(rt2x00dev, 14, 0x1c);
	rt2500usb_bbp_write(rt2x00dev, 15, 0x30);
	rt2500usb_bbp_write(rt2x00dev, 16, 0xac);
	rt2500usb_bbp_write(rt2x00dev, 18, 0x18);
	rt2500usb_bbp_write(rt2x00dev, 19, 0xff);
	rt2500usb_bbp_write(rt2x00dev, 20, 0x1e);
	rt2500usb_bbp_write(rt2x00dev, 21, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 22, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 23, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 24, 0x80);
	rt2500usb_bbp_write(rt2x00dev, 25, 0x50);
	rt2500usb_bbp_write(rt2x00dev, 26, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 27, 0x23);
	rt2500usb_bbp_write(rt2x00dev, 30, 0x10);
	rt2500usb_bbp_write(rt2x00dev, 31, 0x2b);
	rt2500usb_bbp_write(rt2x00dev, 32, 0xb9);
	rt2500usb_bbp_write(rt2x00dev, 34, 0x12);
	rt2500usb_bbp_write(rt2x00dev, 35, 0x50);
	rt2500usb_bbp_write(rt2x00dev, 39, 0xc4);
	rt2500usb_bbp_write(rt2x00dev, 40, 0x02);
	rt2500usb_bbp_write(rt2x00dev, 41, 0x60);
	rt2500usb_bbp_write(rt2x00dev, 53, 0x10);
	rt2500usb_bbp_write(rt2x00dev, 54, 0x18);
	rt2500usb_bbp_write(rt2x00dev, 56, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 57, 0x10);
	rt2500usb_bbp_write(rt2x00dev, 58, 0x08);
	rt2500usb_bbp_write(rt2x00dev, 61, 0x60);
	rt2500usb_bbp_write(rt2x00dev, 62, 0x10);
	rt2500usb_bbp_write(rt2x00dev, 75, 0xff);

	DEBUG(rt2x00dev, "Start initialization from EEPROM...\n");
	for (i = 0; i < EEPROM_BBP_SIZE; i++) {
		rt2x00_eeprom_read(rt2x00dev, EEPROM_BBP_START + i, &eeprom);

		if (eeprom != 0xffff && eeprom != 0x0000) {
			reg_id = rt2x00_get_field16(eeprom, EEPROM_BBP_REG_ID);
			value = rt2x00_get_field16(eeprom, EEPROM_BBP_VALUE);
			DEBUG(rt2x00dev, "BBP: 0x%02x, value: 0x%02x.\n",
			      reg_id, value);
			rt2500usb_bbp_write(rt2x00dev, reg_id, value);
		}
	}
	DEBUG(rt2x00dev, "...End initialization from EEPROM.\n");

	return 0;
}

/*
 * Device state switch handlers.
 */
static void rt2500usb_toggle_rx(struct rt2x00_dev *rt2x00dev,
				enum dev_state state)
{
	u16 reg;

	rt2500usb_register_read(rt2x00dev, TXRX_CSR2, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR2_DISABLE_RX,
			   state == STATE_RADIO_RX_OFF);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR2, reg);
}

static int rt2500usb_enable_radio(struct rt2x00_dev *rt2x00dev)
{
	/*
	 * Initialize all registers.
	 */
	if (rt2500usb_init_registers(rt2x00dev) ||
	    rt2500usb_init_bbp(rt2x00dev)) {
		ERROR(rt2x00dev, "Register initialization failed.\n");
		return -EIO;
	}

	return 0;
}

static void rt2500usb_disable_radio(struct rt2x00_dev *rt2x00dev)
{
	rt2500usb_register_write(rt2x00dev, MAC_CSR13, 0x2121);
	rt2500usb_register_write(rt2x00dev, MAC_CSR14, 0x2121);

	/*
	 * Disable synchronisation.
	 */
	rt2500usb_register_write(rt2x00dev, TXRX_CSR19, 0);

	rt2x00usb_disable_radio(rt2x00dev);
}

static int rt2500usb_set_state(struct rt2x00_dev *rt2x00dev,
			       enum dev_state state)
{
	u16 reg;
	u16 reg2;
	unsigned int i;
	char put_to_sleep;
	char bbp_state;
	char rf_state;

	put_to_sleep = (state != STATE_AWAKE);

	reg = 0;
	rt2x00_set_field16(&reg, MAC_CSR17_BBP_DESIRE_STATE, state);
	rt2x00_set_field16(&reg, MAC_CSR17_RF_DESIRE_STATE, state);
	rt2x00_set_field16(&reg, MAC_CSR17_PUT_TO_SLEEP, put_to_sleep);
	rt2500usb_register_write(rt2x00dev, MAC_CSR17, reg);
	rt2x00_set_field16(&reg, MAC_CSR17_SET_STATE, 1);
	rt2500usb_register_write(rt2x00dev, MAC_CSR17, reg);

	/*
	 * Device is not guaranteed to be in the requested state yet.
	 * We must wait until the register indicates that the
	 * device has entered the correct state.
	 */
	for (i = 0; i < REGISTER_BUSY_COUNT; i++) {
		rt2500usb_register_read(rt2x00dev, MAC_CSR17, &reg2);
		bbp_state = rt2x00_get_field16(reg2, MAC_CSR17_BBP_CURR_STATE);
		rf_state = rt2x00_get_field16(reg2, MAC_CSR17_RF_CURR_STATE);
		if (bbp_state == state && rf_state == state)
			return 0;
		rt2500usb_register_write(rt2x00dev, MAC_CSR17, reg);
		msleep(30);
	}

	NOTICE(rt2x00dev, "Device failed to enter state %d, "
	       "current device state: bbp %d and rf %d.\n",
	       state, bbp_state, rf_state);

	return -EBUSY;
}

static int rt2500usb_set_device_state(struct rt2x00_dev *rt2x00dev,
				      enum dev_state state)
{
	int retval = 0;

	switch (state) {
	case STATE_RADIO_ON:
		retval = rt2500usb_enable_radio(rt2x00dev);
		break;
	case STATE_RADIO_OFF:
		rt2500usb_disable_radio(rt2x00dev);
		break;
	case STATE_RADIO_RX_ON:
	case STATE_RADIO_RX_ON_LINK:
		rt2500usb_toggle_rx(rt2x00dev, STATE_RADIO_RX_ON);
		break;
	case STATE_RADIO_RX_OFF:
	case STATE_RADIO_RX_OFF_LINK:
		rt2500usb_toggle_rx(rt2x00dev, STATE_RADIO_RX_OFF);
		break;
	case STATE_DEEP_SLEEP:
	case STATE_SLEEP:
	case STATE_STANDBY:
	case STATE_AWAKE:
		retval = rt2500usb_set_state(rt2x00dev, state);
		break;
	default:
		retval = -ENOTSUPP;
		break;
	}

	return retval;
}

/*
 * TX descriptor initialization
 */
static void rt2500usb_write_tx_desc(struct rt2x00_dev *rt2x00dev,
				    struct sk_buff *skb,
				    struct txentry_desc *txdesc,
				    struct ieee80211_tx_control *control)
{
	struct skb_frame_desc *skbdesc = get_skb_frame_desc(skb);
	__le32 *txd = skbdesc->desc;
	u32 word;

	/*
	 * Start writing the descriptor words.
	 */
	rt2x00_desc_read(txd, 1, &word);
	rt2x00_set_field32(&word, TXD_W1_IV_OFFSET, IEEE80211_HEADER);
	rt2x00_set_field32(&word, TXD_W1_AIFS, txdesc->aifs);
	rt2x00_set_field32(&word, TXD_W1_CWMIN, txdesc->cw_min);
	rt2x00_set_field32(&word, TXD_W1_CWMAX, txdesc->cw_max);
	rt2x00_desc_write(txd, 1, word);

	rt2x00_desc_read(txd, 2, &word);
	rt2x00_set_field32(&word, TXD_W2_PLCP_SIGNAL, txdesc->signal);
	rt2x00_set_field32(&word, TXD_W2_PLCP_SERVICE, txdesc->service);
	rt2x00_set_field32(&word, TXD_W2_PLCP_LENGTH_LOW, txdesc->length_low);
	rt2x00_set_field32(&word, TXD_W2_PLCP_LENGTH_HIGH, txdesc->length_high);
	rt2x00_desc_write(txd, 2, word);

	rt2x00_desc_read(txd, 0, &word);
	rt2x00_set_field32(&word, TXD_W0_RETRY_LIMIT, control->retry_limit);
	rt2x00_set_field32(&word, TXD_W0_MORE_FRAG,
			   test_bit(ENTRY_TXD_MORE_FRAG, &txdesc->flags));
	rt2x00_set_field32(&word, TXD_W0_ACK,
			   test_bit(ENTRY_TXD_ACK, &txdesc->flags));
	rt2x00_set_field32(&word, TXD_W0_TIMESTAMP,
			   test_bit(ENTRY_TXD_REQ_TIMESTAMP, &txdesc->flags));
	rt2x00_set_field32(&word, TXD_W0_OFDM,
			   test_bit(ENTRY_TXD_OFDM_RATE, &txdesc->flags));
	rt2x00_set_field32(&word, TXD_W0_NEW_SEQ,
			   !!(control->flags & IEEE80211_TXCTL_FIRST_FRAGMENT));
	rt2x00_set_field32(&word, TXD_W0_IFS, txdesc->ifs);
	rt2x00_set_field32(&word, TXD_W0_DATABYTE_COUNT, skbdesc->data_len);
	rt2x00_set_field32(&word, TXD_W0_CIPHER, CIPHER_NONE);
	rt2x00_desc_write(txd, 0, word);
}

static int rt2500usb_get_tx_data_len(struct rt2x00_dev *rt2x00dev,
				     struct sk_buff *skb)
{
	int length;

	/*
	 * The length _must_ be a multiple of 2,
	 * but it must _not_ be a multiple of the USB packet size.
	 */
	length = roundup(skb->len, 2);
	length += (2 * !(length % rt2x00dev->usb_maxpacket));

	return length;
}

/*
 * TX data initialization
 */
static void rt2500usb_kick_tx_queue(struct rt2x00_dev *rt2x00dev,
				    const unsigned int queue)
{
	u16 reg;

	if (queue != RT2X00_BCN_QUEUE_BEACON)
		return;

	rt2500usb_register_read(rt2x00dev, TXRX_CSR19, &reg);
	if (!rt2x00_get_field16(reg, TXRX_CSR19_BEACON_GEN)) {
		rt2x00_set_field16(&reg, TXRX_CSR19_BEACON_GEN, 1);
		/*
		 * Beacon generation will fail initially.
		 * To prevent this we need to register the TXRX_CSR19
		 * register several times.
		 */
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, reg);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, 0);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, reg);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, 0);
		rt2500usb_register_write(rt2x00dev, TXRX_CSR19, reg);
	}
}

/*
 * RX control handlers
 */
static void rt2500usb_fill_rxdone(struct queue_entry *entry,
				  struct rxdone_entry_desc *rxdesc)
{
	struct queue_entry_priv_usb_rx *priv_rx = entry->priv_data;
	struct skb_frame_desc *skbdesc = get_skb_frame_desc(entry->skb);
	__le32 *rxd =
	    (__le32 *)(entry->skb->data +
		       (priv_rx->urb->actual_length - entry->queue->desc_size));
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)entry->skb->data;
	int header_size = ieee80211_get_hdrlen(le16_to_cpu(hdr->frame_control));
	u32 word0;
	u32 word1;

	rt2x00_desc_read(rxd, 0, &word0);
	rt2x00_desc_read(rxd, 1, &word1);

	rxdesc->flags = 0;
	if (rt2x00_get_field32(word0, RXD_W0_CRC_ERROR))
		rxdesc->flags |= RX_FLAG_FAILED_FCS_CRC;
	if (rt2x00_get_field32(word0, RXD_W0_PHYSICAL_ERROR))
		rxdesc->flags |= RX_FLAG_FAILED_PLCP_CRC;

	/*
	 * Obtain the status about this packet.
	 */
	rxdesc->signal = rt2x00_get_field32(word1, RXD_W1_SIGNAL);
	rxdesc->rssi = rt2x00_get_field32(word1, RXD_W1_RSSI) -
	    entry->queue->rt2x00dev->rssi_offset;
	rxdesc->ofdm = rt2x00_get_field32(word0, RXD_W0_OFDM);
	rxdesc->size = rt2x00_get_field32(word0, RXD_W0_DATABYTE_COUNT);
	rxdesc->my_bss = !!rt2x00_get_field32(word0, RXD_W0_MY_BSS);

	/*
	 * The data behind the ieee80211 header must be
	 * aligned on a 4 byte boundary.
	 */
	if (header_size % 4 == 0) {
		skb_push(entry->skb, 2);
		memmove(entry->skb->data, entry->skb->data + 2,
			entry->skb->len - 2);
	}

	/*
	 * Set descriptor pointer.
	 */
	skbdesc->data = entry->skb->data;
	skbdesc->data_len = entry->queue->data_size;
	skbdesc->desc = entry->skb->data + rxdesc->size;
	skbdesc->desc_len = entry->queue->desc_size;

	/*
	 * Remove descriptor from skb buffer and trim the whole thing
	 * down to only contain data.
	 */
	skb_trim(entry->skb, rxdesc->size);
}

/*
 * Interrupt functions.
 */
static void rt2500usb_beacondone(struct urb *urb)
{
	struct queue_entry *entry = (struct queue_entry *)urb->context;
	struct queue_entry_priv_usb_bcn *priv_bcn = entry->priv_data;

	if (!test_bit(DEVICE_ENABLED_RADIO, &entry->queue->rt2x00dev->flags))
		return;

	/*
	 * Check if this was the guardian beacon,
	 * if that was the case we need to send the real beacon now.
	 * Otherwise we should free the sk_buffer, the device
	 * should be doing the rest of the work now.
	 */
	if (priv_bcn->guardian_urb == urb) {
		usb_submit_urb(priv_bcn->urb, GFP_ATOMIC);
	} else if (priv_bcn->urb == urb) {
		dev_kfree_skb(entry->skb);
		entry->skb = NULL;
	}
}

/*
 * Device probe functions.
 */
static int rt2500usb_validate_eeprom(struct rt2x00_dev *rt2x00dev)
{
	u16 word;
	u8 *mac;
	u8 bbp;

	rt2x00usb_eeprom_read(rt2x00dev, rt2x00dev->eeprom, EEPROM_SIZE);

	/*
	 * Start validation of the data that has been read.
	 */
	mac = rt2x00_eeprom_addr(rt2x00dev, EEPROM_MAC_ADDR_0);
	if (!is_valid_ether_addr(mac)) {
		DECLARE_MAC_BUF(macbuf);

		random_ether_addr(mac);
		EEPROM(rt2x00dev, "MAC: %s\n", print_mac(macbuf, mac));
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_ANTENNA, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_ANTENNA_NUM, 2);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_TX_DEFAULT,
				   ANTENNA_SW_DIVERSITY);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_RX_DEFAULT,
				   ANTENNA_SW_DIVERSITY);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_LED_MODE,
				   LED_MODE_DEFAULT);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_DYN_TXAGC, 0);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_HARDWARE_RADIO, 0);
		rt2x00_set_field16(&word, EEPROM_ANTENNA_RF_TYPE, RF2522);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_ANTENNA, word);
		EEPROM(rt2x00dev, "Antenna: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_NIC, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_NIC_CARDBUS_ACCEL, 0);
		rt2x00_set_field16(&word, EEPROM_NIC_DYN_BBP_TUNE, 0);
		rt2x00_set_field16(&word, EEPROM_NIC_CCK_TX_POWER, 0);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_NIC, word);
		EEPROM(rt2x00dev, "NIC: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_CALIBRATE_OFFSET, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_CALIBRATE_OFFSET_RSSI,
				   DEFAULT_RSSI_OFFSET);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_CALIBRATE_OFFSET, word);
		EEPROM(rt2x00dev, "Calibrate offset: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_THRESHOLD, 45);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE, word);
		EEPROM(rt2x00dev, "BBPtune: 0x%04x\n", word);
	}

	/*
	 * Switch lower vgc bound to current BBP R17 value,
	 * lower the value a bit for better quality.
	 */
	rt2500usb_bbp_read(rt2x00dev, 17, &bbp);
	bbp -= 6;

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_VGC, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_VGCUPPER, 0x40);
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_VGCLOWER, bbp);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_VGC, word);
		EEPROM(rt2x00dev, "BBPtune vgc: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R17, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R17_LOW, 0x48);
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R17_HIGH, 0x41);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_R17, word);
		EEPROM(rt2x00dev, "BBPtune r17: 0x%04x\n", word);
	} else {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_VGCLOWER, bbp);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_VGC, word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R24, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R24_LOW, 0x40);
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R24_HIGH, 0x80);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_R24, word);
		EEPROM(rt2x00dev, "BBPtune r24: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R25, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R25_LOW, 0x40);
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R25_HIGH, 0x50);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_R25, word);
		EEPROM(rt2x00dev, "BBPtune r25: 0x%04x\n", word);
	}

	rt2x00_eeprom_read(rt2x00dev, EEPROM_BBPTUNE_R61, &word);
	if (word == 0xffff) {
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R61_LOW, 0x60);
		rt2x00_set_field16(&word, EEPROM_BBPTUNE_R61_HIGH, 0x6d);
		rt2x00_eeprom_write(rt2x00dev, EEPROM_BBPTUNE_R61, word);
		EEPROM(rt2x00dev, "BBPtune r61: 0x%04x\n", word);
	}

	return 0;
}

static int rt2500usb_init_eeprom(struct rt2x00_dev *rt2x00dev)
{
	u16 reg;
	u16 value;
	u16 eeprom;

	/*
	 * Read EEPROM word for configuration.
	 */
	rt2x00_eeprom_read(rt2x00dev, EEPROM_ANTENNA, &eeprom);

	/*
	 * Identify RF chipset.
	 */
	value = rt2x00_get_field16(eeprom, EEPROM_ANTENNA_RF_TYPE);
	rt2500usb_register_read(rt2x00dev, MAC_CSR0, &reg);
	rt2x00_set_chip(rt2x00dev, RT2570, value, reg);

	if (!rt2x00_check_rev(&rt2x00dev->chip, 0)) {
		ERROR(rt2x00dev, "Invalid RT chipset detected.\n");
		return -ENODEV;
	}

	if (!rt2x00_rf(&rt2x00dev->chip, RF2522) &&
	    !rt2x00_rf(&rt2x00dev->chip, RF2523) &&
	    !rt2x00_rf(&rt2x00dev->chip, RF2524) &&
	    !rt2x00_rf(&rt2x00dev->chip, RF2525) &&
	    !rt2x00_rf(&rt2x00dev->chip, RF2525E) &&
	    !rt2x00_rf(&rt2x00dev->chip, RF5222)) {
		ERROR(rt2x00dev, "Invalid RF chipset detected.\n");
		return -ENODEV;
	}

	/*
	 * Identify default antenna configuration.
	 */
	rt2x00dev->default_ant.tx =
	    rt2x00_get_field16(eeprom, EEPROM_ANTENNA_TX_DEFAULT);
	rt2x00dev->default_ant.rx =
	    rt2x00_get_field16(eeprom, EEPROM_ANTENNA_RX_DEFAULT);

	/*
	 * When the eeprom indicates SW_DIVERSITY use HW_DIVERSITY instead.
	 * I am not 100% sure about this, but the legacy drivers do not
	 * indicate antenna swapping in software is required when
	 * diversity is enabled.
	 */
	if (rt2x00dev->default_ant.tx == ANTENNA_SW_DIVERSITY)
		rt2x00dev->default_ant.tx = ANTENNA_HW_DIVERSITY;
	if (rt2x00dev->default_ant.rx == ANTENNA_SW_DIVERSITY)
		rt2x00dev->default_ant.rx = ANTENNA_HW_DIVERSITY;

	/*
	 * Store led mode, for correct led behaviour.
	 */
#ifdef CONFIG_RT2500USB_LEDS
	value = rt2x00_get_field16(eeprom, EEPROM_ANTENNA_LED_MODE);

	switch (value) {
	case LED_MODE_ASUS:
	case LED_MODE_ALPHA:
	case LED_MODE_DEFAULT:
		rt2x00dev->led_flags = LED_SUPPORT_RADIO;
		break;
	case LED_MODE_TXRX_ACTIVITY:
		rt2x00dev->led_flags =
		    LED_SUPPORT_RADIO | LED_SUPPORT_ACTIVITY;
		break;
	case LED_MODE_SIGNAL_STRENGTH:
		rt2x00dev->led_flags = LED_SUPPORT_RADIO;
		break;
	}
#endif /* CONFIG_RT2500USB_LEDS */

	/*
	 * Check if the BBP tuning should be disabled.
	 */
	rt2x00_eeprom_read(rt2x00dev, EEPROM_NIC, &eeprom);
	if (rt2x00_get_field16(eeprom, EEPROM_NIC_DYN_BBP_TUNE))
		__set_bit(CONFIG_DISABLE_LINK_TUNING, &rt2x00dev->flags);

	/*
	 * Read the RSSI <-> dBm offset information.
	 */
	rt2x00_eeprom_read(rt2x00dev, EEPROM_CALIBRATE_OFFSET, &eeprom);
	rt2x00dev->rssi_offset =
	    rt2x00_get_field16(eeprom, EEPROM_CALIBRATE_OFFSET_RSSI);

	return 0;
}

/*
 * RF value list for RF2522
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2522[] = {
	{ 1,  0x00002050, 0x000c1fda, 0x00000101, 0 },
	{ 2,  0x00002050, 0x000c1fee, 0x00000101, 0 },
	{ 3,  0x00002050, 0x000c2002, 0x00000101, 0 },
	{ 4,  0x00002050, 0x000c2016, 0x00000101, 0 },
	{ 5,  0x00002050, 0x000c202a, 0x00000101, 0 },
	{ 6,  0x00002050, 0x000c203e, 0x00000101, 0 },
	{ 7,  0x00002050, 0x000c2052, 0x00000101, 0 },
	{ 8,  0x00002050, 0x000c2066, 0x00000101, 0 },
	{ 9,  0x00002050, 0x000c207a, 0x00000101, 0 },
	{ 10, 0x00002050, 0x000c208e, 0x00000101, 0 },
	{ 11, 0x00002050, 0x000c20a2, 0x00000101, 0 },
	{ 12, 0x00002050, 0x000c20b6, 0x00000101, 0 },
	{ 13, 0x00002050, 0x000c20ca, 0x00000101, 0 },
	{ 14, 0x00002050, 0x000c20fa, 0x00000101, 0 },
};

/*
 * RF value list for RF2523
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2523[] = {
	{ 1,  0x00022010, 0x00000c9e, 0x000e0111, 0x00000a1b },
	{ 2,  0x00022010, 0x00000ca2, 0x000e0111, 0x00000a1b },
	{ 3,  0x00022010, 0x00000ca6, 0x000e0111, 0x00000a1b },
	{ 4,  0x00022010, 0x00000caa, 0x000e0111, 0x00000a1b },
	{ 5,  0x00022010, 0x00000cae, 0x000e0111, 0x00000a1b },
	{ 6,  0x00022010, 0x00000cb2, 0x000e0111, 0x00000a1b },
	{ 7,  0x00022010, 0x00000cb6, 0x000e0111, 0x00000a1b },
	{ 8,  0x00022010, 0x00000cba, 0x000e0111, 0x00000a1b },
	{ 9,  0x00022010, 0x00000cbe, 0x000e0111, 0x00000a1b },
	{ 10, 0x00022010, 0x00000d02, 0x000e0111, 0x00000a1b },
	{ 11, 0x00022010, 0x00000d06, 0x000e0111, 0x00000a1b },
	{ 12, 0x00022010, 0x00000d0a, 0x000e0111, 0x00000a1b },
	{ 13, 0x00022010, 0x00000d0e, 0x000e0111, 0x00000a1b },
	{ 14, 0x00022010, 0x00000d1a, 0x000e0111, 0x00000a03 },
};

/*
 * RF value list for RF2524
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2524[] = {
	{ 1,  0x00032020, 0x00000c9e, 0x00000101, 0x00000a1b },
	{ 2,  0x00032020, 0x00000ca2, 0x00000101, 0x00000a1b },
	{ 3,  0x00032020, 0x00000ca6, 0x00000101, 0x00000a1b },
	{ 4,  0x00032020, 0x00000caa, 0x00000101, 0x00000a1b },
	{ 5,  0x00032020, 0x00000cae, 0x00000101, 0x00000a1b },
	{ 6,  0x00032020, 0x00000cb2, 0x00000101, 0x00000a1b },
	{ 7,  0x00032020, 0x00000cb6, 0x00000101, 0x00000a1b },
	{ 8,  0x00032020, 0x00000cba, 0x00000101, 0x00000a1b },
	{ 9,  0x00032020, 0x00000cbe, 0x00000101, 0x00000a1b },
	{ 10, 0x00032020, 0x00000d02, 0x00000101, 0x00000a1b },
	{ 11, 0x00032020, 0x00000d06, 0x00000101, 0x00000a1b },
	{ 12, 0x00032020, 0x00000d0a, 0x00000101, 0x00000a1b },
	{ 13, 0x00032020, 0x00000d0e, 0x00000101, 0x00000a1b },
	{ 14, 0x00032020, 0x00000d1a, 0x00000101, 0x00000a03 },
};

/*
 * RF value list for RF2525
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2525[] = {
	{ 1,  0x00022020, 0x00080c9e, 0x00060111, 0x00000a1b },
	{ 2,  0x00022020, 0x00080ca2, 0x00060111, 0x00000a1b },
	{ 3,  0x00022020, 0x00080ca6, 0x00060111, 0x00000a1b },
	{ 4,  0x00022020, 0x00080caa, 0x00060111, 0x00000a1b },
	{ 5,  0x00022020, 0x00080cae, 0x00060111, 0x00000a1b },
	{ 6,  0x00022020, 0x00080cb2, 0x00060111, 0x00000a1b },
	{ 7,  0x00022020, 0x00080cb6, 0x00060111, 0x00000a1b },
	{ 8,  0x00022020, 0x00080cba, 0x00060111, 0x00000a1b },
	{ 9,  0x00022020, 0x00080cbe, 0x00060111, 0x00000a1b },
	{ 10, 0x00022020, 0x00080d02, 0x00060111, 0x00000a1b },
	{ 11, 0x00022020, 0x00080d06, 0x00060111, 0x00000a1b },
	{ 12, 0x00022020, 0x00080d0a, 0x00060111, 0x00000a1b },
	{ 13, 0x00022020, 0x00080d0e, 0x00060111, 0x00000a1b },
	{ 14, 0x00022020, 0x00080d1a, 0x00060111, 0x00000a03 },
};

/*
 * RF value list for RF2525e
 * Supports: 2.4 GHz
 */
static const struct rf_channel rf_vals_bg_2525e[] = {
	{ 1,  0x00022010, 0x0000089a, 0x00060111, 0x00000e1b },
	{ 2,  0x00022010, 0x0000089e, 0x00060111, 0x00000e07 },
	{ 3,  0x00022010, 0x0000089e, 0x00060111, 0x00000e1b },
	{ 4,  0x00022010, 0x000008a2, 0x00060111, 0x00000e07 },
	{ 5,  0x00022010, 0x000008a2, 0x00060111, 0x00000e1b },
	{ 6,  0x00022010, 0x000008a6, 0x00060111, 0x00000e07 },
	{ 7,  0x00022010, 0x000008a6, 0x00060111, 0x00000e1b },
	{ 8,  0x00022010, 0x000008aa, 0x00060111, 0x00000e07 },
	{ 9,  0x00022010, 0x000008aa, 0x00060111, 0x00000e1b },
	{ 10, 0x00022010, 0x000008ae, 0x00060111, 0x00000e07 },
	{ 11, 0x00022010, 0x000008ae, 0x00060111, 0x00000e1b },
	{ 12, 0x00022010, 0x000008b2, 0x00060111, 0x00000e07 },
	{ 13, 0x00022010, 0x000008b2, 0x00060111, 0x00000e1b },
	{ 14, 0x00022010, 0x000008b6, 0x00060111, 0x00000e23 },
};

/*
 * RF value list for RF5222
 * Supports: 2.4 GHz & 5.2 GHz
 */
static const struct rf_channel rf_vals_5222[] = {
	{ 1,  0x00022020, 0x00001136, 0x00000101, 0x00000a0b },
	{ 2,  0x00022020, 0x0000113a, 0x00000101, 0x00000a0b },
	{ 3,  0x00022020, 0x0000113e, 0x00000101, 0x00000a0b },
	{ 4,  0x00022020, 0x00001182, 0x00000101, 0x00000a0b },
	{ 5,  0x00022020, 0x00001186, 0x00000101, 0x00000a0b },
	{ 6,  0x00022020, 0x0000118a, 0x00000101, 0x00000a0b },
	{ 7,  0x00022020, 0x0000118e, 0x00000101, 0x00000a0b },
	{ 8,  0x00022020, 0x00001192, 0x00000101, 0x00000a0b },
	{ 9,  0x00022020, 0x00001196, 0x00000101, 0x00000a0b },
	{ 10, 0x00022020, 0x0000119a, 0x00000101, 0x00000a0b },
	{ 11, 0x00022020, 0x0000119e, 0x00000101, 0x00000a0b },
	{ 12, 0x00022020, 0x000011a2, 0x00000101, 0x00000a0b },
	{ 13, 0x00022020, 0x000011a6, 0x00000101, 0x00000a0b },
	{ 14, 0x00022020, 0x000011ae, 0x00000101, 0x00000a1b },

	/* 802.11 UNI / HyperLan 2 */
	{ 36, 0x00022010, 0x00018896, 0x00000101, 0x00000a1f },
	{ 40, 0x00022010, 0x0001889a, 0x00000101, 0x00000a1f },
	{ 44, 0x00022010, 0x0001889e, 0x00000101, 0x00000a1f },
	{ 48, 0x00022010, 0x000188a2, 0x00000101, 0x00000a1f },
	{ 52, 0x00022010, 0x000188a6, 0x00000101, 0x00000a1f },
	{ 66, 0x00022010, 0x000188aa, 0x00000101, 0x00000a1f },
	{ 60, 0x00022010, 0x000188ae, 0x00000101, 0x00000a1f },
	{ 64, 0x00022010, 0x000188b2, 0x00000101, 0x00000a1f },

	/* 802.11 HyperLan 2 */
	{ 100, 0x00022010, 0x00008802, 0x00000101, 0x00000a0f },
	{ 104, 0x00022010, 0x00008806, 0x00000101, 0x00000a0f },
	{ 108, 0x00022010, 0x0000880a, 0x00000101, 0x00000a0f },
	{ 112, 0x00022010, 0x0000880e, 0x00000101, 0x00000a0f },
	{ 116, 0x00022010, 0x00008812, 0x00000101, 0x00000a0f },
	{ 120, 0x00022010, 0x00008816, 0x00000101, 0x00000a0f },
	{ 124, 0x00022010, 0x0000881a, 0x00000101, 0x00000a0f },
	{ 128, 0x00022010, 0x0000881e, 0x00000101, 0x00000a0f },
	{ 132, 0x00022010, 0x00008822, 0x00000101, 0x00000a0f },
	{ 136, 0x00022010, 0x00008826, 0x00000101, 0x00000a0f },

	/* 802.11 UNII */
	{ 140, 0x00022010, 0x0000882a, 0x00000101, 0x00000a0f },
	{ 149, 0x00022020, 0x000090a6, 0x00000101, 0x00000a07 },
	{ 153, 0x00022020, 0x000090ae, 0x00000101, 0x00000a07 },
	{ 157, 0x00022020, 0x000090b6, 0x00000101, 0x00000a07 },
	{ 161, 0x00022020, 0x000090be, 0x00000101, 0x00000a07 },
};

static void rt2500usb_probe_hw_mode(struct rt2x00_dev *rt2x00dev)
{
	struct hw_mode_spec *spec = &rt2x00dev->spec;
	u8 *txpower;
	unsigned int i;

	/*
	 * Initialize all hw fields.
	 */
	rt2x00dev->hw->flags =
	    IEEE80211_HW_HOST_GEN_BEACON_TEMPLATE |
	    IEEE80211_HW_RX_INCLUDES_FCS |
	    IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING;
	rt2x00dev->hw->extra_tx_headroom = TXD_DESC_SIZE;
	rt2x00dev->hw->max_signal = MAX_SIGNAL;
	rt2x00dev->hw->max_rssi = MAX_RX_SSI;
	rt2x00dev->hw->queues = 2;

	SET_IEEE80211_DEV(rt2x00dev->hw, &rt2x00dev_usb(rt2x00dev)->dev);
	SET_IEEE80211_PERM_ADDR(rt2x00dev->hw,
				rt2x00_eeprom_addr(rt2x00dev,
						   EEPROM_MAC_ADDR_0));

	/*
	 * Convert tx_power array in eeprom.
	 */
	txpower = rt2x00_eeprom_addr(rt2x00dev, EEPROM_TXPOWER_START);
	for (i = 0; i < 14; i++)
		txpower[i] = TXPOWER_FROM_DEV(txpower[i]);

	/*
	 * Initialize hw_mode information.
	 */
	spec->num_modes = 2;
	spec->num_rates = 12;
	spec->tx_power_a = NULL;
	spec->tx_power_bg = txpower;
	spec->tx_power_default = DEFAULT_TXPOWER;

	if (rt2x00_rf(&rt2x00dev->chip, RF2522)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_bg_2522);
		spec->channels = rf_vals_bg_2522;
	} else if (rt2x00_rf(&rt2x00dev->chip, RF2523)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_bg_2523);
		spec->channels = rf_vals_bg_2523;
	} else if (rt2x00_rf(&rt2x00dev->chip, RF2524)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_bg_2524);
		spec->channels = rf_vals_bg_2524;
	} else if (rt2x00_rf(&rt2x00dev->chip, RF2525)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_bg_2525);
		spec->channels = rf_vals_bg_2525;
	} else if (rt2x00_rf(&rt2x00dev->chip, RF2525E)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_bg_2525e);
		spec->channels = rf_vals_bg_2525e;
	} else if (rt2x00_rf(&rt2x00dev->chip, RF5222)) {
		spec->num_channels = ARRAY_SIZE(rf_vals_5222);
		spec->channels = rf_vals_5222;
		spec->num_modes = 3;
	}
}

static int rt2500usb_probe_hw(struct rt2x00_dev *rt2x00dev)
{
	int retval;

	/*
	 * Allocate eeprom data.
	 */
	retval = rt2500usb_validate_eeprom(rt2x00dev);
	if (retval)
		return retval;

	retval = rt2500usb_init_eeprom(rt2x00dev);
	if (retval)
		return retval;

	/*
	 * Initialize hw specifications.
	 */
	rt2500usb_probe_hw_mode(rt2x00dev);

	/*
	 * This device requires the atim queue
	 */
	__set_bit(DRIVER_REQUIRE_ATIM_QUEUE, &rt2x00dev->flags);
	__set_bit(DRIVER_REQUIRE_BEACON_GUARD, &rt2x00dev->flags);

	/*
	 * Set the rssi offset.
	 */
	rt2x00dev->rssi_offset = DEFAULT_RSSI_OFFSET;

	return 0;
}

/*
 * IEEE80211 stack callback functions.
 */
static void rt2500usb_configure_filter(struct ieee80211_hw *hw,
				       unsigned int changed_flags,
				       unsigned int *total_flags,
				       int mc_count,
				       struct dev_addr_list *mc_list)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	u16 reg;

	/*
	 * Mask off any flags we are going to ignore from
	 * the total_flags field.
	 */
	*total_flags &=
	    FIF_ALLMULTI |
	    FIF_FCSFAIL |
	    FIF_PLCPFAIL |
	    FIF_CONTROL |
	    FIF_OTHER_BSS |
	    FIF_PROMISC_IN_BSS;

	/*
	 * Apply some rules to the filters:
	 * - Some filters imply different filters to be set.
	 * - Some things we can't filter out at all.
	 */
	if (mc_count)
		*total_flags |= FIF_ALLMULTI;
	if (*total_flags & FIF_OTHER_BSS ||
	    *total_flags & FIF_PROMISC_IN_BSS)
		*total_flags |= FIF_PROMISC_IN_BSS | FIF_OTHER_BSS;

	/*
	 * Check if there is any work left for us.
	 */
	if (rt2x00dev->packet_filter == *total_flags)
		return;
	rt2x00dev->packet_filter = *total_flags;

	/*
	 * When in atomic context, reschedule and let rt2x00lib
	 * call this function again.
	 */
	if (in_atomic()) {
		queue_work(rt2x00dev->hw->workqueue, &rt2x00dev->filter_work);
		return;
	}

	/*
	 * Start configuration steps.
	 * Note that the version error will always be dropped
	 * and broadcast frames will always be accepted since
	 * there is no filter for it at this time.
	 */
	rt2500usb_register_read(rt2x00dev, TXRX_CSR2, &reg);
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_CRC,
			   !(*total_flags & FIF_FCSFAIL));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_PHYSICAL,
			   !(*total_flags & FIF_PLCPFAIL));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_CONTROL,
			   !(*total_flags & FIF_CONTROL));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_NOT_TO_ME,
			   !(*total_flags & FIF_PROMISC_IN_BSS));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_TODS,
			   !(*total_flags & FIF_PROMISC_IN_BSS));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_VERSION_ERROR, 1);
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_MULTICAST,
			   !(*total_flags & FIF_ALLMULTI));
	rt2x00_set_field16(&reg, TXRX_CSR2_DROP_BROADCAST, 0);
	rt2500usb_register_write(rt2x00dev, TXRX_CSR2, reg);
}

static int rt2500usb_beacon_update(struct ieee80211_hw *hw,
				   struct sk_buff *skb,
				   struct ieee80211_tx_control *control)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct usb_device *usb_dev = rt2x00dev_usb_dev(rt2x00dev);
	struct rt2x00_intf *intf = vif_to_intf(control->vif);
	struct queue_entry_priv_usb_bcn *priv_bcn;
	struct skb_frame_desc *skbdesc;
	int pipe = usb_sndbulkpipe(usb_dev, 1);
	int length;

	if (unlikely(!intf->beacon))
		return -ENOBUFS;

	priv_bcn = intf->beacon->priv_data;

	/*
	 * Add the descriptor in front of the skb.
	 */
	skb_push(skb, intf->beacon->queue->desc_size);
	memset(skb->data, 0, intf->beacon->queue->desc_size);

	/*
	 * Fill in skb descriptor
	 */
	skbdesc = get_skb_frame_desc(skb);
	memset(skbdesc, 0, sizeof(*skbdesc));
	skbdesc->data = skb->data + intf->beacon->queue->desc_size;
	skbdesc->data_len = skb->len - intf->beacon->queue->desc_size;
	skbdesc->desc = skb->data;
	skbdesc->desc_len = intf->beacon->queue->desc_size;
	skbdesc->entry = intf->beacon;

	/*
	 * mac80211 doesn't provide the control->queue variable
	 * for beacons. Set our own queue identification so
	 * it can be used during descriptor initialization.
	 */
	control->queue = RT2X00_BCN_QUEUE_BEACON;
	rt2x00lib_write_tx_desc(rt2x00dev, skb, control);

	/*
	 * USB devices cannot blindly pass the skb->len as the
	 * length of the data to usb_fill_bulk_urb. Pass the skb
	 * to the driver to determine what the length should be.
	 */
	length = rt2500usb_get_tx_data_len(rt2x00dev, skb);

	usb_fill_bulk_urb(priv_bcn->urb, usb_dev, pipe,
			  skb->data, length, rt2500usb_beacondone,
			  intf->beacon);

	/*
	 * Second we need to create the guardian byte.
	 * We only need a single byte, so lets recycle
	 * the 'flags' field we are not using for beacons.
	 */
	priv_bcn->guardian_data = 0;
	usb_fill_bulk_urb(priv_bcn->guardian_urb, usb_dev, pipe,
			  &priv_bcn->guardian_data, 1, rt2500usb_beacondone,
			  intf->beacon);

	/*
	 * Send out the guardian byte.
	 */
	usb_submit_urb(priv_bcn->guardian_urb, GFP_ATOMIC);

	/*
	 * Enable beacon generation.
	 */
	rt2500usb_kick_tx_queue(rt2x00dev, control->queue);

	return 0;
}

static const struct ieee80211_ops rt2500usb_mac80211_ops = {
	.tx			= rt2x00mac_tx,
	.start			= rt2x00mac_start,
	.stop			= rt2x00mac_stop,
	.add_interface		= rt2x00mac_add_interface,
	.remove_interface	= rt2x00mac_remove_interface,
	.config			= rt2x00mac_config,
	.config_interface	= rt2x00mac_config_interface,
	.configure_filter	= rt2500usb_configure_filter,
	.get_stats		= rt2x00mac_get_stats,
	.bss_info_changed	= rt2x00mac_bss_info_changed,
	.conf_tx		= rt2x00mac_conf_tx,
	.get_tx_stats		= rt2x00mac_get_tx_stats,
	.beacon_update		= rt2500usb_beacon_update,
};

static const struct rt2x00lib_ops rt2500usb_rt2x00_ops = {
	.probe_hw		= rt2500usb_probe_hw,
	.initialize		= rt2x00usb_initialize,
	.uninitialize		= rt2x00usb_uninitialize,
	.init_rxentry		= rt2x00usb_init_rxentry,
	.init_txentry		= rt2x00usb_init_txentry,
	.set_device_state	= rt2500usb_set_device_state,
	.link_stats		= rt2500usb_link_stats,
	.reset_tuner		= rt2500usb_reset_tuner,
	.link_tuner		= rt2500usb_link_tuner,
	.led_brightness		= rt2500usb_led_brightness,
	.write_tx_desc		= rt2500usb_write_tx_desc,
	.write_tx_data		= rt2x00usb_write_tx_data,
	.get_tx_data_len	= rt2500usb_get_tx_data_len,
	.kick_tx_queue		= rt2500usb_kick_tx_queue,
	.fill_rxdone		= rt2500usb_fill_rxdone,
	.config_intf		= rt2500usb_config_intf,
	.config_preamble	= rt2500usb_config_preamble,
	.config			= rt2500usb_config,
};

static const struct data_queue_desc rt2500usb_queue_rx = {
	.entry_num		= RX_ENTRIES,
	.data_size		= DATA_FRAME_SIZE,
	.desc_size		= RXD_DESC_SIZE,
	.priv_size		= sizeof(struct queue_entry_priv_usb_rx),
};

static const struct data_queue_desc rt2500usb_queue_tx = {
	.entry_num		= TX_ENTRIES,
	.data_size		= DATA_FRAME_SIZE,
	.desc_size		= TXD_DESC_SIZE,
	.priv_size		= sizeof(struct queue_entry_priv_usb_tx),
};

static const struct data_queue_desc rt2500usb_queue_bcn = {
	.entry_num		= BEACON_ENTRIES,
	.data_size		= MGMT_FRAME_SIZE,
	.desc_size		= TXD_DESC_SIZE,
	.priv_size		= sizeof(struct queue_entry_priv_usb_bcn),
};

static const struct data_queue_desc rt2500usb_queue_atim = {
	.entry_num		= ATIM_ENTRIES,
	.data_size		= DATA_FRAME_SIZE,
	.desc_size		= TXD_DESC_SIZE,
	.priv_size		= sizeof(struct queue_entry_priv_usb_tx),
};

static const struct rt2x00_ops rt2500usb_ops = {
	.name		= KBUILD_MODNAME,
	.max_sta_intf	= 1,
	.max_ap_intf	= 1,
	.eeprom_size	= EEPROM_SIZE,
	.rf_size	= RF_SIZE,
	.rx		= &rt2500usb_queue_rx,
	.tx		= &rt2500usb_queue_tx,
	.bcn		= &rt2500usb_queue_bcn,
	.atim		= &rt2500usb_queue_atim,
	.lib		= &rt2500usb_rt2x00_ops,
	.hw		= &rt2500usb_mac80211_ops,
#ifdef CONFIG_RT2X00_LIB_DEBUGFS
	.debugfs	= &rt2500usb_rt2x00debug,
#endif /* CONFIG_RT2X00_LIB_DEBUGFS */
};

/*
 * rt2500usb module information.
 */
static struct usb_device_id rt2500usb_device_table[] = {
	/* ASUS */
	{ USB_DEVICE(0x0b05, 0x1706), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0b05, 0x1707), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Belkin */
	{ USB_DEVICE(0x050d, 0x7050), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x050d, 0x7051), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x050d, 0x705a), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Cisco Systems */
	{ USB_DEVICE(0x13b1, 0x000d), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x13b1, 0x0011), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x13b1, 0x001a), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Conceptronic */
	{ USB_DEVICE(0x14b2, 0x3c02), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* D-LINK */
	{ USB_DEVICE(0x2001, 0x3c00), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Gigabyte */
	{ USB_DEVICE(0x1044, 0x8001), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x1044, 0x8007), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Hercules */
	{ USB_DEVICE(0x06f8, 0xe000), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Melco */
	{ USB_DEVICE(0x0411, 0x005e), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0411, 0x0066), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0411, 0x0067), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0411, 0x008b), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0411, 0x0097), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* MSI */
	{ USB_DEVICE(0x0db0, 0x6861), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0db0, 0x6865), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x0db0, 0x6869), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Ralink */
	{ USB_DEVICE(0x148f, 0x1706), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x148f, 0x2570), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x148f, 0x2573), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ USB_DEVICE(0x148f, 0x9020), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Siemens */
	{ USB_DEVICE(0x0681, 0x3c06), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* SMC */
	{ USB_DEVICE(0x0707, 0xee13), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Spairon */
	{ USB_DEVICE(0x114b, 0x0110), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Trust */
	{ USB_DEVICE(0x0eb0, 0x9020), USB_DEVICE_DATA(&rt2500usb_ops) },
	/* Zinwell */
	{ USB_DEVICE(0x5a57, 0x0260), USB_DEVICE_DATA(&rt2500usb_ops) },
	{ 0, }
};

MODULE_AUTHOR(DRV_PROJECT);
MODULE_VERSION(DRV_VERSION);
MODULE_DESCRIPTION("Ralink RT2500 USB Wireless LAN driver.");
MODULE_SUPPORTED_DEVICE("Ralink RT2570 USB chipset based cards");
MODULE_DEVICE_TABLE(usb, rt2500usb_device_table);
MODULE_LICENSE("GPL");

static struct usb_driver rt2500usb_driver = {
	.name		= KBUILD_MODNAME,
	.id_table	= rt2500usb_device_table,
	.probe		= rt2x00usb_probe,
	.disconnect	= rt2x00usb_disconnect,
	.suspend	= rt2x00usb_suspend,
	.resume		= rt2x00usb_resume,
};

static int __init rt2500usb_init(void)
{
	return usb_register(&rt2500usb_driver);
}

static void __exit rt2500usb_exit(void)
{
	usb_deregister(&rt2500usb_driver);
}

module_init(rt2500usb_init);
module_exit(rt2500usb_exit);
