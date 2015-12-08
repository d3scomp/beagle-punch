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


// These are supposed to be the same as defined in the simulator module. They are used only when DEBUG_SPI is 1
#define IOCTL_SPI_LOG   1
#define SPI_LOG_BUF_SIZE        1024 * 1024 * 2


#define KMODULE_NODE	"/dev/ppsim0"
#define UART_FILENAME	"/dev/ttyO4" // USART4 on BBB
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
				//printf("sending status: %s\n", (char *)response.data);
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

static int normal_mode(int dev_fd, const char * serial_fn)
{
	int serial_fd;
	struct msg_t msg;

	printf("Starting in normal (daemon) mode.\n");
	serial_fd = uart_open(serial_fn);
	if (serial_fd == -1)
	{
		printf("Unable to open serial port file: %s\n", serial_fn);
		return -1;
	}

	uart_init(serial_fd);

	printf("Init probably successful.\n");
	while (true)
	{
		if (uart_receive(serial_fd, &msg, 1000))
		{
			continue;
		}

		//msg.data[msg.data_len] = 0;
		//printf("Message received: %s.\n", (char *)msg.data);
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

#ifdef DEBUG_SPI
static void print_spi_buf(int dev_fd)
{
	long long int i;
	char * buffer = malloc(sizeof(char) * SPI_LOG_BUF_SIZE);
	long long int retval = ioctl(dev_fd, IOCTL_SPI_LOG, buffer);
	if (retval < 0)
	{
		free(buffer);
		printf("Error reading spi_buf\n");
		return;
	}

	printf("retval: %lli, bufsize: %i\n", retval, SPI_LOG_BUF_SIZE);
	if (retval > SPI_LOG_BUF_SIZE)
	{
		retval = SPI_LOG_BUF_SIZE;
	}

	for (i = 0; i < retval; i += 5)
	{
		printf("%c: %x,x:%d,y:%d,f:%d\n", buffer[i], (unsigned int)buffer[i + 1],
		(int)buffer[i + 2], (int)buffer[i + 3], (int)buffer[i + 4]);
	}

	free(buffer);
}
#endif

static int starts_with(const char * str, const char * prefix)
{
	size_t str_len = strlen(str);
	size_t prefix_len = strlen(prefix);
	return prefix_len > str_len ? 0 : strncmp(str, prefix, prefix_len) == 0;
}

static int interactive_mode(int dev_fd)
{
	#define INTERACTIVE_BUF_SIZE	500
	char input[INTERACTIVE_BUF_SIZE];
	char buf[BUF_SIZE];
	char * retval;

	printf("Starting in interactive mode.\n");
	while (true)
	{
		printf("Enter command (exit/status/reset/user): ");
		retval = fgets(input, INTERACTIVE_BUF_SIZE, stdin);
		if (retval == NULL)
		{
			return 0;
		}

		if (starts_with(input, "exit"))
		{
			return 0;
		}
		else if (starts_with(input, "status"))
		{
			read_status(dev_fd, buf, BUF_SIZE);
			printf("%s\n", buf);
		}
		else if (starts_with(input, "reset"))
		{
			write_reset(dev_fd);
		}
		else if (starts_with(input, "user"))
		{
			printf("Enter user command in json: ");
			fgets(input, INTERACTIVE_BUF_SIZE, stdin);
			write_command(dev_fd, input);
		}
#ifdef DEBUG_SPI
		else if (starts_with(input, "read_spi"))
		{
			print_spi_buf(dev_fd);
		}
#endif
		else
		{
			printf("Unknown command.\n");
		}
	}

	return 0;
}

// usage: bbb_user [-i] [-d <device node name>] [-s <serial port name>]
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
			switch (arg[1])
			{
				case 'i':
					interactive = true;
					break;
				default:
					prev_arg = arg[1];
			}
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

	if (interactive)
	{
		retval = interactive_mode(fd);
	}
	else
	{
		retval = normal_mode(fd, serial_fn);
	}

	close(fd);

	return retval;
}

