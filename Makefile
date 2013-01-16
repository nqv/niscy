BIN = nisc

OBJ = \
	src/smtp.o \
	src/base64.o \
	src/nisc.o

CFLAGS += -Wall -Wextra

ifeq (${DEBUG},1)
CFLAGS += -Werror -O0 -g -DDEBUG
else
CFLAGS += -DNDEBUG
endif

CFLAGS += -I/mnt/src/axtls/trunk/_stage/include
LDFLAGS += -L/mnt/src/axtls/trunk/_stage/lib
LIBS += -laxtls

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
	@rm -rf ${BIN} src/*.o
