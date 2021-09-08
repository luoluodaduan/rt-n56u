SRC_NAME=ebtables-2.0.10-4

all:
	$(MAKE) -j$(HOST_NCPU) -C $(SRC_NAME) static KERNEL_INCLUDES=$(KERNEL_HEADERS_PATH)

clean:
	$(MAKE) -C $(SRC_NAME) clean
	rm -f ebtables

romfs:
	$(ROMFSINST) $(SRC_NAME)/static /bin/ebtables
	$(ROMFSINST) $(SRC_NAME)/ethertypes /etc_ro/ethertypes
