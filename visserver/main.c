#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <uart.h>
#include <cJSON.h>


#define KMODULE_NODE	"/dev/ppsim0"
#define UART_FILENAME	"/dev/ttyGSO"
#define BUF_SIZE	1024

static ssize_t read_status(int fd, char * buf, size_t size)
{
	ssize_t retval = read(fd, buf, size - 1);
	if (retval >= 0)
	{
		buf[retval] = 0;
	}
	else
	{
		buf[0] = 0;
	}

	return retval;
}

static int write_command(int fd, const char * command)
{
	size_t remaining = strlen(command);
	while (remaining)
	{
		int retval = write(fd, command, remaining);
		if (retval < 0)
		{
			printf("Command write unsuccessful.\n");
			return retval;
		}

		remaining -= retval;
		command += retval;
	}

	return 0;
}

static int write_reset(int fd)
{
	const char * resetval = "{ \"command\": \"reset\" }";
	return write_command(fd, resetval);
}

#define STATUS_BUF_SIZE	1024

/*
Copies a JSON object from one string to another.
A JSON object is always surrounded by parentheses, which we use to detect the boundaries of the object.
The src pointer has to point to the left parenthesis surrounding the object.
*/
static char * copy_json_object(char * dest, const char * src, size_t size)
{
	if (*src == '{' && size > 1)
	{
		size_t parenthesis_count = 0;
		int in_string = 0;
		do
		{
			char c = *src;
			*dest = c;

			if (in_string)
			{
				if (c == '\"')
				{
					in_string = 0;
				}
			}
			else
			{
				switch (c)
				{
					case '{':
						++parenthesis_count;
						break;
					case '}':
						--parenthesis_count;
						break;
					case '\"':
						in_string = 1;
						break;
				}
			}

			++dest;
			++src;
			--size;
		}
		while (*src != 0 && size > 1 && parenthesis_count > 0);
	}

	if (size > 0)
	{
		*dest = 0;
	}

	return dest;
}

static void process_json_msg(int dev_fd, int serial_fd, struct msg_t * msg)
{
	const char * status_name = "\"status\":";
	const char * session_name = "\"session\":";
	cJSON * root_node, * command_node;

	if (msg->data_len >= UART_MAX_MSG_LENGTH)
	{
		return;
	}

	msg->data[msg->data_len] = 0;
	root_node = cJSON_Parse((char *)msg->data);
	if (root_node != NULL)
	{
		command_node = cJSON_GetObjectItem(root_node, "command");

		if (command_node != NULL && command_node->type == cJSON_String && (strcmp(command_node->valuestring, "status") == 0 || strcmp(command_node->valuestring, "session") == 0))
		{
			struct msg_t response;
			char buffer[STATUS_BUF_SIZE];
			ssize_t retval = read_status(dev_fd, buffer, STATUS_BUF_SIZE);
			if (retval >= 0)
			{
				const char * obj_name;
				char * node;
				response.msgtype = c_response;
				response.msgnumber = msg->msgnumber;
				
				obj_name = (strcmp(command_node->valuestring, "status") == 0) ? status_name : session_name;
				node = strstr(buffer, obj_name);
				if (node != NULL)
				{
					node += strlen(obj_name);
					copy_json_object((char *)response.data, node, UART_MAX_MSG_LENGTH);
					response.data_len = strlen((char *)response.data);
					// printf("%s\n\n%s\n\n", (char *)response.data, buffer);

					uart_send(serial_fd, &response);
				}
			}
		}
		else
		{
			write_command(dev_fd, (char *)msg->data);
		}

		cJSON_Delete(root_node);
	}
}

static int run(int dev_fd, const char * serial_fn)
{
	int serial_fd;
	struct msg_t msg;

	serial_fd = uart_open(serial_fn);
	if (serial_fd == -1)
	{
		printf("Unable to open serial port file: %s\n", serial_fn);
		return -1;
	}

	uart_init(serial_fd);

	while (true)
	{
		if (uart_receive(serial_fd, &msg, 1000))
		{
			continue;
		}

		switch (msg.msgtype)
		{
			case c_echo:
				msg.msgtype = c_echo_response;
				uart_send(serial_fd, &msg); // answer the same message to echo request
				break;
			case c_data: // message of type json
				process_json_msg(dev_fd, serial_fd, &msg);
				break;
			default: // just to shut up the compiler
				break;
		}
	}

	close(serial_fd);
	return 0;
}

// usage: bbb_user [-d <device node name>] [-s <serial port name>]
int main(int argc, char ** argv)
{
	bool interactive = false;
	int fd;
	int retval;
	const char * dev_fn = KMODULE_NODE;
	const char * serial_fn = UART_FILENAME;
	char prev_arg = 0;
	
	// parse arguments
	++argv;
	while (*argv != NULL)
	{
		char * arg = *argv;
		if (arg[0] == '-')
		{
			prev_arg = arg[1];
		}
		else
		{
			if (prev_arg == 'd')
				dev_fn = arg;
			else if (prev_arg == 's')
				serial_fn = arg;
			prev_arg = 0;
		}

		++argv;
	}
	
	fd = open(dev_fn, O_RDWR);
	if (fd == -1)
	{
		printf("Unable to open simulator device file: %s\n", dev_fn);
		return -1;
	}

	retval = run(fd, serial_fn);

	close(fd);

	return retval;
}

