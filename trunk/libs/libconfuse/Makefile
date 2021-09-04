SRC_NAME=confuse-3.3

all: unzip config_test
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME)

unzip:
	( tar -Jxf $(SRC_NAME).tar.xz )

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) && \
	./configure \
		--disable-examples \
		--enable-static \
		--disable-shared \
		--prefix=$(STAGEDIR) \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f config_done

install:
	$(MAKE) -C $(SRC_NAME) install DESTDIR=""

romfs:
