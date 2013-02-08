# niscy - Not Intelligent SMTP Client Yet

include config.mk

BIN = niscy

OBJ = \
	src/smtp.o \
	src/base64.o \
	src/niscy.o

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

romfs:
	${ROMFSINST} ${BIN} /bin/${BIN}

clean:
	rm -rf ${BIN} ${BIN}.gdb src/*.o src/*.su src/*.gdb
