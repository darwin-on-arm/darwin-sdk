PROG := rpcgen

CFLAGS :=
LFLAGS :=

ifneq ($(RC_ARCHS),)
	CFLAGS += -arch $(RC_ARCHS)
endif

CC := $(shell which clang)

CFLAGS += \
	-Wall \
	-Werror \
	-O2

C_SRCS := \
	rpc_clntout.c \
	rpc_cout.c \
	rpc_hout.c \
	rpc_main.c \
	rpc_parse.c \
	rpc_sample.c \
	rpc_scan.c \
	rpc_svcout.c \
	rpc_tblout.c \
	rpc_util.c

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
