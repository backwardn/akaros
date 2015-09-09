/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <linux/kernel.h>
#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/mlx4/driver.h>
#include <linux/mlx4/device.h>
#include <linux/in.h>
#include <net/ip.h>
#include <linux/bitmap.h>

#include "mlx4_en.h"
#include "en_port.h"

#define EN_ETHTOOL_QP_ATTACH (1ull << 63)
#define EN_ETHTOOL_SHORT_MASK cpu_to_be16(0xffff)
#define EN_ETHTOOL_WORD_MASK  cpu_to_be32(0xffffffff)

static int mlx4_en_moderation_update(struct mlx4_en_priv *priv)
{
	int i;
	int err = 0;

	for (i = 0; i < priv->tx_ring_num; i++) {
		priv->tx_cq[i]->moder_cnt = priv->tx_frames;
		priv->tx_cq[i]->moder_time = priv->tx_usecs;
		if (priv->port_up) {
			err = mlx4_en_set_cq_moder(priv, priv->tx_cq[i]);
			if (err)
				return err;
		}
	}

	if (priv->adaptive_rx_coal)
		return 0;

	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->rx_cq[i]->moder_cnt = priv->rx_frames;
		priv->rx_cq[i]->moder_time = priv->rx_usecs;
		priv->last_moder_time[i] = MLX4_EN_AUTO_CONF;
		if (priv->port_up) {
			err = mlx4_en_set_cq_moder(priv, priv->rx_cq[i]);
			if (err)
				return err;
		}
	}

	return err;
}

static void
mlx4_en_get_drvinfo(struct ether *dev, struct ethtool_drvinfo *drvinfo)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	strlcpy(drvinfo->driver, DRV_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->version, DRV_VERSION " (" DRV_RELDATE ")",
		sizeof(drvinfo->version));
	snprintf(drvinfo->fw_version, sizeof(drvinfo->fw_version),
		"%d.%d.%d",
		(u16) (mdev->dev->caps.fw_ver >> 32),
		(u16) ((mdev->dev->caps.fw_ver >> 16) & 0xffff),
		(u16) (mdev->dev->caps.fw_ver & 0xffff));
	strlcpy(drvinfo->bus_info, pci_name(mdev->dev->persist->pdev),
		sizeof(drvinfo->bus_info));
	drvinfo->n_stats = 0;
	drvinfo->regdump_len = 0;
	drvinfo->eedump_len = 0;
}

static const char mlx4_en_priv_flags[][ETH_GSTRING_LEN] = {
	"blueflame",
};

static const char main_strings[][ETH_GSTRING_LEN] = {
	/* main statistics */
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes", "rx_errors",
	"tx_errors", "rx_dropped", "tx_dropped", "multicast", "collisions",
	"rx_length_errors", "rx_over_errors", "rx_crc_errors",
	"rx_frame_errors", "rx_fifo_errors", "rx_missed_errors",
	"tx_aborted_errors", "tx_carrier_errors", "tx_fifo_errors",
	"tx_heartbeat_errors", "tx_window_errors",

	/* port statistics */
	"tso_packets",
	"xmit_more",
	"queue_stopped", "wake_queue", "tx_timeout", "rx_alloc_failed",
	"rx_csum_good", "rx_csum_none", "rx_csum_complete", "tx_chksum_offload",

	/* priority flow control statistics rx */
	"rx_pause_prio_0", "rx_pause_duration_prio_0",
	"rx_pause_transition_prio_0",
	"rx_pause_prio_1", "rx_pause_duration_prio_1",
	"rx_pause_transition_prio_1",
	"rx_pause_prio_2", "rx_pause_duration_prio_2",
	"rx_pause_transition_prio_2",
	"rx_pause_prio_3", "rx_pause_duration_prio_3",
	"rx_pause_transition_prio_3",
	"rx_pause_prio_4", "rx_pause_duration_prio_4",
	"rx_pause_transition_prio_4",
	"rx_pause_prio_5", "rx_pause_duration_prio_5",
	"rx_pause_transition_prio_5",
	"rx_pause_prio_6", "rx_pause_duration_prio_6",
	"rx_pause_transition_prio_6",
	"rx_pause_prio_7", "rx_pause_duration_prio_7",
	"rx_pause_transition_prio_7",

	/* flow control statistics rx */
	"rx_pause", "rx_pause_duration", "rx_pause_transition",

	/* priority flow control statistics tx */
	"tx_pause_prio_0", "tx_pause_duration_prio_0",
	"tx_pause_transition_prio_0",
	"tx_pause_prio_1", "tx_pause_duration_prio_1",
	"tx_pause_transition_prio_1",
	"tx_pause_prio_2", "tx_pause_duration_prio_2",
	"tx_pause_transition_prio_2",
	"tx_pause_prio_3", "tx_pause_duration_prio_3",
	"tx_pause_transition_prio_3",
	"tx_pause_prio_4", "tx_pause_duration_prio_4",
	"tx_pause_transition_prio_4",
	"tx_pause_prio_5", "tx_pause_duration_prio_5",
	"tx_pause_transition_prio_5",
	"tx_pause_prio_6", "tx_pause_duration_prio_6",
	"tx_pause_transition_prio_6",
	"tx_pause_prio_7", "tx_pause_duration_prio_7",
	"tx_pause_transition_prio_7",

	/* flow control statistics tx */
	"tx_pause", "tx_pause_duration", "tx_pause_transition",

	/* packet statistics */
	"rx_multicast_packets",
	"rx_broadcast_packets",
	"rx_jabbers",
	"rx_in_range_length_error",
	"rx_out_range_length_error",
	"tx_multicast_packets",
	"tx_broadcast_packets",
	"rx_prio_0_packets", "rx_prio_0_bytes",
	"rx_prio_1_packets", "rx_prio_1_bytes",
	"rx_prio_2_packets", "rx_prio_2_bytes",
	"rx_prio_3_packets", "rx_prio_3_bytes",
	"rx_prio_4_packets", "rx_prio_4_bytes",
	"rx_prio_5_packets", "rx_prio_5_bytes",
	"rx_prio_6_packets", "rx_prio_6_bytes",
	"rx_prio_7_packets", "rx_prio_7_bytes",
	"rx_novlan_packets", "rx_novlan_bytes",
	"tx_prio_0_packets", "tx_prio_0_bytes",
	"tx_prio_1_packets", "tx_prio_1_bytes",
	"tx_prio_2_packets", "tx_prio_2_bytes",
	"tx_prio_3_packets", "tx_prio_3_bytes",
	"tx_prio_4_packets", "tx_prio_4_bytes",
	"tx_prio_5_packets", "tx_prio_5_bytes",
	"tx_prio_6_packets", "tx_prio_6_bytes",
	"tx_prio_7_packets", "tx_prio_7_bytes",
	"tx_novlan_packets", "tx_novlan_bytes",

};

static const char mlx4_en_test_names[][ETH_GSTRING_LEN]= {
	"Interrupt Test",
	"Link Test",
	"Speed Test",
	"Register Test",
	"Loopback Test",
};

static uint32_t mlx4_en_get_msglevel(struct ether *dev)
{
	return ((struct mlx4_en_priv *) netdev_priv(dev))->msg_enable;
}

static void mlx4_en_set_msglevel(struct ether *dev, uint32_t val)
{
	((struct mlx4_en_priv *) netdev_priv(dev))->msg_enable = val;
}

static void mlx4_en_get_wol(struct ether *netdev,
			    struct ethtool_wolinfo *wol)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	int err = 0;
	uint64_t config = 0;
	uint64_t mask;

	if ((priv->port < 1) || (priv->port > 2)) {
		en_err(priv, "Failed to get WoL information\n");
		return;
	}

	mask = (priv->port == 1) ? MLX4_DEV_CAP_FLAG_WOL_PORT1 :
		MLX4_DEV_CAP_FLAG_WOL_PORT2;

	if (!(priv->mdev->dev->caps.flags & mask)) {
		wol->supported = 0;
		wol->wolopts = 0;
		return;
	}

	err = mlx4_wol_read(priv->mdev->dev, &config, priv->port);
	if (err) {
		en_err(priv, "Failed to get WoL information\n");
		return;
	}

	if (config & MLX4_EN_WOL_MAGIC)
		wol->supported = WAKE_MAGIC;
	else
		wol->supported = 0;

	if (config & MLX4_EN_WOL_ENABLED)
		wol->wolopts = WAKE_MAGIC;
	else
		wol->wolopts = 0;
}

static int mlx4_en_set_wol(struct ether *netdev,
			    struct ethtool_wolinfo *wol)
{
	struct mlx4_en_priv *priv = netdev_priv(netdev);
	uint64_t config = 0;
	int err = 0;
	uint64_t mask;

