#!/bin/sh

adbyby_ip_x=`nvram get adbyby_ip_x`
adbyby_rules_x=`nvram get adbyby_rules_x`
adbyby_set=`nvram get adbyby_set`
adbyby_update=`nvram get adbyby_update`
adbyby_update_hour=`nvram get adbyby_update_hour`
adbyby_update_min=`nvram get adbyby_update_min`
adbybyip_staticnum_x=`nvram get adbybyip_staticnum_x`
adbybyrules_staticnum_x=`nvram get adbybyrules_staticnum_x`
http_username=`nvram get http_username`
nvram set adbybyip_ip_road_x_0=""
nvram set adbybyip_ip_x_0=""
nvram set adbybyip_mac_x_0=""
nvram set adbybyip_name_x_0=""
nvram set adbybyrules_road_x_0=""
nvram set adbybyrules_x_0=""

adbyby_start() {
	sleep 5s
	if [ ! -f "/etc/storage/adbyby_adplus.sh" ]; then
		touch /etc/storage/adbyby_adplus.sh
		chmod 755 "/etc/storage/adbyby_adplus.sh"
	fi
	if [ ! -f "/etc/storage/adbyby_adesc.sh" ]; then
		touch /etc/storage/adbyby_adesc.sh
		chmod 755 "/etc/storage/adbyby_adesc.sh"
	fi
	if [ ! -f "/etc/storage/adbyby_adblack.sh" ]; then
		touch /etc/storage/adbyby_adblack.sh
		chmod 755 "/etc/storage/adbyby_adblack.sh"
	fi
	if [ ! -f "/etc/storage/adbyby_rules.sh" ]; then
		cat >/etc/storage/adbyby_rules.sh <<EOF
! Adbyby 自定义过滤规则 参考以下
! https://blog.csdn.net/weixin_40747900/article/details/104328954
! https://blog.csdn.net/qq_36450004/article/details/105660153
EOF
		chmod 755 "/etc/storage/adbyby_rules.sh"
	fi
	if [ ! -f "/tmp/adbyby/adbyby" ]; then
		tar -xzf "/etc_ro/adbyby.tar.gz" -C "/tmp"
	fi
	rm -f /etc/storage/dnsmasq-adbyby.d/*
	rm -f /tmp/adbyby/data/lazy.txt
	rm -f /tmp/adbyby/data/*.bak
	rm -f /tmp/adbyby/user1.txt
	rm -f /tmp/adbyby/user2.txt
	nvram set adbyby_user=0
	logger -t "adbyby" "正在启动并更新规则..."
	if [ $adbyby_rules_x -eq 1 ]; then
		for i in $(seq 1 $adbybyrules_staticnum_x); do
			j=`expr $i - 1`
			adbybyrules_xj=`nvram get adbybyrules_x$j`
			adbybyrules_road_xj=`nvram get adbybyrules_road_x$j`
			if [ $adbybyrules_road_xj -ne 0 ]; then
				logger -t "adbyby" "正在下载合并$adbybyrules_xj"
				curl -k -s -o /tmp/adbyby/user1.txt --connect-timeout 8 --retry 3 $adbybyrules_xj
				grep -vE '^(#|!|！|\[)' /tmp/adbyby/user1.txt | sort -u | grep -v "^$" >>/tmp/adbyby/user2.txt
				rm -f /tmp/adbyby/user1.txt
			fi
		done
	fi
	grep -vE '^(#|!|！|\[)' /etc/storage/adbyby_rules.sh | sort -u | grep -v "^$" >>/tmp/adbyby/user2.txt
	grep -vE '#|-abp-|\$ping|,ping|\$websocket|,websocket|\$webrtc|,webrtc|\$elemhide|,elemhide|\$generic|,generic|csp=script-src|rewrite=' /tmp/adbyby/user2.txt | sort -u | grep -v "^$" >/tmp/adbyby/data/lazy.txt
	rm -f /tmp/adbyby/user2.txt
	nvram set adbyby_user=`cat /tmp/adbyby/data/lazy.txt | wc -l`
	/tmp/adbyby/adbyby >/dev/null 2>&1 &
	sleep 480s
	logger -t "adbyby" "规则更新完成。"
	mkdir -p /etc/storage/dnsmasq-adbyby.d
	sort -f -u /etc/storage/adbyby_adesc.sh | awk '!/^$/&&!/^#/&&!/^!/{printf("ipset=/%s/'"adbyby_esc"'\n",$0)}' >/etc/storage/dnsmasq-adbyby.d/02.adesc
	sort -f -u /etc/storage/adbyby_adblack.sh | awk '!/^$/&&!/^#/&&!/^!/{printf("address=/%s/'"0.0.0.0"'\n",$0)}' >/etc/storage/dnsmasq-adbyby.d/03.adblack
	sed -i '/dnsmasq-adbyby/d' /etc/storage/dnsmasq/dnsmasq.conf
	cat >>/etc/storage/dnsmasq/dnsmasq.conf <<EOF
conf-dir=/etc/storage/dnsmasq-adbyby.d
EOF
	if [ $adbyby_set -eq 1 ]; then
		sort -f -u /etc/storage/adbyby_adplus.sh | awk '!/^$/&&!/^#/&&!/^!/{printf("ipset=/%s/'"adbyby_wan"'\n",$0)}' >/etc/storage/dnsmasq-adbyby.d/01.adplus
	fi
	iptables-save | grep ADBYBY >/dev/null || iptables -t nat -N ADBYBY
	iptables -t nat -A ADBYBY -d 0.0.0.0/8 -j RETURN
	iptables -t nat -A ADBYBY -d 10.0.0.0/8 -j RETURN
	iptables -t nat -A ADBYBY -d 127.0.0.0/8 -j RETURN
	iptables -t nat -A ADBYBY -d 169.254.0.0/16 -j RETURN
	iptables -t nat -A ADBYBY -d 172.16.0.0/12 -j RETURN
	iptables -t nat -A ADBYBY -d 192.168.0.0/16 -j RETURN
	iptables -t nat -A ADBYBY -d 224.0.0.0/4 -j RETURN
	iptables -t nat -A ADBYBY -d 240.0.0.0/4 -j RETURN
	ipset -N adbyby_esc hash:ip
	iptables -t nat -A ADBYBY -m set --match-set adbyby_esc dst -j RETURN
	if [ $adbyby_ip_x -eq 1 ]; then
		if [ $adbybyip_staticnum_x -ne 0 ]; then
			logger -t "adbyby" "设置内网IP过滤控制。"
			for i in $(seq 1 $adbybyip_staticnum_x); do
				j=`expr $i - 1`
				adbybyip_ip_xj=`nvram get adbybyip_ip_x$j`
				adbybyip_ip_road_xj=`nvram get adbybyip_ip_road_x$j`
				case $adbybyip_ip_road_xj in
				0)
					iptables -t nat -A ADBYBY -s $adbybyip_ip_xj -j RETURN
					logger -t "adbyby" "设置$adbybyip_ip_xj走直连模式。"
					;;
				1)
					iptables -t nat -A ADBYBY -s $adbybyip_ip_xj -p tcp -j REDIRECT --to-ports 8118
					iptables -t nat -A ADBYBY -s $adbybyip_ip_xj -j RETURN
					logger -t "adbyby" "设置$adbybyip_ip_xj走全局过滤。"
					;;
				2)
					ipset -N adbyby_wan hash:ip
					iptables -t nat -A ADBYBY -m set --match-set adbyby_wan dst -s $adbybyip_ip_xj -p tcp -j REDIRECT --to-ports 8118
					sort -f -u /etc/storage/adbyby_adplus.sh | awk '!/^$/&&!/^#/&&!/^!/{printf("ipset=/%s/'"adbyby_wan"'\n",$0)}' >/etc/storage/dnsmasq-adbyby.d/01.adplus
					logger -t "adbyby" "设置$adbybyip_ip_xj走Plus+过滤。"
					;;
				esac
			done
		fi
	fi
	case $adbyby_set in
	0)
		iptables -t nat -A ADBYBY -p tcp -j REDIRECT --to-ports 8118
		;;
	1)
		ipset -N adbyby_wan hash:ip
		iptables -t nat -A ADBYBY -m set --match-set adbyby_wan dst -p tcp -j REDIRECT --to-ports 8118
		;;
	2)
		iptables -t nat -A ADBYBY -d 0.0.0.0/24 -j RETURN
		;;
	esac
	iptables -t nat -I PREROUTING -p tcp --dport 80 -j ADBYBY
	iptables-save | grep -E "ADBYBY|^\*|^COMMIT" | sed -e "s/^-A \(OUTPUT\|PREROUTING\)/-I \1 1/" | sort -u | grep -v "^$" >/tmp/adbyby_iptables.save
	if [ -f "/tmp/adbyby_iptables.save" ]; then
		logger -t "adbyby" "保存防火墙规则成功。"
	else
		logger -t "adbyby" "保存防火墙规则失败！可能会造成重启后过滤广告失效，需要手动关闭再打开ADBYBY！"
	fi
	if [ $adbyby_update -eq 0 ]; then
		sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
		cat >>/etc/storage/cron/crontabs/$http_username <<EOF
$adbyby_update_min $adbyby_update_hour * * * /bin/sh /usr/bin/adbyby.sh uprules >/dev/null 2>&1
EOF
		logger -t "adbyby" "每天$adbyby_update_hour时$adbyby_update_min分，自动更新规则。"
	fi
	if [ $adbyby_update -eq 1 ]; then
		sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
		cat >>/etc/storage/cron/crontabs/$http_username <<EOF
*/$adbyby_update_min */$adbyby_update_hour * * * /bin/sh /usr/bin/adbyby.sh uprules >/dev/null 2>&1
EOF
		logger -t "adbyby" "每隔$adbyby_update_hour时$adbyby_update_min分，自动更新规则。"
	fi
	if [ $adbyby_update -eq 2 ]; then
		sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
	fi
	sleep 2s
	/sbin/restart_dhcpd
	logger -t "adbyby" "启动完成。"
}

