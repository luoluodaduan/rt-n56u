SRC_NAME=pptpd-1.3.3
THISDIR=$(shell pwd)

CFLAGS  += -O2 -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

all: config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	./reconf ; \
	export ac_cv_header_libintl_h=no ; \
	export ac_cv_lib_intl_gettext=no ; \
	./configure \
		KDIR="$(ROOTDIR)/$(LINUXDIR)" \
		PPPDIR="$(ROOTDIR)/user/pppd" \
		PPPVER="2.4.7" \
		--prefix=/usr \
		--enable-bcrelay \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

romfs:
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/pptpd /usr/sbin/pptpd
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/pptpctrl /usr/sbin/pptpctrl
	$(ROMFSINST) -p +x $(THISDIR)/$(SRC_NAME)/bcrelay /usr/sbin/bcrelay