	if ((priv->port < 1) || (priv->port > 2))
		return -EOPNOTSUPP;

	mask = (priv->port == 1) ? MLX4_DEV_CAP_FLAG_WOL_PORT1 :
		MLX4_DEV_CAP_FLAG_WOL_PORT2;

	if (!(priv->mdev->dev->caps.flags & mask))
		return -EOPNOTSUPP;

	if (wol->supported & ~WAKE_MAGIC)
		return -EINVAL;

	err = mlx4_wol_read(priv->mdev->dev, &config, priv->port);
	if (err) {
		en_err(priv, "Failed to get WoL info, unable to modify\n");
		return err;
	}

	if (wol->wolopts & WAKE_MAGIC) {
		config |= MLX4_EN_WOL_DO_MODIFY | MLX4_EN_WOL_ENABLED |
				MLX4_EN_WOL_MAGIC;
	} else {
		config &= ~(MLX4_EN_WOL_ENABLED | MLX4_EN_WOL_MAGIC);
		config |= MLX4_EN_WOL_DO_MODIFY;
	}

	err = mlx4_wol_write(priv->mdev->dev, config, priv->port);
	if (err)
		en_err(priv, "Failed to set WoL information\n");

	return err;
}

struct bitmap_iterator {
	unsigned long *stats_bitmap;
	unsigned int count;
	unsigned int iterator;
	bool advance_array; /* if set, force no increments */
};

static inline void bitmap_iterator_init(struct bitmap_iterator *h,
					unsigned long *stats_bitmap,
					int count)
{
	h->iterator = 0;
	h->advance_array = !bitmap_empty(stats_bitmap, count);
	h->count = h->advance_array ? bitmap_weight(stats_bitmap, count)
		: count;
	h->stats_bitmap = stats_bitmap;
}

static inline int bitmap_iterator_test(struct bitmap_iterator *h)
{
	return !h->advance_array ? 1 : test_bit(h->iterator, h->stats_bitmap);
}

static inline int bitmap_iterator_inc(struct bitmap_iterator *h)
{
	return h->iterator++;
}

static inline unsigned int
bitmap_iterator_count(struct bitmap_iterator *h)
{
	return h->count;
}

static int mlx4_en_get_sset_count(struct ether *dev, int sset)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct bitmap_iterator it;

	bitmap_iterator_init(&it, priv->stats_bitmap.bitmap, NUM_ALL_STATS);

	switch (sset) {
	case ETH_SS_STATS:
		return bitmap_iterator_count(&it) +
			(priv->tx_ring_num * 2) +
#ifdef CONFIG_NET_RX_BUSY_POLL
			(priv->rx_ring_num * 5);
#else
			(priv->rx_ring_num * 2);
#endif
	case ETH_SS_TEST:
		return MLX4_EN_NUM_SELF_TEST - !(priv->mdev->dev->caps.flags
					& MLX4_DEV_CAP_FLAG_UC_LOOPBACK) * 2;
	case ETH_SS_PRIV_FLAGS:
		return ARRAY_SIZE(mlx4_en_priv_flags);
	default:
		return -EOPNOTSUPP;
	}
}

static void mlx4_en_get_ethtool_stats(struct ether *dev,
				      struct ethtool_stats *stats,
				      uint64_t *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int index = 0;
	int i;
	struct bitmap_iterator it;

	bitmap_iterator_init(&it, priv->stats_bitmap.bitmap, NUM_ALL_STATS);

	spin_lock(&priv->stats_lock);

	for (i = 0; i < NUM_MAIN_STATS; i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->stats)[i];

	for (i = 0; i < NUM_PORT_STATS; i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->port_stats)[i];

	for (i = 0; i < NUM_FLOW_PRIORITY_STATS_RX;
	     i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] =
				((uint64_t *)&priv->rx_priority_flowstats)[i];

	for (i = 0; i < NUM_FLOW_STATS_RX; i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] = ((uint64_t *)&priv->rx_flowstats)[i];

	for (i = 0; i < NUM_FLOW_PRIORITY_STATS_TX;
	     i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] =
				((uint64_t *)&priv->tx_priority_flowstats)[i];

	for (i = 0; i < NUM_FLOW_STATS_TX; i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] = ((uint64_t *)&priv->tx_flowstats)[i];

	for (i = 0; i < NUM_PKT_STATS; i++, bitmap_iterator_inc(&it))
		if (bitmap_iterator_test(&it))
			data[index++] = ((unsigned long *)&priv->pkstats)[i];

	for (i = 0; i < priv->tx_ring_num; i++) {
		data[index++] = priv->tx_ring[i]->packets;
		data[index++] = priv->tx_ring[i]->bytes;
	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		data[index++] = priv->rx_ring[i]->packets;
		data[index++] = priv->rx_ring[i]->bytes;
#ifdef CONFIG_NET_RX_BUSY_POLL
		data[index++] = priv->rx_ring[i]->yields;
		data[index++] = priv->rx_ring[i]->misses;
		data[index++] = priv->rx_ring[i]->cleaned;
#endif
	}
	spin_unlock(&priv->stats_lock);

}

static void mlx4_en_self_test(struct ether *dev,
			      struct ethtool_test *etest, uint64_t *buf)
{
	mlx4_en_ex_selftest(dev, &etest->flags, buf);
}

static void mlx4_en_get_strings(struct ether *dev,
				uint32_t stringset, uint8_t *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int index = 0;
	int i, strings = 0;
	struct bitmap_iterator it;

	bitmap_iterator_init(&it, priv->stats_bitmap.bitmap, NUM_ALL_STATS);

	switch (stringset) {
	case ETH_SS_TEST:
		for (i = 0; i < MLX4_EN_NUM_SELF_TEST - 2; i++)
			strcpy(data + i * ETH_GSTRING_LEN, mlx4_en_test_names[i]);
		if (priv->mdev->dev->caps.flags & MLX4_DEV_CAP_FLAG_UC_LOOPBACK)
			for (; i < MLX4_EN_NUM_SELF_TEST; i++)
				strcpy(data + i * ETH_GSTRING_LEN, mlx4_en_test_names[i]);
		break;

	case ETH_SS_STATS:
		/* Add main counters */
		for (i = 0; i < NUM_MAIN_STATS; i++, strings++,
		     bitmap_iterator_inc(&it))
			if (bitmap_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_PORT_STATS; i++, strings++,
		     bitmap_iterator_inc(&it))
			if (bitmap_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_FLOW_STATS; i++, strings++,
		     bitmap_iterator_inc(&it))
			if (bitmap_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < NUM_PKT_STATS; i++, strings++,
		     bitmap_iterator_inc(&it))
			if (bitmap_iterator_test(&it))
				strcpy(data + (index++) * ETH_GSTRING_LEN,
				       main_strings[strings]);

		for (i = 0; i < priv->tx_ring_num; i++) {
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"tx%d_packets", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"tx%d_bytes", i);
		}
		for (i = 0; i < priv->rx_ring_num; i++) {
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_packets", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_bytes", i);
#ifdef CONFIG_NET_RX_BUSY_POLL
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_napi_yield", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_misses", i);
			sprintf(data + (index++) * ETH_GSTRING_LEN,
				"rx%d_cleaned", i);
#endif
		}
		break;
	case ETH_SS_PRIV_FLAGS:
		for (i = 0; i < ARRAY_SIZE(mlx4_en_priv_flags); i++)
			strcpy(data + i * ETH_GSTRING_LEN,
			       mlx4_en_priv_flags[i]);
		break;

	}
}

static uint32_t mlx4_en_autoneg_get(struct ether *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	uint32_t autoneg = AUTONEG_DISABLE;

	if ((mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETH_BACKPL_AN_REP) &&
	    (priv->port_state.flags & MLX4_EN_PORT_ANE))
		autoneg = AUTONEG_ENABLE;

	return autoneg;
}

static uint32_t ptys_get_supported_port(struct mlx4_ptys_reg *ptys_reg)
{
	uint32_t eth_proto = be32_to_cpu(ptys_reg->eth_proto_cap);

	if (eth_proto & (MLX4_PROT_MASK(MLX4_10GBASE_T)
			 | MLX4_PROT_MASK(MLX4_1000BASE_T)
			 | MLX4_PROT_MASK(MLX4_100BASE_TX))) {
			return SUPPORTED_TP;
	}

	if (eth_proto & (MLX4_PROT_MASK(MLX4_10GBASE_CR)
			 | MLX4_PROT_MASK(MLX4_10GBASE_SR)
			 | MLX4_PROT_MASK(MLX4_56GBASE_SR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_CR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_SR4)
			 | MLX4_PROT_MASK(MLX4_1000BASE_CX_SGMII))) {
			return SUPPORTED_FIBRE;
	}

	if (eth_proto & (MLX4_PROT_MASK(MLX4_56GBASE_KR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_KR4)
			 | MLX4_PROT_MASK(MLX4_20GBASE_KR2)
			 | MLX4_PROT_MASK(MLX4_10GBASE_KR)
			 | MLX4_PROT_MASK(MLX4_10GBASE_KX4)
			 | MLX4_PROT_MASK(MLX4_1000BASE_KX))) {
			return SUPPORTED_Backplane;
	}
	return 0;
}