adbyby_close() {
	killall -q adbyby
	iptables -t nat -D PREROUTING -p tcp --dport 80 -j ADBYBY 2>/dev/null
	iptables -t nat -F ADBYBY 2>/dev/null
	iptables -t nat -X ADBYBY 2>/dev/null
	ipset -F adbyby_esc 2>/dev/null
	ipset -X adbyby_esc 2>/dev/null
	ipset -F adbyby_wan 2>/dev/null
	ipset -X adbyby_wan 2>/dev/null
	sed -i '/adbyby/d' /etc/storage/cron/crontabs/$http_username
	sed -i '/dnsmasq-adbyby/d' /etc/storage/dnsmasq/dnsmasq.conf
	rm -f /etc/storage/dnsmasq-adbyby.d/*
	rm -f /tmp/adbyby/data/lazy.txt
	rm -f /tmp/adbyby/data/*.bak
	nvram set adbyby_user=0
	sleep 2s
	/sbin/restart_dhcpd
	logger -t "adbyby" "已关闭。"
}

adbyby_uprules() {
	adbyby_close
	adbyby_start
}

case $1 in
start)
	adbyby_start
	;;
stop)
	adbyby_close
	;;
uprules)
	adbyby_uprules
	;;
*)
	echo "check"
	;;
esac
