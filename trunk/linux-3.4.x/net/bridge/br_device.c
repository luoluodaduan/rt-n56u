/*
 *	Device handling code
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/netpoll.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/list.h>
#include <linux/netfilter_bridge.h>

#include <asm/uaccess.h>
#include "br_private.h"

/* net device transmit always called with BH disabled */
netdev_tx_t br_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);
	const unsigned char *dest = skb->data;
	struct net_bridge_fdb_entry *dst;
	struct net_bridge_mdb_entry *mdst;
	struct br_cpu_netstats *brstats = this_cpu_ptr(br->stats);

	rcu_read_lock();
#ifdef CONFIG_BRIDGE_NETFILTER
	if (skb->nf_bridge && (skb->nf_bridge->mask & BRNF_BRIDGED_DNAT)) {
		br_nf_pre_routing_finish_bridge_slow(skb);
		rcu_read_unlock();
		return NETDEV_TX_OK;
	}
#endif

	u64_stats_update_begin(&brstats->syncp);
	brstats->tx_packets++;
	brstats->tx_bytes += skb->len;
	u64_stats_update_end(&brstats->syncp);

	BR_INPUT_SKB_CB(skb)->brdev = dev;

	skb_reset_mac_header(skb);
	skb_pull(skb, ETH_HLEN);

	if (is_broadcast_ether_addr(dest))
		br_flood_deliver(br, skb);
	else if (is_multicast_ether_addr(dest)) {
#ifdef CONFIG_NETPOLL
		if (unlikely(netpoll_tx_running(dev))) {
			br_flood_deliver(br, skb);
			goto out;
		}
#endif
		if (br_multicast_rcv(br, NULL, skb)) {
			kfree_skb(skb);
			goto out;
		}

		mdst = br_mdb_get(br, skb);
		if (mdst || BR_INPUT_SKB_CB_MROUTERS_ONLY(skb))
			br_multicast_deliver(mdst, skb);
		else
			br_flood_deliver(br, skb);
	} else if ((dst = __br_fdb_get(br, dest)) != NULL)
		br_deliver(dst->dst, skb);
	else
		br_flood_deliver(br, skb);

out:
	rcu_read_unlock();
	return NETDEV_TX_OK;
}

static int br_dev_init(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	br->stats = netdev_alloc_pcpu_stats(struct br_cpu_netstats);
	if (!br->stats)
		return -ENOMEM;

	return 0;
}

static int br_dev_open(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	netdev_update_features(dev);
	netif_start_queue(dev);
	br_stp_enable_bridge(br);
	br_multicast_open(br);

	return 0;
}

static void br_dev_set_multicast_list(struct net_device *dev)
{
}

static int br_dev_stop(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	br_stp_disable_bridge(br);
	br_multicast_stop(br);

	netif_stop_queue(dev);

	return 0;
}

static struct rtnl_link_stats64 *br_get_stats64(struct net_device *dev,
						struct rtnl_link_stats64 *stats)
{
	struct net_bridge *br = netdev_priv(dev);
	struct br_cpu_netstats tmp, sum = { 0 };
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		unsigned int start;
		const struct br_cpu_netstats *bstats
			= per_cpu_ptr(br->stats, cpu);
		do {
			start = u64_stats_fetch_begin_bh(&bstats->syncp);
			memcpy(&tmp, bstats, sizeof(tmp));
		} while (u64_stats_fetch_retry_bh(&bstats->syncp, start));
		sum.tx_bytes   += tmp.tx_bytes;
		sum.tx_packets += tmp.tx_packets;
		sum.rx_bytes   += tmp.rx_bytes;
		sum.rx_packets += tmp.rx_packets;
	}

	netdev_stats_to_stats64(stats, &dev->stats);
	stats->tx_bytes   = sum.tx_bytes;
	stats->tx_packets = sum.tx_packets;
	stats->rx_bytes   = sum.rx_bytes;
	stats->rx_packets = sum.rx_packets;

	return stats;
}

static int br_change_mtu(struct net_device *dev, int new_mtu)
{
	struct net_bridge *br = netdev_priv(dev);
	if (new_mtu < 68 || new_mtu > br_min_mtu(br))
		return -EINVAL;

	dev->mtu = new_mtu;

#ifdef CONFIG_BRIDGE_NETFILTER
	/* remember the MTU in the rtable for PMTU */
	dst_metric_set(&br->fake_rtable.dst, RTAX_MTU, new_mtu);
#endif

	return 0;
}

/* Allow setting mac address to any valid ethernet address. */
static int br_set_mac_address(struct net_device *dev, void *p)
{
	struct net_bridge *br = netdev_priv(dev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	/* dev_set_mac_addr() can be called by a master device on bridge's
	 * NETDEV_UNREGISTER, but since it's being destroyed do nothing
	 */
	if (dev->reg_state != NETREG_REGISTERED)
		return -EBUSY;

	spin_lock_bh(&br->lock);
	if (compare_ether_addr(dev->dev_addr, addr->sa_data)) {
		dev->addr_assign_type &= ~NET_ADDR_RANDOM;
		memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
		br_fdb_change_mac_address(br, addr->sa_data);
		br_stp_change_bridge_id(br, addr->sa_data);
	}
	br->flags |= BR_SET_MAC_ADDR;
	spin_unlock_bh(&br->lock);

	return 0;
}

static void br_getinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy(info->driver, "bridge");
	strcpy(info->version, BR_VERSION);
	strcpy(info->fw_version, "N/A");
	strcpy(info->bus_info, "N/A");
}

