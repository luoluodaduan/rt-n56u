SRC_NAME=miniupnpd-2.x

# reference to iptables package
IPT_VERSION:=iptables-1.4.21

# disable support IPv6 yet, because no profit w/o IGDv2
# e.g. Windows not supported IGDv2 and UPnP not worked
ENABLE_IPV6:=0

IPTABLESPATH=$(ROOTDIR)/user/iptables/$(IPT_VERSION)

all:
	tar -xJf $(SRC_NAME).tar.xz
	cd $(SRC_NAME) && ./genconfig.sh
	$(MAKE) -j$(HOST_NCPU) -f Makefile.linux -C $(SRC_NAME) IPTABLESPATH=$(IPTABLESPATH) ENABLE_IPV6=$(ENABLE_IPV6)

clean:
	$(MAKE) -f Makefile.linux -C $(SRC_NAME) clean
	rm -f miniupnpd

romfs:
	cp $(SRC_NAME)/miniupnpd .
	$(STRIP) miniupnpd
	$(ROMFSINST) /usr/bin/miniupnpd
