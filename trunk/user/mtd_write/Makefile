USERDIR = $(ROOTDIR)/user
SHDIR = $(ROOTDIR)/user/shared

EXEC = mtd_write
OBJS = mtd.o

CFLAGS += -I$(SHDIR)/include
LDFLAGS += -static

CFLAGS += -ffunction-sections -fdata-sections -fvisibility=hidden
LDFLAGS += -Wl,--gc-sections

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	rm -f $(EXEC) *.o
