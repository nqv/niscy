# niscy - Not Intelligent SMTP Client Yet

ifdef CONFIG_USER_NISC_WITH_AXTLS
# uClinux
TLS = AXTLS
AXTLS_CFLAGS = -I${ROOTDIR}/lib/axtls/_stage/include
AXTLS_LDFLAGS = -L${ROOTDIR}/lib/axtls/_stage/lib
AXTLS_LIBS = ${ROOTDIR}/lib/axtls/_stage/lib/libaxtls.a
else
TLS ?= AXTLS
AXTLS_CFLAGS ?= -I/mnt/src/axtls/trunk/_stage/include
AXTLS_LDFLAGS ?= -L/mnt/src/axtls/trunk/_stage/lib
AXTLS_LIBS ?= /mnt/src/axtls/trunk/_stage/lib/libaxtls.a
endif

CFLAGS += -Wall -Wextra
#CFLAGS += -fstack-usage

ifeq (${DEBUG},1)
CFLAGS += -Werror -O0 -g -DDEBUG
else
CFLAGS += -DNDEBUG
endif

ifeq (${TLS},AXTLS)
CFLAGS += -DNI_AXTLS
CFLAGS += ${AXTLS_CFLAGS}
LDFLAGS += ${AXTLS_LDFLAGS}
LIBS += ${AXTLS_LIBS}
endif

