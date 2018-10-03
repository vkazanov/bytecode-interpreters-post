CC = gcc
CFLAGS = -std=gnu11

INTERPRETERS = basic-switch immediate-arg stack-machine register-machine regexp

all: $(INTERPRETERS)

%: interpreter-%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -vf $(INTERPRETERS)

.PHONY: all clean
