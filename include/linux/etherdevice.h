/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  NET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Ethernet handlers.
 *
 * Version:	@(#)eth.h	1.0.4	05/13/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		Relocated to include/linux where it belongs by Alan Cox 
 *							<gw4pts@gw4pts.ampr.org>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 */
#ifndef _LINUX_ETHERDEVICE_H
#define _LINUX_ETHERDEVICE_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <asm/unaligned.h>
#include <asm/bitsperlong.h>

#ifdef __KERNEL__
__be16 eth_type_trans(struct sk_buff *skb, struct net_device *dev);
extern const struct header_ops eth_header_ops;

int eth_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
	       const void *daddr, const void *saddr, unsigned len);
int eth_rebuild_header(struct sk_buff *skb);
int eth_header_parse(const struct sk_buff *skb, unsigned char *haddr);
int eth_header_cache(const struct neighbour *neigh, struct hh_cache *hh,
		     __be16 type);
void eth_header_cache_update(struct hh_cache *hh, const struct net_device *dev,
			     const unsigned char *haddr);
int eth_prepare_mac_addr_change(struct net_device *dev, void *p);
void eth_commit_mac_addr_change(struct net_device *dev, void *p);
int eth_mac_addr(struct net_device *dev, void *p);
int eth_change_mtu(struct net_device *dev, int new_mtu);
int eth_validate_addr(struct net_device *dev);

struct net_device *alloc_etherdev_mqs(int sizeof_priv, unsigned int txqs,
					    unsigned int rxqs);
#define alloc_etherdev(sizeof_priv) alloc_etherdev_mq(sizeof_priv, 1)
#define alloc_etherdev_mq(sizeof_priv, count) alloc_etherdev_mqs(sizeof_priv, count, count)

/* Reserved Ethernet Addresses per IEEE 802.1Q */
static const u8 eth_reserved_addr_base[ETH_ALEN] __aligned(2) =
{ 0x01, 0x80, 0xc2, 0x00, 0x00, 0x00 };

/**
 * is_link_local_ether_addr - Determine if given Ethernet address is link-local
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if address is link local reserved addr (01:80:c2:00:00:0X) per
 * IEEE 802.1Q 8.6.3 Frame filtering.
 */
static inline bool is_link_local_ether_addr(const u8 *addr)
{
	__be16 *a = (__be16 *)addr;
	static const __be16 *b = (const __be16 *)eth_reserved_addr_base;
	static const __be16 m = cpu_to_be16(0xfff0);

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | ((a[2] ^ b[2]) & m)) == 0;
}

/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
static inline bool is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline bool is_multicast_ether_addr(const u8 *addr)
{
	return 0x01 & addr[0];
}

/**
 * is_local_ether_addr - Determine if the Ethernet address is locally-assigned one (IEEE 802).
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a local address.
 */
static inline bool is_local_ether_addr(const u8 *addr)
{
	return 0x02 & addr[0];
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline bool is_broadcast_ether_addr(const u8 *addr)
{
	return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}

/**
 * is_unicast_ether_addr - Determine if the Ethernet address is unicast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a unicast address.
 */
static inline bool is_unicast_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr);
}

/**
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 */
static inline bool is_valid_ether_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

/**
 * eth_random_addr - Generate software assigned random Ethernet address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Generate a random Ethernet address (MAC) that is not multicast
 * and has the local assigned bit set.
 */
static inline void eth_random_addr(u8 *addr)
{
	get_random_bytes(addr, ETH_ALEN);
	addr[0] &= 0xfe;	/* clear multicast bit */
	addr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
}

#define random_ether_addr(addr) eth_random_addr(addr)

/**
 * eth_broadcast_addr - Assign broadcast address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Assign the broadcast address to the given address array.
 */
static inline void eth_broadcast_addr(u8 *addr)
{
	memset(addr, 0xff, ETH_ALEN);
}

/**
 * eth_zero_addr - Assign zero address
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Assign the zero address to the given address array.
 */
static inline void eth_zero_addr(u8 *addr)
{
	memset(addr, 0x00, ETH_ALEN);
}

/**
 * eth_hw_addr_random - Generate software assigned random Ethernet and
 * set device flag
 * @dev: pointer to net_device structure
 *
 * Generate a random Ethernet address (MAC) to be used by a net device
 * and set addr_assign_type so the state can be read by sysfs and be
 * used by userspace.
 */
static inline void eth_hw_addr_random(struct net_device *dev)
{
	dev->addr_assign_type = NET_ADDR_RANDOM;
	eth_random_addr(dev->dev_addr);
}

