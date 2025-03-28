/*
 *	IPv6 input
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>
 *	Ian P. Morris		<I.P.Morris@soton.ac.uk>
 *
 *	Based in linux/net/ipv4/ip_input.c
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
/* Changes
 *
 * 	Mitsuru KANDA @USAGI and
 * 	YOSHIFUJI Hideaki @USAGI: Remove ipv6_parse_exthdrs().
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <linux/mroute6.h>
#include <linux/slab.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/rawv6.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/xfrm.h>

#if IS_ENABLED(CONFIG_RA_HW_NAT)
#if defined(CONFIG_RA_HW_NAT_IPV6)
#include "../nat/hw_nat/ra_nat.h"
#endif
#endif

inline int ip6_rcv_finish( struct sk_buff *skb)
{
	if (skb_dst(skb) == NULL)
		ip6_route_input(skb);

	return dst_input(skb);
}

int ipv6_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	const struct ipv6hdr *hdr;
	u32 		pkt_len;
	struct inet6_dev *idev;
	struct net *net = dev_net(skb->dev);

	if (skb->pkt_type == PACKET_OTHERHOST) {
		kfree_skb(skb);
		return NET_RX_DROP;
	}

	rcu_read_lock();

	idev = __in6_dev_get(skb->dev);

	IP6_UPD_PO_STATS_BH(net, idev, IPSTATS_MIB_IN, skb->len);

	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL ||
	    !idev || unlikely(idev->cnf.disable_ipv6)) {
		IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INDISCARDS);
		goto drop;
	}

	memset(IP6CB(skb), 0, sizeof(struct inet6_skb_parm));

	/*
	 * Store incoming device index. When the packet will
	 * be queued, we cannot refer to skb->dev anymore.
	 *
	 * BTW, when we send a packet for our own local address on a
	 * non-loopback interface (e.g. ethX), it is being delivered
	 * via the loopback interface (lo) here; skb->dev = loopback_dev.
	 * It, however, should be considered as if it is being
	 * arrived via the sending interface (ethX), because of the
	 * nature of scoping architecture. --yoshfuji
	 */
	IP6CB(skb)->iif = skb_dst(skb) ? ip6_dst_idev(skb_dst(skb))->dev->ifindex : dev->ifindex;

	if (unlikely(!pskb_may_pull(skb, sizeof(*hdr))))
		goto err;

	hdr = ipv6_hdr(skb);

	if (hdr->version != 6)
		goto err;

	/*
	 * RFC4291 2.5.3
	 * The loopback address must not be used as the source address in IPv6
	 * packets that are sent outside of a single node. [..]
	 * A packet received on an interface with a destination address
	 * of loopback must be dropped.
	 */
	if ((ipv6_addr_loopback(&hdr->saddr) ||
	     ipv6_addr_loopback(&hdr->daddr)) &&
	     !(dev->flags & IFF_LOOPBACK))
		goto err;

	/* RFC4291 Errata ID: 3480
	 * Interface-Local scope spans only a single interface on a
	 * node and is useful only for loopback transmission of
	 * multicast.  Packets with interface-local scope received
	 * from another node must be discarded.
	 */
	if (!(skb->pkt_type == PACKET_LOOPBACK ||
	      dev->flags & IFF_LOOPBACK) &&
	    ipv6_addr_is_multicast(&hdr->daddr) &&
	    IPV6_ADDR_MC_SCOPE(&hdr->daddr) == 1)
		goto err;

	/* RFC4291 2.7
	 * Nodes must not originate a packet to a multicast address whose scope
	 * field contains the reserved value 0; if such a packet is received, it
	 * must be silently dropped.
	 */
	if (ipv6_addr_is_multicast(&hdr->daddr) &&
	    IPV6_ADDR_MC_SCOPE(&hdr->daddr) == 0)
		goto err;

	/*
	 * RFC4291 2.7
	 * Multicast addresses must not be used as source addresses in IPv6
	 * packets or appear in any Routing header.
	 */
	if (ipv6_addr_is_multicast(&hdr->saddr))
		goto err;

	/* While RFC4291 is not explicit about v4mapped addresses
	 * in IPv6 headers, it seems clear linux dual-stack
	 * model can not deal properly with these.
	 * Security models could be fooled by ::ffff:127.0.0.1 for example.
	 *
	 * https://tools.ietf.org/html/draft-itojun-v6ops-v4mapped-harmful-02
	 */
	if (ipv6_addr_v4mapped(&hdr->saddr))
		goto err;

	skb->transport_header = skb->network_header + sizeof(*hdr);
	IP6CB(skb)->nhoff = offsetof(struct ipv6hdr, nexthdr);

	pkt_len = ntohs(hdr->payload_len);

	/* pkt_len may be zero if Jumbo payload option is present */
	if (pkt_len || hdr->nexthdr != NEXTHDR_HOP) {
		if (pkt_len + sizeof(struct ipv6hdr) > skb->len) {
			IP6_INC_STATS_BH(net,
					 idev, IPSTATS_MIB_INTRUNCATEDPKTS);
			goto drop;
		}
		if (pskb_trim_rcsum(skb, pkt_len + sizeof(struct ipv6hdr))) {
			IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INHDRERRORS);
			goto drop;
		}
		hdr = ipv6_hdr(skb);
	}

	if (hdr->nexthdr == NEXTHDR_HOP) {
		if (ipv6_parse_hopopts(skb) < 0) {
			IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INHDRERRORS);
			rcu_read_unlock();
			return NET_RX_DROP;
		}
	}

	rcu_read_unlock();

	/* Must drop socket now because of tproxy. */
	skb_orphan(skb);

	return NF_HOOK(NFPROTO_IPV6, NF_INET_PRE_ROUTING, skb, dev, NULL,
		       ip6_rcv_finish);
