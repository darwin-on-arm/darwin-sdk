DEVELOPER_DIR ?= /opt/Developer

DIRS := \
	asa \
	ctags \
	indent \
	lorder \
	mkdep \
	rpcgen \
	unifdef \
	vgrind \
	what

define do_make
	for dir in $1; do \
		make -C $$dir RC_ARCHS=$(RC_ARCHS) DEVELOPER_DIR=$(DEVELOPER_DIR) DESTDIR=$(DESTDIR) $2; \
	done
endef

all:
	$(call do_make, $(DIRS), all)

install:
	$(call do_make, $(DIRS), install)

clean:
	$(call do_make, $(DIRS), clean)
