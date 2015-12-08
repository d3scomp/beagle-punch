
#include "simulation.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_SIZE 1024
#define STATUS_SIZE 1024

static int cmd_cmp(const char * input, const char * cmd)
{
	int len = strlen(cmd);
	return strncmp(input, cmd, len) == 0;
}

static void print_stat(struct pp_t * pp_sim)
{
	char status[STATUS_SIZE];
	json_print_status(status, STATUS_SIZE, pp_sim);
	printf("%s\n", status);
}

#define UPDATE_FREQUENCY	11111
#define UPDATE_PERIOD_US	(1000000 / UPDATE_FREQUENCY)

int main(int argc, char ** argv)
{
	char input[INPUT_SIZE];
	struct pp_t pp_sim;

	pp_init(&pp_sim);

	while (1)
	{
		printf("Enter command (user/status/exit/update <count>/pwrx <power>/punch): ");
		fgets(input, INPUT_SIZE, stdin);

		if (cmd_cmp(input, "status"))
		{
			print_stat(&pp_sim);
		}
		else if (cmd_cmp(input, "user"))
		{
			printf("Enter json: ");
			fgets(input, STATUS_SIZE, stdin);
			json_apply_command(&pp_sim, input);
		}
		else if (cmd_cmp(input, "exit"))
		{
			return 0;
		}
		else if (cmd_cmp(input, "update"))
		{
			char *it = input;
			while (*it != 0 && *it != ' ') { ++it; }
			if (*it != 0)
			{
				int count = strtol(it, NULL, 10);
				while (count-- > 0) { pp_update(&pp_sim, UPDATE_PERIOD_US); }
				print_stat(&pp_sim);
			}
		}
		else if (cmd_cmp(input, "reset"))
		{
			pp_reset(&pp_sim);
		}
		else if (cmd_cmp(input, "pwrx"))
		{
			char *it = input;
			while (*it != 0 && *it != ' ') { ++it; }
			if (*it != 0)
			{
				int pwr = strtol(it, NULL, 10);
				pp_sim.x_axis.power = pwr;
				print_stat(&pp_sim);
			}
			
		}
		else if (cmd_cmp(input, "punch"))
		{
			pp_punch(&pp_sim);
		}
		else if (cmd_cmp(input, "\n"))
		{
			pp_update(&pp_sim, UPDATE_PERIOD_US);
			print_stat(&pp_sim);
		}
		else
		{
			printf("Bad command.\n");
		}

		printf("\n");
	}
	
	return 0;
}
