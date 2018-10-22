CC = gcc
CFLAGS = -std=gnu11

INTERPRETERS = basic-switch immediate-arg stack-machine register-machine regexp

LIBJIT_PATH=$$HOME/var/libjit
LIBJIT_INCLUDE_PATH=$(LIBJIT_PATH)/include
LIBJIT_LIB_PATH=$(LIBJIT_PATH)/jit/.libs
LIBJIT_AR=$(LIBJIT_LIB_PATH)/libjit.a

LDFLAGS=-ldl -lm -lpthread

all: $(INTERPRETERS) pigletvm

test: all
	$(foreach interpr,$(INTERPRETERS),./$(interpr);)

%: interpreter-%.c
	$(CC) $(CFLAGS) $< -o $@

pigletvm: pigletvm.c pigletvm-exec.c
	$(CC) $(CFLAGS) -I$(LIBJIT_INCLUDE_PATH) $(LIBJIT_AR) $^  $(LDFLAGS) -o $@

pigletvm-test: pigletvm.c pigletvm-test.c
	$(CC) -g $(CFLAGS) -I$(LIBJIT_INCLUDE_PATH) $(LIBJIT_AR) $^ $(LDFLAGS) -o $@
	./pigletvm-test

jittest: jittest.c
	$(CC) -g $(CFLAGS) -I$(LIBJIT_INCLUDE_PATH) $^  $(LIBJIT_AR) $(LDFLAGS) -o $@
	./jittest

clean:
	rm -vf $(INTERPRETERS) pigletvm pigletvm-test

.PHONY: all clean pigletvm-test