err:
	IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INHDRERRORS);
drop:
	rcu_read_unlock();
	kfree_skb(skb);
	return NET_RX_DROP;
}

/*
 *	Deliver the packet to the host
 */


static int ip6_input_finish(struct sk_buff *skb)
{
	struct net *net = dev_net(skb_dst(skb)->dev);
	const struct inet6_protocol *ipprot;
	struct inet6_dev *idev;
	unsigned int nhoff;
	int nexthdr, raw;

	/*
	 *	Parse extension headers
	 */

	rcu_read_lock();
resubmit:
	idev = ip6_dst_idev(skb_dst(skb));
	if (!pskb_pull(skb, skb_transport_offset(skb)))
		goto discard;
	nhoff = IP6CB(skb)->nhoff;
	nexthdr = skb_network_header(skb)[nhoff];

	raw = raw6_local_deliver(skb, nexthdr);
	if ((ipprot = rcu_dereference(inet6_protos[nexthdr])) != NULL) {
		int ret;

		if (ipprot->flags & INET6_PROTO_FINAL) {
			const struct ipv6hdr *hdr;

			/* Free reference early: we don't need it any more,
			   and it may hold ip_conntrack module loaded
			   indefinitely. */
			nf_reset(skb);

			skb_postpull_rcsum(skb, skb_network_header(skb),
					   skb_network_header_len(skb));
			hdr = ipv6_hdr(skb);
			if (ipv6_addr_is_multicast(&hdr->daddr) &&
			    !ipv6_chk_mcast_addr(skb->dev, &hdr->daddr,
			    &hdr->saddr) &&
			    !ipv6_is_mld(skb, nexthdr))
				goto discard;
		}
#ifdef CONFIG_XFRM
		if (!(ipprot->flags & INET6_PROTO_NOPOLICY) &&
		    !xfrm6_policy_check(NULL, XFRM_POLICY_IN, skb))
			goto discard;
#endif

		ret = ipprot->handler(skb);
		if (ret > 0)
			goto resubmit;
		else if (ret == 0)
			IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INDELIVERS);
	} else {
		if (!raw) {
#ifdef CONFIG_XFRM
			if (xfrm6_policy_check(NULL, XFRM_POLICY_IN, skb)) {
#endif
				IP6_INC_STATS_BH(net, idev,
						 IPSTATS_MIB_INUNKNOWNPROTOS);
				icmpv6_send(skb, ICMPV6_PARAMPROB,
					    ICMPV6_UNK_NEXTHDR, nhoff);
#ifdef CONFIG_XFRM
			}
#endif
			kfree_skb(skb);
		} else {
			IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INDELIVERS);
			consume_skb(skb);
		}
	}
	rcu_read_unlock();
	return 0;