static netdev_features_t br_fix_features(struct net_device *dev,
	netdev_features_t features)
{
	struct net_bridge *br = netdev_priv(dev);

	return br_features_recompute(br, features);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void br_poll_controller(struct net_device *br_dev)
{
}

static void br_netpoll_cleanup(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);
	struct net_bridge_port *p, *n;

	list_for_each_entry_safe(p, n, &br->port_list, list) {
		br_netpoll_disable(p);
	}
}

static int br_netpoll_setup(struct net_device *dev, struct netpoll_info *ni)
{
	struct net_bridge *br = netdev_priv(dev);
	struct net_bridge_port *p, *n;
	int err = 0;

	list_for_each_entry_safe(p, n, &br->port_list, list) {
		if (!p->dev)
			continue;

		err = br_netpoll_enable(p);
		if (err)
			goto fail;
	}

out:
	return err;

fail:
	br_netpoll_cleanup(dev);
	goto out;
}

int br_netpoll_enable(struct net_bridge_port *p)
{
	struct netpoll *np;
	int err = 0;

	np = kzalloc(sizeof(*p->np), GFP_KERNEL);
	err = -ENOMEM;
	if (!np)
		goto out;

	np->dev = p->dev;
	strlcpy(np->dev_name, p->dev->name, IFNAMSIZ);

	err = __netpoll_setup(np);
	if (err) {
		kfree(np);
		goto out;
	}

	p->np = np;

out:
	return err;
}

void br_netpoll_disable(struct net_bridge_port *p)
{
	struct netpoll *np = p->np;

	if (!np)
		return;

	p->np = NULL;

	/* Wait for transmitting packets to finish before freeing. */
	synchronize_rcu_bh();

	__netpoll_cleanup(np);
	kfree(np);
}

#endif

static int br_add_slave(struct net_device *dev, struct net_device *slave_dev)

{
	struct net_bridge *br = netdev_priv(dev);

	return br_add_if(br, slave_dev);
}

static int br_del_slave(struct net_device *dev, struct net_device *slave_dev)
{
	struct net_bridge *br = netdev_priv(dev);

	return br_del_if(br, slave_dev);
}

static const struct ethtool_ops br_ethtool_ops = {
	.get_drvinfo    = br_getinfo,
	.get_link	= ethtool_op_get_link,
};

static const struct net_device_ops br_netdev_ops = {
	.ndo_open		 = br_dev_open,
	.ndo_stop		 = br_dev_stop,
	.ndo_init		 = br_dev_init,
	.ndo_start_xmit		 = br_dev_xmit,
	.ndo_get_stats64	 = br_get_stats64,
	.ndo_set_mac_address	 = br_set_mac_address,
	.ndo_set_rx_mode	 = br_dev_set_multicast_list,
	.ndo_change_mtu		 = br_change_mtu,
	.ndo_do_ioctl		 = br_dev_ioctl,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_netpoll_setup	 = br_netpoll_setup,
	.ndo_netpoll_cleanup	 = br_netpoll_cleanup,
	.ndo_poll_controller	 = br_poll_controller,
#endif
	.ndo_add_slave		 = br_add_slave,
	.ndo_del_slave		 = br_del_slave,
	.ndo_fix_features        = br_fix_features,
};

static void br_dev_free(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	free_percpu(br->stats);
	free_netdev(dev);
}

static struct device_type br_type = {
	.name	= "bridge",
};

void br_dev_setup(struct net_device *dev)
{
	struct net_bridge *br = netdev_priv(dev);

	eth_hw_addr_random(dev);
	ether_setup(dev);

	dev->netdev_ops = &br_netdev_ops;
	dev->destructor = br_dev_free;
	SET_ETHTOOL_OPS(dev, &br_ethtool_ops);
	SET_NETDEV_DEVTYPE(dev, &br_type);
	dev->tx_queue_len = 0;
	dev->priv_flags = IFF_EBRIDGE;

	dev->features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA |
			NETIF_F_GSO_MASK | NETIF_F_HW_CSUM | NETIF_F_LLTX |
			NETIF_F_NETNS_LOCAL | NETIF_F_HW_VLAN_TX;
	dev->hw_features = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_HIGHDMA |
			   NETIF_F_GSO_MASK | NETIF_F_HW_CSUM |
			   NETIF_F_HW_VLAN_TX;

	br->dev = dev;
	spin_lock_init(&br->lock);
	INIT_LIST_HEAD(&br->port_list);
	spin_lock_init(&br->hash_lock);

	br->bridge_id.prio[0] = 0x80;
	br->bridge_id.prio[1] = 0x00;

	memcpy(br->group_addr, br_group_address, ETH_ALEN);

	br->stp_enabled = BR_NO_STP;
	br->group_fwd_mask = BR_GROUPFWD_DEFAULT;

	br->designated_root = br->bridge_id;
	br->bridge_max_age = br->max_age = 20 * HZ;
	br->bridge_hello_time = br->hello_time = 2 * HZ;
	br->bridge_forward_delay = br->forward_delay = 15 * HZ;
	br->ageing_time = 300 * HZ;

	br_netfilter_rtable_init(br);
	br_stp_timer_init(br);
	br_multicast_init(br);
}
