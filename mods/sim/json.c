
#include "json.h"
#include "simulation.h"

#define snprintf_loc(buf, size, fmt, args...) scnprintf(buf, size, fmt,##args)

static int str_contains(const char * str, char c)
{
	while (*str != 0)
	{
		if (*str == c)
		{
			return 1;
		}

		++str;
	}

	return 0;
}

static const char * json_whitespace = "{} \t\r\n:,";

static const char * skip_whitespace(const char * json)
{
	while (str_contains(json_whitespace, *json))
	{
		++json;
	}

	return json;
}

const char * copy_token(const char *json, char * buf, size_t buf_size)
{
	size_t rem_length = buf_size;
	while (rem_length > 1 && *json != 0 && !str_contains(json_whitespace, *json))
	{
		*buf = *json;
		--rem_length;
		++json;
		++buf;
	}

	*buf = 0;

	while (*json != 0 && !str_contains(json_whitespace, *json))
	{
		++json;
	}

	return json;
}

const char * get_json_item(struct json_item * item, const char * json)
{
	json = skip_whitespace(json);
	json = copy_token(json, item->name, MAX_JSON_NAME);
	json = skip_whitespace(json);
	json = copy_token(json, item->value, MAX_JSON_VALUE);
	if (item->value[0] != 0)
	{
		return json;
	}

	return 0;
}

/************** command functions ******************/

int json_print_status(char * buf, size_t size, struct pp_t * pp_sim)
{
	const char * truestr = "true";
	const char * falsestr = "false";

	return snprintf_loc(
		buf,
		size,
		"{"
		"\"status\":{"
			"\"pos\":{\"x\":%d,\"y\":%d},\n"
			"\"vel\":{\"x\":%d,\"y\":%d},\n"
			"\"fail\":%s,\n"
			"\"punch\":{\"x\":%d,\"y\":%d},\n"
			"\"num\": %d"
		"},"
		"\"session\":{"
			"\"id\":\"%s\","
			"\"rnd\":%s,"
			"\"ip\":{\"x\":%d,\"y\":%d}"
		"}"
		"}",
		pp_sim->x_axis.head_pos, pp_sim->y_axis.head_pos,
		pp_sim->x_axis.velocity, pp_sim->y_axis.velocity,
		pp_sim->failed ? truestr : falsestr,
		pp_sim->last_punch.x_pos, pp_sim->last_punch.y_pos,
		pp_sim->punched_punches,

		pp_sim->session_id,
		(!pp_sim->use_init_pos) ? truestr : falsestr,
		pp_sim->x_init_pos, pp_sim->y_init_pos
	);
}

int json_apply_command(struct pp_t * pp_sim, const char * json_command)
{
	const char * reset_command = "\"reset\"";
	const char * position_command = "\"position\"";
	const char * velocity_command = "\"velocity\"";
	const char * power_command = "\"power\"";
	const char * initpos_command = "\"initpos\"";
	const char * random_initpos_command = "\"random_initpos\"";
	char command[MAX_JSON_VALUE];
	int x, y;
	struct json_item item;

	command[0] = 0;
	while ((json_command = get_json_item(&item, json_command)) != 0)
	{
		if (strcmp(item.name, "\"command\"") == 0)
		{
			strcpy(command, item.value);
		}
		else if (strcmp(item.name, "\"x\"") == 0)
		{
			if (kstrtoint(item.value, 10, &x))
			{
				x = 0;
			}
		}
		else if (strcmp(item.name, "\"y\"") == 0)
		{
			if (kstrtoint(item.value, 10, &y))
			{
				y = 0;
			}
		}
	}

	if (strcmp(command, reset_command) == 0)
	{
		pp_reset(pp_sim);
	}
	else if (strcmp(command, initpos_command) == 0)
	{
		pp_sim->x_init_pos = x;
		pp_sim->y_init_pos = y;
		pp_sim->use_init_pos = 1;
	}
	else if (strcmp(command, random_initpos_command) == 0)
	{
		pp_sim->use_init_pos = 0;
	}
	else if (strcmp(command, position_command) == 0)
	{
		// for this to be atomic we have to disable irqs or 
		pp_sim->x_axis.head_pos = x;
		pp_sim->y_axis.head_pos = y;
	}
	else if (strcmp(command, velocity_command) == 0)
	{
		// for this to be atomic we have to disable irqs or 
		pp_sim->x_axis.velocity = x;
		pp_sim->y_axis.velocity = y;
	}
	else if (strcmp(command, power_command) == 0)
	{
		// for this to be atomic we have to disable irqs or 
		pp_sim->x_axis.power = x;
		pp_sim->y_axis.power = y;
	} else {
		return -EINVAL;
	}

	return 0;
}

