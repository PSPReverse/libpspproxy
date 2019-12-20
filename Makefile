CC           = gcc
AR           = ar
RANLIB       = ranlib
CFLAGS       = -std=c1x -Iinclude -O2 -fPIC -pedantic -Wall -Wextra -Werror -march=native -fms-extensions
LDFLAGS      = -shared

SO_TARGET  = libpspproxy.so
A_TARGET   = libpspproxy.a
OBJS = psp-proxy.o

all: $(SO_TARGET) $(A_TARGET)

clean:
	rm -f $(OBJS) $(SO_TARGET) $(A_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(SO_TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(SO_TARGET) $(OBJS)

$(A_TARGET): $(OBJS)
	$(AR) r $@ $^
	$(RANLIB) $(A_TARGET)