static uint32_t ptys_get_active_port(struct mlx4_ptys_reg *ptys_reg)
{
	uint32_t eth_proto = be32_to_cpu(ptys_reg->eth_proto_oper);

	if (!eth_proto) /* link down */
		eth_proto = be32_to_cpu(ptys_reg->eth_proto_cap);

	if (eth_proto & (MLX4_PROT_MASK(MLX4_10GBASE_T)
			 | MLX4_PROT_MASK(MLX4_1000BASE_T)
			 | MLX4_PROT_MASK(MLX4_100BASE_TX))) {
			return PORT_TP;
	}

	if (eth_proto & (MLX4_PROT_MASK(MLX4_10GBASE_SR)
			 | MLX4_PROT_MASK(MLX4_56GBASE_SR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_SR4)
			 | MLX4_PROT_MASK(MLX4_1000BASE_CX_SGMII))) {
			return PORT_FIBRE;
	}

	if (eth_proto & (MLX4_PROT_MASK(MLX4_10GBASE_CR)
			 | MLX4_PROT_MASK(MLX4_56GBASE_CR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_CR4))) {
			return PORT_DA;
	}

	if (eth_proto & (MLX4_PROT_MASK(MLX4_56GBASE_KR4)
			 | MLX4_PROT_MASK(MLX4_40GBASE_KR4)
			 | MLX4_PROT_MASK(MLX4_20GBASE_KR2)
			 | MLX4_PROT_MASK(MLX4_10GBASE_KR)
			 | MLX4_PROT_MASK(MLX4_10GBASE_KX4)
			 | MLX4_PROT_MASK(MLX4_1000BASE_KX))) {
			return PORT_NONE;
	}
	return PORT_OTHER;
}

#define MLX4_LINK_MODES_SZ \
	(FIELD_SIZEOF(struct mlx4_ptys_reg, eth_proto_cap) * 8)

enum ethtool_report {
	SUPPORTED = 0,
	ADVERTISED = 1,
	SPEED = 2
};

/* Translates mlx4 link mode to equivalent ethtool Link modes/speed */
static uint32_t ptys2ethtool_map[MLX4_LINK_MODES_SZ][3] = {
	[MLX4_100BASE_TX] = {
		SUPPORTED_100baseT_Full,
		ADVERTISED_100baseT_Full,
		SPEED_100
		},

	[MLX4_1000BASE_T] = {
		SUPPORTED_1000baseT_Full,
		ADVERTISED_1000baseT_Full,
		SPEED_1000
		},
	[MLX4_1000BASE_CX_SGMII] = {
		SUPPORTED_1000baseKX_Full,
		ADVERTISED_1000baseKX_Full,
		SPEED_1000
		},
	[MLX4_1000BASE_KX] = {
		SUPPORTED_1000baseKX_Full,
		ADVERTISED_1000baseKX_Full,
		SPEED_1000
		},

	[MLX4_10GBASE_T] = {
		SUPPORTED_10000baseT_Full,
		ADVERTISED_10000baseT_Full,
		SPEED_10000
		},
	[MLX4_10GBASE_CX4] = {
		SUPPORTED_10000baseKX4_Full,
		ADVERTISED_10000baseKX4_Full,
		SPEED_10000
		},
	[MLX4_10GBASE_KX4] = {
		SUPPORTED_10000baseKX4_Full,
		ADVERTISED_10000baseKX4_Full,
		SPEED_10000
		},
	[MLX4_10GBASE_KR] = {
		SUPPORTED_10000baseKR_Full,
		ADVERTISED_10000baseKR_Full,
		SPEED_10000
		},
	[MLX4_10GBASE_CR] = {
		SUPPORTED_10000baseKR_Full,
		ADVERTISED_10000baseKR_Full,
		SPEED_10000
		},
	[MLX4_10GBASE_SR] = {
		SUPPORTED_10000baseKR_Full,
		ADVERTISED_10000baseKR_Full,
		SPEED_10000
		},

	[MLX4_20GBASE_KR2] = {
		SUPPORTED_20000baseMLD2_Full | SUPPORTED_20000baseKR2_Full,
		ADVERTISED_20000baseMLD2_Full | ADVERTISED_20000baseKR2_Full,
		SPEED_20000
		},

	[MLX4_40GBASE_CR4] = {
		SUPPORTED_40000baseCR4_Full,
		ADVERTISED_40000baseCR4_Full,
		SPEED_40000
		},
	[MLX4_40GBASE_KR4] = {
		SUPPORTED_40000baseKR4_Full,
		ADVERTISED_40000baseKR4_Full,
		SPEED_40000
		},
	[MLX4_40GBASE_SR4] = {
		SUPPORTED_40000baseSR4_Full,
		ADVERTISED_40000baseSR4_Full,
		SPEED_40000
		},

	[MLX4_56GBASE_KR4] = {
		SUPPORTED_56000baseKR4_Full,
		ADVERTISED_56000baseKR4_Full,
		SPEED_56000
		},
	[MLX4_56GBASE_CR4] = {
		SUPPORTED_56000baseCR4_Full,
		ADVERTISED_56000baseCR4_Full,
		SPEED_56000
		},
	[MLX4_56GBASE_SR4] = {
		SUPPORTED_56000baseSR4_Full,
		ADVERTISED_56000baseSR4_Full,
		SPEED_56000
		},
};

static uint32_t ptys2ethtool_link_modes(uint32_t eth_proto,
					enum ethtool_report report)
{
	int i;
	uint32_t link_modes = 0;

	for (i = 0; i < MLX4_LINK_MODES_SZ; i++) {
		if (eth_proto & MLX4_PROT_MASK(i))
			link_modes |= ptys2ethtool_map[i][report];
	}
	return link_modes;
}

static uint32_t ethtool2ptys_link_modes(uint32_t link_modes,
					enum ethtool_report report)
{
	int i;
	uint32_t ptys_modes = 0;

	for (i = 0; i < MLX4_LINK_MODES_SZ; i++) {
		if (ptys2ethtool_map[i][report] & link_modes)
			ptys_modes |= 1 << i;
	}
	return ptys_modes;
}

/* Convert actual speed (SPEED_XXX) to ptys link modes */
static uint32_t speed2ptys_link_modes(uint32_t speed)
{
	int i;
	uint32_t ptys_modes = 0;

	for (i = 0; i < MLX4_LINK_MODES_SZ; i++) {
		if (ptys2ethtool_map[i][SPEED] == speed)
			ptys_modes |= 1 << i;
	}
	return ptys_modes;
}

static int ethtool_get_ptys_settings(struct ether *dev,
				     struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_ptys_reg ptys_reg;
	uint32_t eth_proto;
	int ret;

	memset(&ptys_reg, 0, sizeof(ptys_reg));
	ptys_reg.local_port = priv->port;
	ptys_reg.proto_mask = MLX4_PTYS_EN;
	ret = mlx4_ACCESS_PTYS_REG(priv->mdev->dev,
				   MLX4_ACCESS_REG_QUERY, &ptys_reg);
	if (ret) {
		en_warn(priv, "Failed to run mlx4_ACCESS_PTYS_REG status(%x)",
			ret);
		return ret;
	}
	en_dbg(DRV, priv, "ptys_reg.proto_mask       %x\n",
	       ptys_reg.proto_mask);
	en_dbg(DRV, priv, "ptys_reg.eth_proto_cap    %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_cap));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_admin  %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_admin));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_oper   %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_oper));
	en_dbg(DRV, priv, "ptys_reg.eth_proto_lp_adv %x\n",
	       be32_to_cpu(ptys_reg.eth_proto_lp_adv));

	cmd->supported = 0;
	cmd->advertising = 0;

	cmd->supported |= ptys_get_supported_port(&ptys_reg);

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_cap);
	cmd->supported |= ptys2ethtool_link_modes(eth_proto, SUPPORTED);

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_admin);
	cmd->advertising |= ptys2ethtool_link_modes(eth_proto, ADVERTISED);

	cmd->supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
	cmd->advertising |= (priv->prof->tx_pause) ? ADVERTISED_Pause : 0;

	cmd->advertising |= (priv->prof->tx_pause ^ priv->prof->rx_pause) ?
		ADVERTISED_Asym_Pause : 0;

	cmd->port = ptys_get_active_port(&ptys_reg);
	cmd->transceiver = (SUPPORTED_TP & cmd->supported) ?
		XCVR_EXTERNAL : XCVR_INTERNAL;

	if (mlx4_en_autoneg_get(dev)) {
		cmd->supported |= SUPPORTED_Autoneg;
		cmd->advertising |= ADVERTISED_Autoneg;
	}

	cmd->autoneg = (priv->port_state.flags & MLX4_EN_PORT_ANC) ?
		AUTONEG_ENABLE : AUTONEG_DISABLE;

	eth_proto = be32_to_cpu(ptys_reg.eth_proto_lp_adv);
	cmd->lp_advertising = ptys2ethtool_link_modes(eth_proto, ADVERTISED);

	cmd->lp_advertising |= (priv->port_state.flags & MLX4_EN_PORT_ANC) ?
			ADVERTISED_Autoneg : 0;

	cmd->phy_address = 0;
	cmd->mdio_support = 0;
	cmd->maxtxpkt = 0;
	cmd->maxrxpkt = 0;
	cmd->eth_tp_mdix = ETH_TP_MDI_INVALID;
	cmd->eth_tp_mdix_ctrl = ETH_TP_MDI_AUTO;

	return ret;
}