/**
 * eth_hw_addr_inherit - Copy dev_addr from another net_device
 * @dst: pointer to net_device to copy dev_addr to
 * @src: pointer to net_device to copy dev_addr from
 *
 * Copy the Ethernet address from one net_device to another along with
 * the address attributes (addr_assign_type).
 */
static inline void eth_hw_addr_inherit(struct net_device *dst,
				       struct net_device *src)
{
	dst->addr_assign_type = src->addr_assign_type;
	memcpy(dst->dev_addr, src->dev_addr, ETH_ALEN);
}

/**
 * ether_addr_equal - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two Ethernet addresses, returns true if equal
 *
 * Please note: addr1 & addr2 must both be aligned to u16.
 */
static inline bool ether_addr_equal(const u8 *addr1, const u8 *addr2)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	u32 fold = ((*(const u32 *)addr1) ^ (*(const u32 *)addr2)) |
		   ((*(const u16 *)(addr1 + 4)) ^ (*(const u16 *)(addr2 + 4)));

	return fold == 0;
#else
	const u16 *a = (const u16 *)addr1;
	const u16 *b = (const u16 *)addr2;

	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) == 0;
#endif
}

/**
 * ether_addr_equal_64bits - Compare two Ethernet addresses
 * @addr1: Pointer to an array of 8 bytes
 * @addr2: Pointer to an other array of 8 bytes
 *
 * Compare two Ethernet addresses, returns true if equal, false otherwise.
 *
 * The function doesn't need any conditional branches and possibly uses
 * word memory accesses on CPU allowing cheap unaligned memory reads.
 * arrays = { byte1, byte2, byte3, byte4, byte5, byte6, pad1, pad2 }
 *
 * Please note that alignment of addr1 & addr2 are only guaranteed to be 16 bits.
 */

static inline bool ether_addr_equal_64bits(const u8 addr1[6+2],
					   const u8 addr2[6+2])
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
	u64 fold = (*(const u64 *)addr1) ^ (*(const u64 *)addr2);

#ifdef __BIG_ENDIAN
	return (fold >> 16) == 0;
#else
	return (fold << 16) == 0;
#endif
#else
	return ether_addr_equal(addr1, addr2);
#endif
}

/**
 * is_etherdev_addr - Tell if given Ethernet address belongs to the device.
 * @dev: Pointer to a device structure
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Compare passed address with all addresses of the device. Return true if the
 * address if one of the device addresses.
 *
 * Note that this function calls ether_addr_equal_64bits() so take care of
 * the right padding.
 */
static inline bool is_etherdev_addr(const struct net_device *dev,
				    const u8 addr[6 + 2])
{
	struct netdev_hw_addr *ha;
	bool res = false;

	rcu_read_lock();
	for_each_dev_addr(dev, ha) {
		res = ether_addr_equal_64bits(addr, ha->addr);
		if (res)
			break;
	}
	rcu_read_unlock();
	return res;
}
#endif	/* __KERNEL__ */

/**
 * compare_ether_header - Compare two Ethernet headers
 * @a: Pointer to Ethernet header
 * @b: Pointer to Ethernet header
 *
 * Compare two Ethernet headers, returns 0 if equal.
 * This assumes that the network header (i.e., IP header) is 4-byte
 * aligned OR the platform can handle unaligned access.  This is the
 * case for all packets coming into netif_receive_skb or similar
 * entry points.
 */

static inline unsigned long compare_ether_header(const void *a, const void *b)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) && BITS_PER_LONG == 64
	unsigned long fold;

	/*
	 * We want to compare 14 bytes:
	 *  [a0 ... a13] ^ [b0 ... b13]
	 * Use two long XOR, ORed together, with an overlap of two bytes.
	 *  [a0  a1  a2  a3  a4  a5  a6  a7 ] ^ [b0  b1  b2  b3  b4  b5  b6  b7 ] |
	 *  [a6  a7  a8  a9  a10 a11 a12 a13] ^ [b6  b7  b8  b9  b10 b11 b12 b13]
	 * This means the [a6 a7] ^ [b6 b7] part is done two times.
	*/
	fold = *(unsigned long *)a ^ *(unsigned long *)b;
	fold |= *(unsigned long *)(a + 6) ^ *(unsigned long *)(b + 6);
	return fold;
#else
	u32 *a32 = (u32 *)((u8 *)a + 2);
	u32 *b32 = (u32 *)((u8 *)b + 2);

	return (*(u16 *)a ^ *(u16 *)b) | (a32[0] ^ b32[0]) |
	       (a32[1] ^ b32[1]) | (a32[2] ^ b32[2]);
#endif
}

#endif	/* _LINUX_ETHERDEVICE_H */
