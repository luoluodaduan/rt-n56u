USERDIR = $(ROOTDIR)/user

CFLAGS += -I. -I$(USERDIR)/shared -I$(USERDIR)/shared/include
LDFLAGS += -L$(USERDIR)/shared -lshared

CFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden
LDFLAGS += -Wl,--gc-sections

OBJS=infosvr.o asus.o

all: infosvr

%.o: %.c
	$(CC) $(CFLAGS) -c $<

infosvr: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o infosvr $(LIBS)
	$(STRIP) $@

romfs:
	$(ROMFSINST) /usr/sbin/infosvr

clean:
	rm -f *.o *~ infosvr
