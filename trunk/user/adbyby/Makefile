THISDIR = $(shell pwd)

all:

clean:

romfs:
	$(ROMFSINST) /etc_ro/adbyby.tar.gz
	$(ROMFSINST) -p +x $(THISDIR)/adbyby.sh /usr/bin/adbyby.sh
