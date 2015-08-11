SHELL := /bin/bash
CC := gcc
CFLAGS := -g -O3 -std=gnu11

all: build/mztx06a

build/mztx06a: build/mztx06a.o build/bcm2835.o
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(CFLAGS) $^

build/lcdinit: build/lcdinit.o build/bcm2835.o
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(CFLAGS) $^

build/lcdpi: build/lcdpi.o build/bcm2835.o
	@mkdir -p $(dir $@)
	gcc -o $@ $(CFLAGS) $^

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $(CFLAGS) $^