static void ethtool_get_default_settings(struct ether *dev,
					 struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int trans_type;

	cmd->autoneg = AUTONEG_DISABLE;
	cmd->supported = SUPPORTED_10000baseT_Full;
	cmd->advertising = ADVERTISED_10000baseT_Full;
	trans_type = priv->port_state.transceiver;

	if (trans_type > 0 && trans_type <= 0xC) {
		cmd->port = PORT_FIBRE;
		cmd->transceiver = XCVR_EXTERNAL;
		cmd->supported |= SUPPORTED_FIBRE;
		cmd->advertising |= ADVERTISED_FIBRE;
	} else if (trans_type == 0x80 || trans_type == 0) {
		cmd->port = PORT_TP;
		cmd->transceiver = XCVR_INTERNAL;
		cmd->supported |= SUPPORTED_TP;
		cmd->advertising |= ADVERTISED_TP;
	} else  {
		cmd->port = -1;
		cmd->transceiver = -1;
	}
}

static int mlx4_en_get_settings(struct ether *dev, struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int ret = -EINVAL;

	if (mlx4_en_QUERY_PORT(priv->mdev, priv->port))
		return -ENOMEM;

	en_dbg(DRV, priv, "query port state.flags ANC(%x) ANE(%x)\n",
	       priv->port_state.flags & MLX4_EN_PORT_ANC,
	       priv->port_state.flags & MLX4_EN_PORT_ANE);

	if (priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETH_PROT_CTRL)
		ret = ethtool_get_ptys_settings(dev, cmd);
	if (ret) /* ETH PROT CRTL is not supported or PTYS CMD failed */
		ethtool_get_default_settings(dev, cmd);

	if (netif_carrier_ok(dev)) {
		ethtool_cmd_speed_set(cmd, priv->port_state.link_speed);
		cmd->duplex = DUPLEX_FULL;
	} else {
		ethtool_cmd_speed_set(cmd, SPEED_UNKNOWN);
		cmd->duplex = DUPLEX_UNKNOWN;
	}
	return 0;
}

/* Calculate PTYS admin according ethtool speed (SPEED_XXX) */
static __be32 speed_set_ptys_admin(struct mlx4_en_priv *priv, uint32_t speed,
				   __be32 proto_cap)
{
	__be32 proto_admin = 0;

	if (!speed) { /* Speed = 0 ==> Reset Link modes */
		proto_admin = proto_cap;
		en_info(priv, "Speed was set to 0, Reset advertised Link Modes to default (%x)\n",
			be32_to_cpu(proto_cap));
	} else {
		uint32_t ptys_link_modes = speed2ptys_link_modes(speed);

		proto_admin = cpu_to_be32(ptys_link_modes) & proto_cap;
		en_info(priv, "Setting Speed to %d\n", speed);
	}
	return proto_admin;
}

static int mlx4_en_set_settings(struct ether *dev, struct ethtool_cmd *cmd)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_ptys_reg ptys_reg;
	__be32 proto_admin;
	int ret;

	uint32_t ptys_adv = ethtool2ptys_link_modes(cmd->advertising, ADVERTISED);
	int speed = ethtool_cmd_speed(cmd);

	en_dbg(DRV, priv, "Set Speed=%d adv=0x%x autoneg=%d duplex=%d\n",
	       speed, cmd->advertising, cmd->autoneg, cmd->duplex);

	if (!(priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_ETH_PROT_CTRL) ||
	    (cmd->duplex == DUPLEX_HALF))
		return -EINVAL;

	memset(&ptys_reg, 0, sizeof(ptys_reg));
	ptys_reg.local_port = priv->port;
	ptys_reg.proto_mask = MLX4_PTYS_EN;
	ret = mlx4_ACCESS_PTYS_REG(priv->mdev->dev,
				   MLX4_ACCESS_REG_QUERY, &ptys_reg);
	if (ret) {
		en_warn(priv, "Failed to QUERY mlx4_ACCESS_PTYS_REG status(%x)\n",
			ret);
		return 0;
	}

	proto_admin = cmd->autoneg == AUTONEG_ENABLE ?
		cpu_to_be32(ptys_adv) :
		speed_set_ptys_admin(priv, speed,
				     ptys_reg.eth_proto_cap);

	proto_admin &= ptys_reg.eth_proto_cap;
	if (!proto_admin) {
		en_warn(priv, "Not supported link mode(s) requested, check supported link modes.\n");
		return -EINVAL; /* nothing to change due to bad input */
	}

	if (proto_admin == ptys_reg.eth_proto_admin)
		return 0; /* Nothing to change */

	en_dbg(DRV, priv, "mlx4_ACCESS_PTYS_REG SET: ptys_reg.eth_proto_admin = 0x%x\n",
	       be32_to_cpu(proto_admin));

	ptys_reg.eth_proto_admin = proto_admin;
	ret = mlx4_ACCESS_PTYS_REG(priv->mdev->dev, MLX4_ACCESS_REG_WRITE,
				   &ptys_reg);
	if (ret) {
		en_warn(priv, "Failed to write mlx4_ACCESS_PTYS_REG eth_proto_admin(0x%x) status(0x%x)",
			be32_to_cpu(ptys_reg.eth_proto_admin), ret);
		return ret;
	}

	qlock(&priv->mdev->state_lock);
	if (priv->port_up) {
		en_warn(priv, "Port link mode changed, restarting port...\n");
		mlx4_en_stop_port(dev, 1);
		if (mlx4_en_start_port(dev))
			en_err(priv, "Failed restarting port %d\n", priv->port);
	}
	qunlock(&priv->mdev->state_lock);
	return 0;
}

static int mlx4_en_get_coalesce(struct ether *dev,
				struct ethtool_coalesce *coal)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	coal->tx_coalesce_usecs = priv->tx_usecs;
	coal->tx_max_coalesced_frames = priv->tx_frames;
	coal->tx_max_coalesced_frames_irq = priv->tx_work_limit;

	coal->rx_coalesce_usecs = priv->rx_usecs;
	coal->rx_max_coalesced_frames = priv->rx_frames;

	coal->pkt_rate_low = priv->pkt_rate_low;
	coal->rx_coalesce_usecs_low = priv->rx_usecs_low;
	coal->pkt_rate_high = priv->pkt_rate_high;
	coal->rx_coalesce_usecs_high = priv->rx_usecs_high;
	coal->rate_sample_interval = priv->sample_interval;
	coal->use_adaptive_rx_coalesce = priv->adaptive_rx_coal;

	return 0;
}

static int mlx4_en_set_coalesce(struct ether *dev,
				struct ethtool_coalesce *coal)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (!coal->tx_max_coalesced_frames_irq)
		return -EINVAL;

	priv->rx_frames = (coal->rx_max_coalesced_frames ==
			   MLX4_EN_AUTO_CONF) ?
				MLX4_EN_RX_COAL_TARGET :
				coal->rx_max_coalesced_frames;
	priv->rx_usecs = (coal->rx_coalesce_usecs ==
			  MLX4_EN_AUTO_CONF) ?
				MLX4_EN_RX_COAL_TIME :
				coal->rx_coalesce_usecs;

	/* Setting TX coalescing parameters */
	if (coal->tx_coalesce_usecs != priv->tx_usecs ||
	    coal->tx_max_coalesced_frames != priv->tx_frames) {
		priv->tx_usecs = coal->tx_coalesce_usecs;
		priv->tx_frames = coal->tx_max_coalesced_frames;
	}

	/* Set adaptive coalescing params */
	priv->pkt_rate_low = coal->pkt_rate_low;
	priv->rx_usecs_low = coal->rx_coalesce_usecs_low;
	priv->pkt_rate_high = coal->pkt_rate_high;
	priv->rx_usecs_high = coal->rx_coalesce_usecs_high;
	priv->sample_interval = coal->rate_sample_interval;
	priv->adaptive_rx_coal = coal->use_adaptive_rx_coalesce;
	priv->tx_work_limit = coal->tx_max_coalesced_frames_irq;

	return mlx4_en_moderation_update(priv);
}

