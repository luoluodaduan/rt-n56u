SRC_NAME=ipset-6.x

THISDIR = $(shell pwd)

CFLAGS += -fvisibility=hidden

all: unzip config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)
	$(MAKE) modules -j$(HOST_NCPU) -C $(SRC_NAME)

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
		libmnl_CFLAGS="-I$(STAGEDIR)/include" \
		libmnl_LIBS="-L$(STAGEDIR)/lib -lmnl" \
		--with-kmod=yes \
		--with-kbuild=$(ROOTDIR)/$(LINUXDIR) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) modules_clean ; \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) $(THISDIR)/$(SRC_NAME)/src/ipset /sbin/ipset
