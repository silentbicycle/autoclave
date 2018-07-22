PROJECT =	autoclave
OPTIMIZE =	-O3
WARN =		-Wall -pedantic -Wextra
CSTD +=		-std=c99 -D_POSIX_C_SOURCE=200112L #-D_C99_SOURCE
CDEFS +=
CFLAGS +=	${CSTD} -g ${WARN} ${CDEFS} ${OPTIMIZE}
LDFLAGS +=

EXAMPLES=	examples/crash_example \
		examples/deadlock_example

all: ${PROJECT} ${EXAMPLES}

OBJS=

# Basic targets

${PROJECT}: main.o ${OBJS}
	${CC} -o $@ main.o ${OBJS} ${LDFLAGS}

examples/%: examples/%.o
	${CC} -o $@ $^ ${OBJS} ${LDFLAGS} -lpthread

clean:
	rm -f ${PROJECT} *.o *.core ${EXAMPLES}

main.o: types.h Makefile

# Regenerate documentation (requires ronn)
docs: man/${PROJECT}.1 man/${PROJECT}.1.html

man/${PROJECT}.1: man/${PROJECT}.1.ronn
	ronn --roff $<

man/${PROJECT}.1.html: man/${PROJECT}.1.ronn
	ronn --html $<

# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm

install:
	${INSTALL} -c ${PROJECT} ${PREFIX}/bin

uninstall:
	${RM} -f ${PREFIX}/bin/${PROJECT}