static int mlx4_en_set_pauseparam(struct ether *dev,
				  struct ethtool_pauseparam *pause)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	if (pause->autoneg)
		return -EINVAL;

	priv->prof->tx_pause = pause->tx_pause != 0;
	priv->prof->rx_pause = pause->rx_pause != 0;
	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_skb_size + ETH_FCS_LEN,
				    priv->prof->tx_pause,
				    priv->prof->tx_ppp,
				    priv->prof->rx_pause,
				    priv->prof->rx_ppp);
	if (err)
		en_err(priv, "Failed setting pause params\n");
	else
		mlx4_en_update_pfc_stats_bitmap(mdev->dev, &priv->stats_bitmap,
						priv->prof->rx_ppp,
						priv->prof->rx_pause,
						priv->prof->tx_ppp,
						priv->prof->tx_pause);

	return err;
}

static void mlx4_en_get_pauseparam(struct ether *dev,
				   struct ethtool_pauseparam *pause)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	pause->tx_pause = priv->prof->tx_pause;
	pause->rx_pause = priv->prof->rx_pause;
}

static int mlx4_en_set_ringparam(struct ether *dev,
				 struct ethtool_ringparam *param)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	uint32_t rx_size, tx_size;
	int port_up = 0;
	int err = 0;

	if (param->rx_jumbo_pending || param->rx_mini_pending)
		return -EINVAL;

	rx_size = ROUNDUPPWR2(param->rx_pending);
	rx_size = MAX_T(uint32_t, rx_size, MLX4_EN_MIN_RX_SIZE);
	rx_size = MIN_T(uint32_t, rx_size, MLX4_EN_MAX_RX_SIZE);
	tx_size = ROUNDUPPWR2(param->tx_pending);
	tx_size = MAX_T(uint32_t, tx_size, MLX4_EN_MIN_TX_SIZE);
	tx_size = MIN_T(uint32_t, tx_size, MLX4_EN_MAX_TX_SIZE);

	if (rx_size == (priv->port_up ? priv->rx_ring[0]->actual_size :
					priv->rx_ring[0]->size) &&
	    tx_size == priv->tx_ring[0]->size)
		return 0;

	qlock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev, 1);
	}

	mlx4_en_free_resources(priv);

	priv->prof->tx_ring_size = tx_size;
	priv->prof->rx_ring_size = rx_size;

	err = mlx4_en_alloc_resources(priv);
	if (err) {
		en_err(priv, "Failed reallocating port resources\n");
		goto out;
	}
	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

	err = mlx4_en_moderation_update(priv);

out:
	qunlock(&mdev->state_lock);
	return err;
}

static void mlx4_en_get_ringparam(struct ether *dev,
				  struct ethtool_ringparam *param)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	memset(param, 0, sizeof(*param));
	param->rx_max_pending = MLX4_EN_MAX_RX_SIZE;
	param->tx_max_pending = MLX4_EN_MAX_TX_SIZE;
	param->rx_pending = priv->port_up ?
		priv->rx_ring[0]->actual_size : priv->rx_ring[0]->size;
	param->tx_pending = priv->tx_ring[0]->size;
}

static uint32_t mlx4_en_get_rxfh_indir_size(struct ether *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	return priv->rx_ring_num;
}

static uint32_t mlx4_en_get_rxfh_key_size(struct ether *netdev)
{
	return MLX4_EN_RSS_KEY_SIZE;
}

static int mlx4_en_check_rxfh_func(struct ether *dev, uint8_t hfunc)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	/* check if requested function is supported by the device */
	if (hfunc == ETH_RSS_HASH_TOP) {
		if (!(priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_RSS_TOP))
			return -EINVAL;
		if (!(dev->feat & NETIF_F_RXHASH))
			en_warn(priv, "Toeplitz hash function should be used in conjunction with RX hashing for optimal performance\n");
		return 0;
	} else if (hfunc == ETH_RSS_HASH_XOR) {
		if (!(priv->mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_RSS_XOR))
			return -EINVAL;
		if (dev->feat & NETIF_F_RXHASH)
			en_warn(priv, "Enabling both XOR Hash function and RX Hashing can limit RPS functionality\n");
		return 0;
	}

	return -EINVAL;
}

static int mlx4_en_get_rxfh(struct ether *dev, uint32_t *ring_index,
			    uint8_t *key,
			    uint8_t *hfunc)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_rss_map *rss_map = &priv->rss_map;
	int rss_rings;
	size_t n = priv->rx_ring_num;
	int err = 0;

	rss_rings = priv->prof->rss_rings ?: priv->rx_ring_num;
	rss_rings = 1 << LOG2_UP(rss_rings);

	while (n--) {
		if (!ring_index)
			break;
		ring_index[n] = rss_map->qps[n % rss_rings].qpn -
			rss_map->base_qpn;
	}
	if (key)
		memcpy(key, priv->rss_key, MLX4_EN_RSS_KEY_SIZE);
	if (hfunc)
		*hfunc = priv->rss_hash_fn;
	return err;
}

static int mlx4_en_set_rxfh(struct ether *dev,
			    const uint32_t *ring_index,
			    const uint8_t *key, const uint8_t hfunc)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int err = 0;
	int i;
	int rss_rings = 0;

	/* Calculate RSS table size and make sure flows are spread evenly
	 * between rings
	 */
	for (i = 0; i < priv->rx_ring_num; i++) {
		if (!ring_index)
			continue;
		if (i > 0 && !ring_index[i] && !rss_rings)
			rss_rings = i;

		if (ring_index[i] != (i % (rss_rings ?: priv->rx_ring_num)))
			return -EINVAL;
	}

	if (!rss_rings)
		rss_rings = priv->rx_ring_num;

	/* RSS table size must be an order of 2 */
	if (!IS_PWR2(rss_rings))
		return -EINVAL;

	if (hfunc != ETH_RSS_HASH_NO_CHANGE) {
		err = mlx4_en_check_rxfh_func(dev, hfunc);
		if (err)
			return err;
	}

	qlock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev, 1);
	}

	if (ring_index)
		priv->prof->rss_rings = rss_rings;
	if (key)
		memcpy(priv->rss_key, key, MLX4_EN_RSS_KEY_SIZE);
	if (hfunc !=  ETH_RSS_HASH_NO_CHANGE)
		priv->rss_hash_fn = hfunc;

	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

	qunlock(&mdev->state_lock);
	return err;
}

#define all_zeros_or_all_ones(field)		\
	((field) == 0 || (field) == (__force typeof(field))-1)

static int mlx4_en_validate_flow(struct ether *dev,
				 struct ethtool_rxnfc *cmd)
{
	struct ethtool_usrip4_spec *l3_mask;
	struct ethtool_tcpip4_spec *l4_mask;
	struct ethhdr *eth_mask;

