SRC_NAME=curl-7.78.0

LDFLAGS += -Wl,-rpath-link=$(STAGEDIR)/lib

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
	autoreconf -f && \
	./configure \
		--prefix=$(STAGEDIR) \
		--enable-openssl-auto-load-config \
		--enable-static \
		--enable-shared \
		$(if $(CONFIG_IPV6),--enable-ipv6,--disable-ipv6) \
		--disable-ares \
		--disable-curldebug \
		--disable-debug \
		--disable-dict \
		--disable-gopher \
		--disable-imap \
		--disable-ldap \
		--disable-ldaps \
		--disable-libcurl-option \
		--disable-manual \
		--disable-pop3 \
		--disable-rt \
		--disable-rtsp \
		--disable-smb \
		--disable-smtp \
		--disable-sspi \
		--disable-telnet \
		--disable-tftp \
		--disable-threaded-resolver \
		--disable-tls-srp \
		--disable-verbose \
		--without-ca-bundle \
		--without-gnutls \
		--without-krb4 \
		--without-libssh2 \
		--without-nss \
		--with-random=/dev/urandom \
		--with-zlib=$(STAGEDIR) \
		--with-ssl=$(STAGEDIR) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) clean ; \
	fi ; \
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME) install DESTDIR=""

romfs:
ifeq ($(CONFIG_FIRMWARE_INCLUDE_CURL),y)
	$(ROMFSINST) -d $(DESTDIR)/bin/curl /usr/bin/curl
endif
	cp -fP $(DESTDIR)/lib/libcurl.so* $(ROMFSDIR)/lib
