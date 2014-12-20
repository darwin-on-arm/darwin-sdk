DEVELOPER_DIR ?= /opt/Developer

DIRS := \
	setsegname \
	kextsymboltool

define do_make
	@for dir in $1; do \
		make -C $$dir DEVELOPER_DIR=$(DEVELOPER_DIR) DESTDIR=$(DESTDIR) $2; \
	done
endef

all:
	$(call do_make, $(DIRS), all)

install:
	$(call do_make, $(DIRS), install)

clean:
	$(call do_make, $(DIRS), clean)
