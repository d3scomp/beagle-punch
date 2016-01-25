
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include "uart.h"

//#define dp(fmt, args...) printf("Func %s, %d: " fmt, __func__, __LINE__, ##args)
#define dp(fmt, args...)

#define LENGTH(array) (sizeof(array)/sizeof(*array))
#define MSG_START 0xA5

static const char * msgtype2string(enum msgtype_t c)
{
	switch (c)
	{
		case c_echo:
			return "c_echo";
		case c_echo_response:
			return "c_echo_response";
		case c_data:
			return "c_data";
		case c_response:
			return "c_response";
		case c_count:
			return "c_count";
	}

	return "UNKNOWN";
}

static uint8_t msg_checksum(const struct msg_t * msg)
{
	uint8_t checksum = msg->data_len + (msg->data_len >> 7) + msg->msgtype + msg->msgnumber;
	size_t i;

	for (i = 0; i < msg->data_len; ++i)
	{
		checksum += msg->data[i];
	}

	return checksum & 0x7f;
}

/* return value: -1 == error, -2 == timeout */
static int read_timeout(int fd, fd_set * set, uint8_t *result, size_t length, struct timeval * timeout)
{
	while (length > 0)
	{
		FD_SET(fd, set);
		int retval = select(fd + 1, set, NULL, NULL, timeout);
		if (retval == -1 || retval == 0)
		{
			return -1; // -1 == error, 0 == timeout
		}

		ssize_t r = read(fd, result, length);
		if (r <= 0)
		{
			return -1;
		}

		length -= r;
		result += r;
	}

	return 0;
}

static int send_buffer(int fd, const uint8_t * buf, size_t length)
{
	ssize_t written;
	while (length != 0)
	{
		written = write(fd, buf, length);
		if (written < 0)
		{
			return -1;
		}

		length -= written;
		buf += written;
	}

	return 0;
}

int uart_open(const char * fn)
{
	return open(fn, O_RDWR | O_NOCTTY | O_NONBLOCK);
}

void uart_init(int fd)
{
	struct termios setup;
	tcgetattr(fd, &setup);
	cfmakeraw(&setup);
	cfsetspeed(&setup, B115200);
	tcsetattr(fd, TCSAFLUSH, &setup);
}

int uart_receive(int fd, struct msg_t * msg, unsigned int timeout_ms)
{
	fd_set set;
	struct timeval timeout;
	int retval;
	uint8_t header[6];
	
	FD_ZERO(&set);

	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;

	while (!(retval = read_timeout(fd, &set, header, 1, &timeout)) && *header != MSG_START);
	if (retval || *header != MSG_START)
	{
		dp("Unable to read message header in time.\n");
		return -2;
	}
	if (!read_timeout(fd, &set, header + 1, LENGTH(header) - 1, &timeout))
	{
		uint8_t checksum = header[1] & 0x7f;
		msg->msgtype = header[4] & 0x7f;
		msg->msgnumber = header[5] & 0x7f;
		msg->data_len = ((header[2] & 0x7f) << 7) | (header[3] & 0x7f);
		if (msg->msgtype >= c_count || msg->data_len > UART_MAX_MSG_LENGTH)
		{
			dp("Wrong msgtype or too long data, msgtype: %s, data_len: %u\n", msgtype2string(msg->msgtype), (unsigned int)msg->data_len);
			return -1;
		}
		if (!read_timeout(fd, &set, msg->data, msg->data_len, &timeout) && msg_checksum(msg) == checksum)
		{
			return 0;
		}

		dp("Wrong checksum.\n");
	}

	return -1;
}

int uart_send(int fd, const struct msg_t * msg)
{
	uint8_t checksum = msg_checksum(msg);
	uint8_t header[6];
	header[0] = MSG_START;
	header[1] = checksum;
	header[2] = (uint8_t)((msg->data_len >> 7) & 0x7f);
	header[3] = (uint8_t)(msg->data_len & 0x7f);
	header[4] = (uint8_t)msg->msgtype;
	header[5] = (uint8_t)msg->msgnumber;

	if (msg->data_len > UART_MAX_MSG_LENGTH)
	{
		return -1; // data too long
	}

	if (send_buffer(fd, header, LENGTH(header)))
	{
		return -1;
	}
	if (msg->data_len > 0)
	{
		return send_buffer(fd, msg->data, msg->data_len);
	}

	return 0;
}

int uart_send_and_receive(int fd, struct msg_t * msg, unsigned int timeout_ms)
{
	int retval;
	if (uart_send(fd, msg))
	{
		dp("Error sending message.\n");
		return -1;
	}
	
	retval = uart_receive(fd, msg, timeout_ms);
	if (retval == -1)
	{
		dp("Error receiving a message.\n");
	}
	if (retval == -2)
	{
		dp("Timeout receiving a message.\n");
	}

	return retval;
}

