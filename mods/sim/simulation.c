#include <linux/kernel.h>
#include <linux/string.h> // memset

#include <asm/div64.h>

#include "simulation.h"
#include "xorshift.h"
#include "json.h"

static int sign(s64 x)
{
	return x < 0 ? -1 : 1;
}

static inline s32 abs32(s32 x)
{
	return x < 0 ? -x : x;
}

#define signed_do_div(dividend, divisor) \
({ \
	int s1 = sign(dividend); \
	int s2 = sign(divisor); \
	u64 udividend = abs64(dividend); \
	u32 udivisor = abs(divisor); \
	do_div(udividend, udivisor); \
	dividend = (s64)udividend * s1 * s2; \
})


#define ERS_TABLE_SAFE_ZONE 20000000 //nm /* this is zone around the punching area, exiting this zone with the center of the punching head leads to failure */
#define ERS_TABLE_FAIL_ZONE 20000000 //nm /* this is the area around the punching area which is still displayed */

#define ERS_TABLE_PUNCH_AREA_WIDTH 1500000000 /* nm */
#define ERS_TABLE_PUNCH_AREA_HEIGHT 1000000000 /* nm */

#define ERS_TABLE_FULL_WIDTH (ERS_TABLE_PUNCH_AREA_WIDTH + 2 * (ERS_TABLE_SAFE_ZONE + ERS_TABLE_FAIL_ZONE)) /* nm */
#define ERS_TABLE_FULL_HEIGHT (ERS_TABLE_PUNCH_AREA_HEIGHT + 2 * (ERS_TABLE_SAFE_ZONE + ERS_TABLE_FAIL_ZONE)) /* nm */

#define ERS_HEAD_MASS_G 2000
#define ERS_FRICTION_KOEF 0.1


#define ERS_PUNCH_DURATION_MS 100
#define ERS_PUNCH_MAX_VEL_UM_S	100

// The lenght in mm after which the four combinations of the encoder again repeat
#define ERS_QENC_PERIOD_NM 1000000 // in nanometers

static inline void set_encoder(struct pp_axis_t * axis)
{
	unsigned int enc;
	enc = (unsigned int)(axis->head_pos / (ERS_QENC_PERIOD_NM / 4)) % 4;

	if (enc == 2)
		enc = 3;
	else if (enc == 3)
		enc = 2;

	axis->encoder = enc;
}

static inline u8 get_state(struct pp_axis_t * axis, int vertical, s32 max_axis_value)
{
	u8 result = axis->encoder;

	s32 head_pos = axis->head_pos;

	if (head_pos < 0)
		result |= US_SAFE_L << (vertical ? 2 : 0);
	if (head_pos > max_axis_value)
		result |= US_SAFE_R << (vertical ? 2 : 0);
	if (head_pos < -ERS_TABLE_SAFE_ZONE || head_pos > max_axis_value + ERS_TABLE_SAFE_ZONE)
		result |= US_FAIL;
	
	return result;
}

static inline update_state_t update_axis(
	struct pp_axis_t * axis,
	u32 us_period,
	int vertical,
	s32 max_axis_value)
{
	s32 head_pos;
	s64 head_pos_change;

	s32 friction = ERS_FRICTION_KOEF * ERS_HEAD_MASS_G;
	s32 vel_decreased_by_friction = (friction * us_period) / 1000;

	s32 vel = axis->velocity;
	s64 vel_change;
	s64 motor_force = (axis->power * 2500 - vel) * 5; // division by 80 is ok, 400 / 80 = 5, 1000000 / 80 = 12500
	s64 random_force_factor = motor_force * (xorshift_rand() % 1000);

	signed_do_div(random_force_factor, 100000); // divide random_force_factor by 100000 -> (motor_force * (xorshift_rand() % 1000)) / 100000
	motor_force += random_force_factor;

	vel_change = motor_force * us_period;
	signed_do_div(vel_change, 1000000); // divide vel_change by 1000000 -> (s32)((motor_force * us_period) / 1000000)
	vel += (s32)vel_change;


	if (abs32(vel) <= 1) {
		vel = 0;
	} else {
		s32 vel_decr = vel_decreased_by_friction;
		if (abs32(vel) <= vel_decr) {
			vel = 0;
		} else {
			if (vel < 0) {
				vel += vel_decr;
			} else {
				vel -= vel_decr;
			}
		}
	}

	axis->velocity = vel;
	
	head_pos_change = (s64)vel * (s64)us_period;
	signed_do_div(head_pos_change, 1000); // divide head_pos_change by 1000 -> (s32)((s64)vel * (s64)us_period / 1000)
	axis->head_pos += (s32)head_pos_change;
	head_pos = axis->head_pos;

	return get_state(axis, vertical, max_axis_value);
}

