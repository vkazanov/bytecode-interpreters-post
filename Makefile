CC = gcc
CFLAGS = -std=gnu11

INTERPRETERS = basic-switch immediate-arg stack-machine register-machine regexp

all: $(INTERPRETERS) pigletvm

test: all
	$(foreach interpr,$(INTERPRETERS),./$(interpr);)

%: interpreter-%.c
	$(CC) $(CFLAGS) $< -o $@

pigletvm: pigletvm.c pigletvm-exec.c
	$(CC) $(CFLAGS) $^ -o $@

pigletvm-test: pigletvm.c pigletvm-test.c
	$(CC) -g $(CFLAGS) $^ -o $@
	./pigletvm-test

clean:
	rm -vf $(INTERPRETERS) pigletvm pigletvm-test

.PHONY: all clean pigletvm-test
