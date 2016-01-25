
#ifndef JSON_H_
#define JSON_H_

#ifdef __KERNEL__
#include <linux/kernel.h>
#else
#include <stddef.h>
#include <string.h>
#endif

#define MAX_JSON_NAME	10
#define MAX_JSON_VALUE	20

struct json_item
{
	char name[MAX_JSON_NAME];
	char value[MAX_JSON_VALUE];
};

const char * copy_token(const char *json, char * buf, size_t buf_size);
const char * get_json_item(struct json_item * item, const char * json);

struct pp_t;

int json_print_status(char * buf, size_t size, struct pp_t * pp_sim);
int json_apply_command(struct pp_t * pp_sim, const char * json_command);

#endif