update_state_t pp_update(struct pp_t * pp, u32 us_period) {
	update_state_t retval;

	if (!pp->failed) {
		retval = update_axis(&pp->x_axis, us_period, 0, ERS_TABLE_PUNCH_AREA_WIDTH);
		retval |= update_axis(&pp->y_axis, us_period, 1, ERS_TABLE_PUNCH_AREA_HEIGHT);
	
		if (pp->punch) {
			if (pp->remaining_punch_time > 0 || abs32(pp->x_axis.velocity) > ERS_PUNCH_MAX_VEL_UM_S || abs32(pp->y_axis.velocity) > ERS_PUNCH_MAX_VEL_UM_S) {
				pp->failed = 1;
				retval |= US_FAIL;
				return retval;
			}

			pp->last_punch.x_pos = pp->x_axis.head_pos;
			pp->last_punch.y_pos = pp->y_axis.head_pos;
			pp->remaining_punch_time = ERS_PUNCH_DURATION_MS * 1000;

			mb(); // decrease the number of punches only after the punch has started

			pp->punch = 0;

		} else {
			if (pp->remaining_punch_time > 0) {
				pp->remaining_punch_time -= us_period;

				if (abs32(pp->x_axis.velocity) > ERS_PUNCH_MAX_VEL_UM_S || abs32(pp->y_axis.velocity) > ERS_PUNCH_MAX_VEL_UM_S) {
					pp->failed = 1;
					retval |= US_FAIL;
					return retval;
				}

				if (pp->remaining_punch_time <= 0)
				{
					retval |= US_HEAD_UP;
					pp->remaining_punch_time = 0;
					++pp->punched_punches;
				}
			} else {
				retval |= US_HEAD_UP;
			}
		}

	} else {
		retval = get_state(&pp->x_axis, 0, ERS_TABLE_PUNCH_AREA_WIDTH);
		retval |= get_state(&pp->y_axis, 1, ERS_TABLE_PUNCH_AREA_HEIGHT);
		retval |= US_FAIL;
	}

	return retval;
}

static char pp_get_random_char(void)
{
	u32 r = xorshift_rand();
	switch ((r >> 7) & 0x3)
	{
		case 1:
			return (char)('0' + r % ('9' - '0' + 1));
		case 2:
			return (char)('a' + r % ('z' - 'a' + 1));
	}

	return (char)('A' + r % ('Z' - 'A' + 1));
}

static void pp_init_common(struct pp_t * pp)
{
	int i;

	set_encoder(&pp->x_axis);
	set_encoder(&pp->y_axis);

	for (i = 0; i < SESSION_ID_LENGTH; ++i)
	{
		pp->session_id[i] = pp_get_random_char();
	}

	pp->session_id[SESSION_ID_LENGTH] = '\0';
}

void pp_reinit(struct pp_t * pp)
{
	s32 x_init_pos = pp->x_init_pos;
	s32 y_init_pos = pp->y_init_pos;
	int use_init_pos = pp->use_init_pos;

	memset(pp, 0, sizeof(*pp));

	pp->x_init_pos = x_init_pos;
	pp->y_init_pos = y_init_pos;
	pp->use_init_pos = use_init_pos;

	if (use_init_pos)
	{
		pp->x_axis.head_pos = pp->x_init_pos;
		pp->y_axis.head_pos = pp->y_init_pos;
	}
	else
	{
		pp->x_axis.head_pos = xorshift_rand() % ERS_TABLE_PUNCH_AREA_WIDTH;
		pp->y_axis.head_pos = xorshift_rand() % ERS_TABLE_PUNCH_AREA_HEIGHT;
	}

	pp_init_common(pp);
}

void pp_init(struct pp_t * pp)
{
	xorshift_srand();
	pp_reinit(pp);
}

void pp_reset(struct pp_t * pp)
{
	memset(pp, 0, sizeof(*pp));
	pp_init_common(pp);
}

