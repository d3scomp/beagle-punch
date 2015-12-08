
NAME=visualizer_interface
SRCS=main.c ./cJSON_lib/cJSON.c ./serial_lib/uart.c
CFLAGS=-Wall -I ./cJSON_lib -I ./serial_lib -lm
CC=gcc

.PHONY=all

all: $(NAME)

$(NAME): $(SRCS)
	$(CC) -o $(NAME) $(CFLAGS) $(SRCS)

clean:
	rm -f $(NAME)

