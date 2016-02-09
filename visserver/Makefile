.PHONY=all clean

SRCS=main.c ./cJSON_lib/cJSON.c ./serial_lib/uart.c
CFLAGS=-Wall -I ./cJSON_lib -I ./serial_lib -lm
CC=gcc


all: visserver

visserver: $(SRCS)
	gcc -o visserver $(CFLAGS) $(SRCS)

clean:
	rm -f visserver

