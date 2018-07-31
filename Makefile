PROJECT =	autoclave
OPTIMIZE =	-O3
WARN =		-Wall -pedantic -Wextra
CSTD +=		-std=c99 -D_POSIX_C_SOURCE=200112L
CDEFS +=
CFLAGS +=	${CSTD} -g ${WARN} ${CDEFS} ${OPTIMIZE}
LDFLAGS +=

BUILD =		build
SRC =		src
EXAMPLES = 	examples
MAN =		man

EXAMPLE_PROGS=	${BUILD}/crash_example \
		${BUILD}/deadlock_example \
		${BUILD}/fail_if_argv1_eq_5 \
		${BUILD}/gdb_it \
		${BUILD}/tick_then_die \
		${BUILD}/tick_then_okay \
		${BUILD}/tick_then_wait \


all: ${BUILD}/${PROJECT} ${EXAMPLE_PROGS}

OBJS=		${BUILD}/main.o

# Basic targets

${BUILD}/${PROJECT}: ${OBJS}
	${CC} -o $@ $< ${LDFLAGS}

${BUILD}/%: ${EXAMPLES}/%.o
	${CC} -o $@ $< ${LDFLAGS} -lpthread

clean:
	rm -rf ${BUILD}

${BUILD}/%.o: ${SRC}/%.c ${SRC}/*.h | ${BUILD}
	${CC} -c -o $@ ${CFLAGS} $<

${BUILD}/%.o: ${EXAMPLES}/%.c ${SRC}/*.h | ${BUILD}
	${CC} -o $@ ${CFLAGS} $<

# For the example programs that are shell scripts,
# just copy them into build.
${BUILD}/%: ${EXAMPLES}/% | ${BUILD}
	cp $< $@

${BUILD}/*.o: Makefile ${SRC}/types.h

${BUILD}:
	mkdir -p $@

# Regenerate documentation (requires ronn)
docs: ${MAN}/${PROJECT}.1 ${MAN}/${PROJECT}.1.html

${MAN}/${PROJECT}.1: ${MAN}/${PROJECT}.1.ronn
	ronn --roff $<

${MAN}/${PROJECT}.1.html: ${MAN}/${PROJECT}.1.ronn
	ronn --html $<


# Installation
PREFIX ?=	/usr/local
INSTALL ?=	install
RM ?=		rm
MAN_DEST ?=	${PREFIX}/share/man

install:
	${INSTALL} -c ${BUILD}/${PROJECT} ${PREFIX}/bin
	${INSTALL} -c ${MAN}/${PROJECT}.1 ${MAN_DEST}/man1/

uninstall:
	${RM} -f ${PREFIX}/bin/${PROJECT}
	${RM} -f ${MAN_DEST}/man1/${PROJECT}.1
