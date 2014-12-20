PROG := image3maker

CFLAGS :=
LFLAGS :=

CC := $(shell which clang)

CFLAGS += \
	-O2 \
	-pipe

C_SRCS := \
	image3maker.c

OBJS := \
	$(patsubst %.c,%.o, $(filter %.c,$(C_SRCS)))

%.c.o:
	$(CC) -x c $(CFLAGS) -c $< -o $@

all: $(OBJS)
	$(CC) $(OBJS) -o $(PROG) $(LFLAGS)

install: all
	install -d $(DESTDIR)/$(DEVELOPER_DIR)/usr/bin
	install -s -m 755 $(PROG) $(DESTDIR)/$(DEVELOPER_DIR)/usr/bin/$(PROG)

clean:
	rm -f $(OBJS) $(PROG)
