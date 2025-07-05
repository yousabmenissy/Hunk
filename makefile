CC = gcc
URING=external/liburing
INC = -I. -Ilib -I$(URING)/src/include
CFLAGS = -Wall -Wextra -O3
SRCS = $(wildcard ./*.c)
OBJS = $(SRCS:.c=.o)
LIB = libhunk.a

# Build the static library
$(LIB): $(OBJS)
	${MAKE} -C $(URING) clean
	${MAKE} -C $(URING) library
	ar x $(URING)/src/liburing.a
	ar rcs $@ $^ *.ol
	rm *.o *.ol

# Compile source files into object files
%.o: %.c
	$(CC) ${INC} -c $< -o $@ $(CFLAGS)
 
# Clean up build files
clean:
	rm -f $(OBJS) ${LIB} *.o *.ol

.PHONY: ${LIB} all clean 