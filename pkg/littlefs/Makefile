PKG_NAME=littlefs
PKG_URL=https://github.com/ARMmbed/littlefs.git
# v1.7.2
PKG_VERSION=7e110b44c0e796dc56e2fe86587762d685653029
PKG_LICENSE=Apache-2.0

include $(RIOTBASE)/pkg/pkg.mk

CFLAGS += -Wno-format
# GCC 4.9 bug (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=64480)
# used by MIPS
CFLAGS += -Wno-missing-field-initializers

# Disable debug printing
ifneq ($(DEVELHELP),1)
  CFLAGS += -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR
endif

all:
	"$(MAKE)" -C $(PKG_BUILDDIR) -f $(RIOTBASE)/Makefile.base
