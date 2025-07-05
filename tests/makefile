GCC = gcc 
SRC = $(wildcard ../*.c)
INC = -I.. -I../lib
CFLAGS = -Wall -Wextra -luring -g

PROGS = httpsrv

all: ${PROGS}
${PROGS}: %: %.c
	$(GCC) ${SRC} $< -o $@ $(INC) $(CFLAGS)

clean:
	rm -f ${PROGS} *.o

.PHONY: all clean ${PROGS}
