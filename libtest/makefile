GCC=gcc
CFLAGS=-I.. -L.. -lhunk 
PROGS=serv

all: ${PROGS}
${PROGS}: %: %.c
	$(GCC) $< -o $@ ${INC} ${CFLAGS}

clean:
	rm -f ${PROGS} *.o

.PHONY: all clean ${PROGS}
