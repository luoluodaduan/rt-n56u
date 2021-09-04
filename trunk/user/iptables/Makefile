SRC_NAME=iptables-1.6.2

THISDIR = $(shell pwd)

CFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden
LDFLAGS += -Wl,--gc-sections

all: unzip config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

unzip:
	( tar Jxf $(SRC_NAME).tar.xz )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) && \
	./autogen.sh && \
	export PKG_CONFIG_PATH=$(STAGEDIR)/lib/pkgconfig && \
	./configure \
		--prefix= \
		--enable-static \
		--disable-shared \
		--disable-nftables \
		--disable-connlabel \
		--with-xt-lock-name=/var/run/xtables.lock \
		--with-xtlibdir=/usr/lib/xtables \
		--with-kernel=$(ROOTDIR)/$(LINUXDIR) \
		$(if $(CONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	mkdir -p $(ROMFSDIR)/usr/lib/xtables
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/iptables/xtables-multi /bin/xtables-multi
	$(ROMFSINST) -s xtables-multi /bin/iptables
	$(ROMFSINST) -s xtables-multi /bin/iptables-restore
	$(ROMFSINST) -s xtables-multi /bin/iptables-save
ifdef CONFIG_IPV6
	$(ROMFSINST) -s xtables-multi /bin/ip6tables
	$(ROMFSINST) -s xtables-multi /bin/ip6tables-restore
	$(ROMFSINST) -s xtables-multi /bin/ip6tables-save
endif
