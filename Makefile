# NISC - Not Intelligent SMTP Client

SSL ?= AXTLS
AXTLS_CFLAGS ?= -I/mnt/src/axtls/trunk/_stage/include
AXTLS_LDFLAGS ?= -L/mnt/src/axtls/trunk/_stage/lib
AXTLS_LIBS ?= -laxtls

BIN = nisc

OBJ = \
	src/smtp.o \
	src/base64.o \
	src/nisc.o

CFLAGS += -Wall -Wextra
#CFLAGS += -fstack-usage

ifeq (${DEBUG},1)
CFLAGS += -Werror -O0 -g -DDEBUG
else
CFLAGS += -DNDEBUG
endif

ifeq (${SSL},AXTLS)
CFLAGS += -DNISC_SSL -DNISC_AXTLS
CFLAGS += ${AXTLS_CFLAGS}
LDFLAGS += ${AXTLS_LDFLAGS}
LIBS += ${AXTLS_LIBS}
endif

all: options ${BIN}

${BIN}: ${OBJ}
	@echo CC -o $@
	@${CC} ${LDFLAGS} -o $@ ${OBJ} ${LIBS}

options:
	@echo ${PGK} build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "LIBS    = ${LIBS}"
	@echo "CC      = ${CC}"

%.o: %.c
	@echo CC $<
	@${CC} ${CFLAGS} -o $@ -c $<

clean:
	@rm -rf ${BIN} src/*.o src/*.su