	if (cmd->fs.location >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	if (cmd->fs.flow_type & FLOW_MAC_EXT) {
		/* dest mac mask must be ff:ff:ff:ff:ff:ff */
		if (!is_broadcast_ether_addr(cmd->fs.m_ext.h_dest))
			return -EINVAL;
	}

	switch (cmd->fs.flow_type & ~(FLOW_EXT | FLOW_MAC_EXT)) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
		if (cmd->fs.m_u.tcp_ip4_spec.tos)
			return -EINVAL;
		l4_mask = &cmd->fs.m_u.tcp_ip4_spec;
		/* don't allow mask which isn't all 0 or 1 */
		if (!all_zeros_or_all_ones(l4_mask->ip4src) ||
		    !all_zeros_or_all_ones(l4_mask->ip4dst) ||
		    !all_zeros_or_all_ones(l4_mask->psrc) ||
		    !all_zeros_or_all_ones(l4_mask->pdst))
			return -EINVAL;
		break;
	case IP_USER_FLOW:
		l3_mask = &cmd->fs.m_u.usr_ip4_spec;
		if (l3_mask->l4_4_bytes || l3_mask->tos || l3_mask->proto ||
		    cmd->fs.h_u.usr_ip4_spec.ip_ver != ETH_RX_NFC_IP4 ||
		    (!l3_mask->ip4src && !l3_mask->ip4dst) ||
		    !all_zeros_or_all_ones(l3_mask->ip4src) ||
		    !all_zeros_or_all_ones(l3_mask->ip4dst))
			return -EINVAL;
		break;
	case ETHER_FLOW:
		eth_mask = &cmd->fs.m_u.ether_spec;
		/* source mac mask must not be set */
		if (!is_zero_ether_addr(eth_mask->h_source))
			return -EINVAL;

		/* dest mac mask must be ff:ff:ff:ff:ff:ff */
		if (!is_broadcast_ether_addr(eth_mask->h_dest))
			return -EINVAL;

		if (!all_zeros_or_all_ones(eth_mask->h_proto))
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if ((cmd->fs.flow_type & FLOW_EXT)) {
		if (cmd->fs.m_ext.vlan_etype ||
		    !((cmd->fs.m_ext.vlan_tci & cpu_to_be16(VLAN_VID_MASK)) ==
		      0 ||
		      (cmd->fs.m_ext.vlan_tci & cpu_to_be16(VLAN_VID_MASK)) ==
		      cpu_to_be16(VLAN_VID_MASK)))
			return -EINVAL;

		if (cmd->fs.m_ext.vlan_tci) {
			if (be16_to_cpu(cmd->fs.h_ext.vlan_tci) >= VLAN_N_VID)
				return -EINVAL;

		}
	}

	return 0;
}

static int mlx4_en_ethtool_add_mac_rule(struct ethtool_rxnfc *cmd,
					struct list_head *rule_list_h,
					struct mlx4_spec_list *spec_l2,
					unsigned char *mac)
{
	int err = 0;
	__be64 mac_msk = cpu_to_be64(MLX4_MAC_MASK << 16);

	spec_l2->id = MLX4_NET_TRANS_RULE_ID_ETH;
	memcpy(spec_l2->eth.dst_mac_msk, &mac_msk, Eaddrlen);
	memcpy(spec_l2->eth.dst_mac, mac, Eaddrlen);

	if ((cmd->fs.flow_type & FLOW_EXT) &&
	    (cmd->fs.m_ext.vlan_tci & cpu_to_be16(VLAN_VID_MASK))) {
		spec_l2->eth.vlan_id = cmd->fs.h_ext.vlan_tci;
		spec_l2->eth.vlan_id_msk = cpu_to_be16(VLAN_VID_MASK);
	}

	list_add_tail(&spec_l2->list, rule_list_h);

	return err;
}

static int mlx4_en_ethtool_add_mac_rule_by_ipv4(struct mlx4_en_priv *priv,
						struct ethtool_rxnfc *cmd,
						struct list_head *rule_list_h,
						struct mlx4_spec_list *spec_l2,
						__be32 ipv4_dst)
{
#ifdef CONFIG_INET
	unsigned char mac[Eaddrlen];

	if (!ipv4_is_multicast(ipv4_dst)) {
		if (cmd->fs.flow_type & FLOW_MAC_EXT)
			memcpy(&mac, cmd->fs.h_ext.h_dest, Eaddrlen);
		else
			memcpy(&mac, priv->dev->ea, Eaddrlen);
	} else {
		ip_eth_mc_map(ipv4_dst, mac);
	}

	return mlx4_en_ethtool_add_mac_rule(cmd, rule_list_h, spec_l2, &mac[0]);
#else
	return -EINVAL;
#endif
}

static int add_ip_rule(struct mlx4_en_priv *priv,
		       struct ethtool_rxnfc *cmd,
		       struct list_head *list_h)
{
	int err;
	struct mlx4_spec_list *spec_l2 = NULL;
	struct mlx4_spec_list *spec_l3 = NULL;
	struct ethtool_usrip4_spec *l3_mask = &cmd->fs.m_u.usr_ip4_spec;

	spec_l3 = kzmalloc(sizeof(*spec_l3), KMALLOC_WAIT);
	spec_l2 = kzmalloc(sizeof(*spec_l2), KMALLOC_WAIT);
	if (!spec_l2 || !spec_l3) {
		err = -ENOMEM;
		goto free_spec;
	}

	err = mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h, spec_l2,
						   cmd->fs.h_u.
						   usr_ip4_spec.ip4dst);
	if (err)
		goto free_spec;
	spec_l3->id = MLX4_NET_TRANS_RULE_ID_IPV4;
	spec_l3->ipv4.src_ip = cmd->fs.h_u.usr_ip4_spec.ip4src;
	if (l3_mask->ip4src)
		spec_l3->ipv4.src_ip_msk = EN_ETHTOOL_WORD_MASK;
	spec_l3->ipv4.dst_ip = cmd->fs.h_u.usr_ip4_spec.ip4dst;
	if (l3_mask->ip4dst)
		spec_l3->ipv4.dst_ip_msk = EN_ETHTOOL_WORD_MASK;
	list_add_tail(&spec_l3->list, list_h);

	return 0;

free_spec:
	kfree(spec_l2);
	kfree(spec_l3);
	return err;
}

static int add_tcp_udp_rule(struct mlx4_en_priv *priv,
			     struct ethtool_rxnfc *cmd,
			     struct list_head *list_h, int proto)
{
	int err;
	struct mlx4_spec_list *spec_l2 = NULL;
	struct mlx4_spec_list *spec_l3 = NULL;
	struct mlx4_spec_list *spec_l4 = NULL;
	struct ethtool_tcpip4_spec *l4_mask = &cmd->fs.m_u.tcp_ip4_spec;

	spec_l2 = kzmalloc(sizeof(*spec_l2), KMALLOC_WAIT);
	spec_l3 = kzmalloc(sizeof(*spec_l3), KMALLOC_WAIT);
	spec_l4 = kzmalloc(sizeof(*spec_l4), KMALLOC_WAIT);
	if (!spec_l2 || !spec_l3 || !spec_l4) {
		err = -ENOMEM;
		goto free_spec;
	}

	spec_l3->id = MLX4_NET_TRANS_RULE_ID_IPV4;

	if (proto == TCP_V4_FLOW) {
		err = mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h,
							   spec_l2,
							   cmd->fs.h_u.
							   tcp_ip4_spec.ip4dst);
		if (err)
			goto free_spec;
		spec_l4->id = MLX4_NET_TRANS_RULE_ID_TCP;
		spec_l3->ipv4.src_ip = cmd->fs.h_u.tcp_ip4_spec.ip4src;
		spec_l3->ipv4.dst_ip = cmd->fs.h_u.tcp_ip4_spec.ip4dst;
		spec_l4->tcp_udp.src_port = cmd->fs.h_u.tcp_ip4_spec.psrc;
		spec_l4->tcp_udp.dst_port = cmd->fs.h_u.tcp_ip4_spec.pdst;
	} else {
		err = mlx4_en_ethtool_add_mac_rule_by_ipv4(priv, cmd, list_h,
							   spec_l2,
							   cmd->fs.h_u.
							   udp_ip4_spec.ip4dst);
		if (err)
			goto free_spec;
		spec_l4->id = MLX4_NET_TRANS_RULE_ID_UDP;
		spec_l3->ipv4.src_ip = cmd->fs.h_u.udp_ip4_spec.ip4src;
		spec_l3->ipv4.dst_ip = cmd->fs.h_u.udp_ip4_spec.ip4dst;
		spec_l4->tcp_udp.src_port = cmd->fs.h_u.udp_ip4_spec.psrc;
		spec_l4->tcp_udp.dst_port = cmd->fs.h_u.udp_ip4_spec.pdst;
	}

	if (l4_mask->ip4src)
		spec_l3->ipv4.src_ip_msk = EN_ETHTOOL_WORD_MASK;
	if (l4_mask->ip4dst)
		spec_l3->ipv4.dst_ip_msk = EN_ETHTOOL_WORD_MASK;

	if (l4_mask->psrc)
		spec_l4->tcp_udp.src_port_msk = EN_ETHTOOL_SHORT_MASK;
	if (l4_mask->pdst)
		spec_l4->tcp_udp.dst_port_msk = EN_ETHTOOL_SHORT_MASK;

	list_add_tail(&spec_l3->list, list_h);
	list_add_tail(&spec_l4->list, list_h);

	return 0;

free_spec:
	kfree(spec_l2);
	kfree(spec_l3);
	kfree(spec_l4);
	return err;
}

static int mlx4_en_ethtool_to_net_trans_rule(struct ether *dev,
					     struct ethtool_rxnfc *cmd,
					     struct list_head *rule_list_h)
{
	int err;
	struct ethhdr *eth_spec;
	struct mlx4_spec_list *spec_l2;
	struct mlx4_en_priv *priv = netdev_priv(dev);

	err = mlx4_en_validate_flow(dev, cmd);
	if (err)
		return err;