discard:
	IP6_INC_STATS_BH(net, idev, IPSTATS_MIB_INDISCARDS);
	rcu_read_unlock();
	kfree_skb(skb);
	return 0;
}


int ip6_input(struct sk_buff *skb)
{
#if IS_ENABLED(CONFIG_RA_HW_NAT)
#if defined(CONFIG_RA_HW_NAT_IPV6)
	FOE_ALG_MARK(skb);
#endif
#endif
	return NF_HOOK(NFPROTO_IPV6, NF_INET_LOCAL_IN, skb, skb->dev, NULL,
		       ip6_input_finish);
}

int ip6_mc_input(struct sk_buff *skb)
{
	const struct ipv6hdr *hdr;
	int deliver;

	IP6_UPD_PO_STATS_BH(dev_net(skb_dst(skb)->dev),
			 ip6_dst_idev(skb_dst(skb)), IPSTATS_MIB_INMCAST,
			 skb->len);

	hdr = ipv6_hdr(skb);
	deliver = ipv6_chk_mcast_addr(skb->dev, &hdr->daddr, NULL);

#ifdef CONFIG_IPV6_MROUTE
	/*
	 *      IPv6 multicast router mode is now supported ;)
	 */
	if (dev_net(skb->dev)->ipv6.devconf_all->mc_forwarding &&
	    !(ipv6_addr_type(&hdr->daddr) &
	      (IPV6_ADDR_LOOPBACK|IPV6_ADDR_LINKLOCAL)) &&
	    likely(!(IP6CB(skb)->flags & IP6SKB_FORWARDED))) {
		/*
		 * Okay, we try to forward - split and duplicate
		 * packets.
		 */
		struct sk_buff *skb2;
		struct inet6_skb_parm *opt = IP6CB(skb);

		/* Check for MLD */
		if (unlikely(opt->flags & IP6SKB_ROUTERALERT)) {
			/* Check if this is a mld message */
			struct icmp6hdr *icmp6;
			u8 nexthdr = hdr->nexthdr;
			__be16 frag_off;
			int offset;

			/* Check if the value of Router Alert
			 * is for MLD (0x0000).
			 */
			if (opt->ra == htons(IPV6_OPT_ROUTERALERT_MLD)) {
				deliver = 0;

				if (!ipv6_ext_hdr(nexthdr)) {
					/* BUG */
					goto out;
				}
				offset = ipv6_skip_exthdr(skb, sizeof(*hdr),
							  &nexthdr, &frag_off);
				if (offset < 0)
					goto out;

				if (nexthdr != IPPROTO_ICMPV6)
					goto out;

				if (!pskb_may_pull(skb, (skb_network_header(skb) +
						   offset + 1 - skb->data)))
					goto out;

				icmp6 = (struct icmp6hdr *)(skb_network_header(skb) + offset);

				switch (icmp6->icmp6_type) {
				case ICMPV6_MGM_QUERY:
				case ICMPV6_MGM_REPORT:
				case ICMPV6_MGM_REDUCTION:
				case ICMPV6_MLD2_REPORT:
					deliver = 1;
					break;
				}
				goto out;
			}
			/* unknown RA - process it normally */
		}

		if (deliver)
			skb2 = skb_clone(skb, GFP_ATOMIC);
		else {
			skb2 = skb;
			skb = NULL;
		}

		if (skb2) {
			ip6_mr_input(skb2);
		}
	}
out:
#endif
	if (likely(deliver))
		ip6_input(skb);
	else {
		/* discard */
		kfree_skb(skb);
	}

	return 0;
}
