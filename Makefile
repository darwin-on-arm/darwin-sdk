SDK=DarwinARM
TOOLCHAIN=DarwinARM

DEVELOPER_DIR ?= /opt/Developer
TOOLCHAIN_DIR ?= $(DEVELOPER_DIR)/Toolchains/$(TOOLCHAIN).toolchain
SDKROOT ?= $(DEVELOPER_DIR)/SDKs/$(SDK).sdk

GNUMAKE_DIRS := \
	developer_cmds \
	kext_tools \
	migcom \
	xcode_tools \
	image3maker \
	AvailabilityVersions

AUTOTOOLS_DIRS = \
	cctools

define do_make
	@for dir in $1; do \
		make -C $$dir RC_ARCHS=$(RC_ARCHS) DEVELOPER_DIR=$(DEVELOPER_DIR) DESTDIR=$(DESTDIR) SDKROOT=$(SDKROOT) $2; \
	done
endef

define do_autoconf_and_make
	@for dir in $1; do \
		cd $$dir; \
		./builder.sh $2 $3; \
	done
endef

all:
	$(call do_make, $(GNUMAKE_DIRS), all)
	$(call do_autoconf_and_make, $(AUTOTOOLS_DIRS), do_build)

install: all
	$(call do_make, $(GNUMAKE_DIRS), install)
	$(call do_autoconf_and_make, $(AUTOTOOLS_DIRS), do_install, $(DESTDIR)/$(TOOLCHAIN_DIR))

clean:
	$(call do_make, $(GNUMAKE_DIRS), clean)
	$(call do_autoconf_and_make, $(AUTOTOOLS_DIRS), do_clean)