	switch (cmd->fs.flow_type & ~(FLOW_EXT | FLOW_MAC_EXT)) {
	case ETHER_FLOW:
		spec_l2 = kzmalloc(sizeof(*spec_l2), KMALLOC_WAIT);
		if (!spec_l2)
			return -ENOMEM;

		eth_spec = &cmd->fs.h_u.ether_spec;
		mlx4_en_ethtool_add_mac_rule(cmd, rule_list_h, spec_l2,
					     &eth_spec->h_dest[0]);
		spec_l2->eth.ether_type = eth_spec->h_proto;
		if (eth_spec->h_proto)
			spec_l2->eth.ether_type_enable = 1;
		break;
	case IP_USER_FLOW:
		err = add_ip_rule(priv, cmd, rule_list_h);
		break;
	case TCP_V4_FLOW:
		err = add_tcp_udp_rule(priv, cmd, rule_list_h, TCP_V4_FLOW);
		break;
	case UDP_V4_FLOW:
		err = add_tcp_udp_rule(priv, cmd, rule_list_h, UDP_V4_FLOW);
		break;
	}

	return err;
}

static int mlx4_en_flow_replace(struct ether *dev,
				struct ethtool_rxnfc *cmd)
{
	int err;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct ethtool_flow_id *loc_rule;
	struct mlx4_spec_list *spec, *tmp_spec;
	uint32_t qpn;
	uint64_t reg_id;

	struct mlx4_net_trans_rule rule = {
		.queue_mode = MLX4_NET_TRANS_Q_FIFO,
		.exclusive = 0,
		.allow_loopback = 1,
		.promisc_mode = MLX4_FS_REGULAR,
	};

	rule.port = priv->port;
	rule.priority = MLX4_DOMAIN_ETHTOOL | cmd->fs.location;
	INIT_LIST_HEAD(&rule.list);

	/* Allow direct QP attaches if the EN_ETHTOOL_QP_ATTACH flag is set */
	if (cmd->fs.ring_cookie == RX_CLS_FLOW_DISC)
		qpn = priv->drop_qp.qpn;
	else if (cmd->fs.ring_cookie & EN_ETHTOOL_QP_ATTACH) {
		qpn = cmd->fs.ring_cookie & (EN_ETHTOOL_QP_ATTACH - 1);
	} else {
		if (cmd->fs.ring_cookie >= priv->rx_ring_num) {
			en_warn(priv, "rxnfc: RX ring (%llu) doesn't exist\n",
				cmd->fs.ring_cookie);
			return -EINVAL;
		}
		qpn = priv->rss_map.qps[cmd->fs.ring_cookie].qpn;
		if (!qpn) {
			en_warn(priv, "rxnfc: RX ring (%llu) is inactive\n",
				cmd->fs.ring_cookie);
			return -EINVAL;
		}
	}
	rule.qpn = qpn;
	err = mlx4_en_ethtool_to_net_trans_rule(dev, cmd, &rule.list);
	if (err)
		goto out_free_list;

	loc_rule = &priv->ethtool_rules[cmd->fs.location];
	if (loc_rule->id) {
		err = mlx4_flow_detach(priv->mdev->dev, loc_rule->id);
		if (err) {
			en_err(priv, "Fail to detach network rule at location %d. registration id = %llx\n",
			       cmd->fs.location, loc_rule->id);
			goto out_free_list;
		}
		loc_rule->id = 0;
		memset(&loc_rule->flow_spec, 0,
		       sizeof(struct ethtool_rx_flow_spec));
		list_del(&loc_rule->list);
	}
	err = mlx4_flow_attach(priv->mdev->dev, &rule, &reg_id);
	if (err) {
		en_err(priv, "Fail to attach network rule at location %d\n",
		       cmd->fs.location);
		goto out_free_list;
	}
	loc_rule->id = reg_id;
	memcpy(&loc_rule->flow_spec, &cmd->fs,
	       sizeof(struct ethtool_rx_flow_spec));
	list_add_tail(&loc_rule->list, &priv->ethtool_list);

out_free_list:
	list_for_each_entry_safe(spec, tmp_spec, &rule.list, list) {
		list_del(&spec->list);
		kfree(spec);
	}
	return err;
}

static int mlx4_en_flow_detach(struct ether *dev,
			       struct ethtool_rxnfc *cmd)
{
	int err = 0;
	struct ethtool_flow_id *rule;
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (cmd->fs.location >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	rule = &priv->ethtool_rules[cmd->fs.location];
	if (!rule->id) {
		err =  -ENOENT;
		goto out;
	}

	err = mlx4_flow_detach(priv->mdev->dev, rule->id);
	if (err) {
		en_err(priv, "Fail to detach network rule at location %d. registration id = 0x%llx\n",
		       cmd->fs.location, rule->id);
		goto out;
	}
	rule->id = 0;
	memset(&rule->flow_spec, 0, sizeof(struct ethtool_rx_flow_spec));
	list_del(&rule->list);
out:
	return err;

}

static int mlx4_en_get_flow(struct ether *dev, struct ethtool_rxnfc *cmd,
			    int loc)
{
	int err = 0;
	struct ethtool_flow_id *rule;
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (loc < 0 || loc >= MAX_NUM_OF_FS_RULES)
		return -EINVAL;

	rule = &priv->ethtool_rules[loc];
	if (rule->id)
		memcpy(&cmd->fs, &rule->flow_spec,
		       sizeof(struct ethtool_rx_flow_spec));
	else
		err = -ENOENT;

	return err;
}

static int mlx4_en_get_num_flows(struct mlx4_en_priv *priv)
{

	int i, res = 0;
	for (i = 0; i < MAX_NUM_OF_FS_RULES; i++) {
		if (priv->ethtool_rules[i].id)
			res++;
	}
	return res;

}

static int mlx4_en_get_rxnfc(struct ether *dev, struct ethtool_rxnfc *cmd,
			     uint32_t *rule_locs)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;
	int i = 0, priority = 0;

	if ((cmd->cmd == ETHTOOL_GRXCLSRLCNT ||
	     cmd->cmd == ETHTOOL_GRXCLSRULE ||
	     cmd->cmd == ETHTOOL_GRXCLSRLALL) &&
	    (mdev->dev->caps.steering_mode !=
	     MLX4_STEERING_MODE_DEVICE_MANAGED || !priv->port_up))
		return -EINVAL;

	switch (cmd->cmd) {
	case ETHTOOL_GRXRINGS:
		cmd->data = priv->rx_ring_num;
		break;
	case ETHTOOL_GRXCLSRLCNT:
		cmd->rule_cnt = mlx4_en_get_num_flows(priv);
		break;
	case ETHTOOL_GRXCLSRULE:
		err = mlx4_en_get_flow(dev, cmd, cmd->fs.location);
		break;
	case ETHTOOL_GRXCLSRLALL:
		while ((!err || err == -ENOENT) && priority < cmd->rule_cnt) {
			err = mlx4_en_get_flow(dev, cmd, i);
			if (!err)
				rule_locs[priority++] = i;
			i++;
		}
		err = 0;
		break;
	default:
		err = -EOPNOTSUPP;
		break;
	}

	return err;
}

static int mlx4_en_set_rxnfc(struct ether *dev, struct ethtool_rxnfc *cmd)
{
	int err = 0;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	if (mdev->dev->caps.steering_mode !=
	    MLX4_STEERING_MODE_DEVICE_MANAGED || !priv->port_up)
		return -EINVAL;

	switch (cmd->cmd) {
	case ETHTOOL_SRXCLSRLINS:
		err = mlx4_en_flow_replace(dev, cmd);
		break;
	case ETHTOOL_SRXCLSRLDEL:
		err = mlx4_en_flow_detach(dev, cmd);
		break;
	default:
		en_warn(priv, "Unsupported ethtool command. (%d)\n", cmd->cmd);
		return -EINVAL;
	}

	return err;
}

static void mlx4_en_get_channels(struct ether *dev,
				 struct ethtool_channels *channel)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	memset(channel, 0, sizeof(*channel));

	channel->max_rx = MAX_RX_RINGS;
	channel->max_tx = MLX4_EN_MAX_TX_RING_P_UP;

	channel->rx_count = priv->rx_ring_num;
	channel->tx_count = priv->tx_ring_num / MLX4_EN_NUM_UP;
}

