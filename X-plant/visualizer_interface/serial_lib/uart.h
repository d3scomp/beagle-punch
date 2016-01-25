
#ifndef UART_H_
#define UART_H_

#include <stdint.h>

#define UART_MAX_MSG_LENGTH ((1 << 14) - 1) // maximal length of a sended/received msgtype

enum msgtype_t
{
	c_echo = 1,
	c_echo_response,
	c_data,
	c_response,
	c_count, // number of msgtypes, not a real msgtype, add new msgtypes before this one
};

struct msg_t
{
	enum msgtype_t msgtype; 
	uint8_t msgnumber;
	uint8_t data[UART_MAX_MSG_LENGTH];
	size_t data_len;
};

int uart_open(const char * fn);
void uart_init(int fd);
int uart_receive(int fd, struct msg_t * msg, unsigned int timeout_ms);
int uart_send(int fd, const struct msg_t * msg);
int uart_send_and_receive(int fd, struct msg_t * msg, unsigned int timeout_ms);

#endif