static int mlx4_en_set_channels(struct ether *dev,
				struct ethtool_channels *channel)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int err = 0;

	if (channel->other_count || channel->combined_count ||
	    channel->tx_count > MLX4_EN_MAX_TX_RING_P_UP ||
	    channel->rx_count > MAX_RX_RINGS ||
	    !channel->tx_count || !channel->rx_count)
		return -EINVAL;

	qlock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_stop_port(dev, 1);
	}

	mlx4_en_free_resources(priv);

	priv->num_tx_rings_p_up = channel->tx_count;
	priv->tx_ring_num = channel->tx_count * MLX4_EN_NUM_UP;
	priv->rx_ring_num = channel->rx_count;

	err = mlx4_en_alloc_resources(priv);
	if (err) {
		en_err(priv, "Failed reallocating port resources\n");
		goto out;
	}

	netif_set_real_num_tx_queues(dev, priv->tx_ring_num);
	netif_set_real_num_rx_queues(dev, priv->rx_ring_num);

	if (dev->num_tc)
		mlx4_en_setup_tc(dev, MLX4_EN_NUM_UP);

	en_warn(priv, "Using %d TX rings\n", priv->tx_ring_num);
	en_warn(priv, "Using %d RX rings\n", priv->rx_ring_num);

	if (port_up) {
		err = mlx4_en_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}

	err = mlx4_en_moderation_update(priv);

out:
	qunlock(&mdev->state_lock);
	return err;
}

static int mlx4_en_get_ts_info(struct ether *dev,
			       struct ethtool_ts_info *info)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int ret;

	ret = ethtool_op_get_ts_info(dev, info);
	if (ret)
		return ret;

	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_TS) {
		info->so_timestamping |=
			SOF_TIMESTAMPING_TX_HARDWARE |
			SOF_TIMESTAMPING_RX_HARDWARE |
			SOF_TIMESTAMPING_RAW_HARDWARE;

		info->tx_types =
			(1 << HWTSTAMP_TX_OFF) |
			(1 << HWTSTAMP_TX_ON);

		info->rx_filters =
			(1 << HWTSTAMP_FILTER_NONE) |
			(1 << HWTSTAMP_FILTER_ALL);

		if (mdev->ptp_clock)
			info->phc_index = ptp_clock_index(mdev->ptp_clock);
	}

	return ret;
}

static int mlx4_en_set_priv_flags(struct ether *dev, uint32_t flags)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	bool bf_enabled_new = !!(flags & MLX4_EN_PRIV_FLAGS_BLUEFLAME);
	bool bf_enabled_old = !!(priv->pflags & MLX4_EN_PRIV_FLAGS_BLUEFLAME);
	int i;

	if (bf_enabled_new == bf_enabled_old)
		return 0; /* Nothing to do */

	if (bf_enabled_new) {
		bool bf_supported = true;

		for (i = 0; i < priv->tx_ring_num; i++)
			bf_supported &= priv->tx_ring[i]->bf_alloced;

		if (!bf_supported) {
			en_err(priv, "BlueFlame is not supported\n");
			return -EINVAL;
		}

		priv->pflags |= MLX4_EN_PRIV_FLAGS_BLUEFLAME;
	} else {
		priv->pflags &= ~MLX4_EN_PRIV_FLAGS_BLUEFLAME;
	}

	for (i = 0; i < priv->tx_ring_num; i++)
		priv->tx_ring[i]->bf_enabled = bf_enabled_new;

	en_info(priv, "BlueFlame %s\n",
		bf_enabled_new ?  "Enabled" : "Disabled");

	return 0;
}

static uint32_t mlx4_en_get_priv_flags(struct ether *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	return priv->pflags;
}

static int mlx4_en_get_tunable(struct ether *dev,
			       const struct ethtool_tunable *tuna,
			       void *data)
{
	const struct mlx4_en_priv *priv = netdev_priv(dev);
	int ret = 0;

	switch (tuna->id) {
	case ETHTOOL_TX_COPYBREAK:
		*(uint32_t *)data = priv->prof->inline_thold;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mlx4_en_set_tunable(struct ether *dev,
			       const struct ethtool_tunable *tuna,
			       const void *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int val, ret = 0;

	switch (tuna->id) {
	case ETHTOOL_TX_COPYBREAK:
		val = *(uint32_t *)data;
		if (val < MIN_PKT_LEN || val > MAX_INLINE)
			ret = -EINVAL;
		else
			priv->prof->inline_thold = val;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mlx4_en_get_module_info(struct ether *dev,
				   struct ethtool_modinfo *modinfo)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int ret;
	uint8_t data[4];

	/* Read first 2 bytes to get Module & REV ID */
	ret = mlx4_get_module_info(mdev->dev, priv->port,
				   0/*offset*/, 2/*size*/, data);
	if (ret < 2)
		return -EIO;

	switch (data[0] /* identifier */) {
	case MLX4_MODULE_ID_QSFP:
		modinfo->type = ETH_MODULE_SFF_8436;
		modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
		break;
	case MLX4_MODULE_ID_QSFP_PLUS:
		if (data[1] >= 0x3) { /* revision id */
			modinfo->type = ETH_MODULE_SFF_8636;
			modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
		} else {
			modinfo->type = ETH_MODULE_SFF_8436;
			modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
		}
		break;
	case MLX4_MODULE_ID_QSFP28:
		modinfo->type = ETH_MODULE_SFF_8636;
		modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
		break;
	case MLX4_MODULE_ID_SFP:
		modinfo->type = ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = ETH_MODULE_SFF_8472_LEN;
		break;
	default:
		return -ENOSYS;
	}

	return 0;
}

static int mlx4_en_get_module_eeprom(struct ether *dev,
				     struct ethtool_eeprom *ee,
				     uint8_t *data)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int offset = ee->offset;
	int i = 0, ret;

	if (ee->len == 0)
		return -EINVAL;

	memset(data, 0, ee->len);

	while (i < ee->len) {
		en_dbg(DRV, priv,
		       "mlx4_get_module_info i(%d) offset(%d) len(%d)\n",
		       i, offset, ee->len - i);

		ret = mlx4_get_module_info(mdev->dev, priv->port,
					   offset, ee->len - i, data + i);

		if (!ret) /* Done reading */
			return 0;

		if (ret < 0) {
			en_err(priv,
			       "mlx4_get_module_info i(%d) offset(%d) bytes_to_read(%d) - FAILED (0x%x)\n",
			       i, offset, ee->len - i, ret);
			return 0;
		}

		i += ret;
		offset += ret;
	}
	return 0;
}

static int mlx4_en_set_phys_id(struct ether *dev,
			       enum ethtool_phys_id_state state)
{
	int err;
	uint16_t beacon_duration;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	if (!(mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_PORT_BEACON))
		return -EOPNOTSUPP;

	switch (state) {
	case ETHTOOL_ID_ACTIVE:
		beacon_duration = PORT_BEACON_MAX_LIMIT;
		break;
	case ETHTOOL_ID_INACTIVE:
		beacon_duration = 0;
		break;
	default:
		return -EOPNOTSUPP;
	}

	err = mlx4_SET_PORT_BEACON(mdev->dev, priv->port, beacon_duration);
	return err;
}

const struct ethtool_ops mlx4_en_ethtool_ops = {
	.get_drvinfo = mlx4_en_get_drvinfo,
	.get_settings = mlx4_en_get_settings,
	.set_settings = mlx4_en_set_settings,
	.get_link = ethtool_op_get_link,
	.get_strings = mlx4_en_get_strings,
	.get_sset_count = mlx4_en_get_sset_count,
	.get_ethtool_stats = mlx4_en_get_ethtool_stats,
	.self_test = mlx4_en_self_test,
	.set_phys_id = mlx4_en_set_phys_id,
	.get_wol = mlx4_en_get_wol,
	.set_wol = mlx4_en_set_wol,
	.get_msglevel = mlx4_en_get_msglevel,
	.set_msglevel = mlx4_en_set_msglevel,
	.get_coalesce = mlx4_en_get_coalesce,
	.set_coalesce = mlx4_en_set_coalesce,
	.get_pauseparam = mlx4_en_get_pauseparam,
	.set_pauseparam = mlx4_en_set_pauseparam,
	.get_ringparam = mlx4_en_get_ringparam,
	.set_ringparam = mlx4_en_set_ringparam,
	.get_rxnfc = mlx4_en_get_rxnfc,
	.set_rxnfc = mlx4_en_set_rxnfc,
	.get_rxfh_indir_size = mlx4_en_get_rxfh_indir_size,
	.get_rxfh_key_size = mlx4_en_get_rxfh_key_size,
	.get_rxfh = mlx4_en_get_rxfh,
	.set_rxfh = mlx4_en_set_rxfh,
	.get_channels = mlx4_en_get_channels,
	.set_channels = mlx4_en_set_channels,
	.get_ts_info = mlx4_en_get_ts_info,
	.set_priv_flags = mlx4_en_set_priv_flags,
	.get_priv_flags = mlx4_en_get_priv_flags,
	.get_tunable		= mlx4_en_get_tunable,
	.set_tunable		= mlx4_en_set_tunable,
	.get_module_info = mlx4_en_get_module_info,
	.get_module_eeprom = mlx4_en_get_module_eeprom
};




