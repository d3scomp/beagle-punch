
#ifndef SIMULATION_H_
#define SIMULATION_H_

#include "types.h"

struct pp_punch_t
{
	s32 x_pos; // x position of the punch, nm from left size of the
	s32 y_pos;
};

typedef enum {
	PP_NONE = 0,
	PP_START_ZONE = 1,
	PP_END_ZONE = 2,
	PP_FAIL = 4
} pp_error_state_t;

struct pp_axis_t
{
	s8 power : 8; // -128 .. 127 speed
	u8 encoder : 2; // quadratic encoder
	s32 velocity; // head velocity on the axis um/s
	s32 head_pos; // head position on the axis (nanometers)
	pp_error_state_t errors;
};

#define SESSION_ID_LENGTH	10

struct pp_t
{
	char session_id[SESSION_ID_LENGTH + 1];

	int use_init_pos;
	s32 x_init_pos;
	s32 y_init_pos;

	struct pp_axis_t x_axis;
	struct pp_axis_t y_axis;

	u8 irq_enabled : 1; // 1 means that interrupts are delivered
	pp_error_state_t errors;
	
	atomic_t punches; // punch requests received from the previous update
	s32 remaining_punch_time; // time remaining to complete the current punch (ns)
	struct pp_punch_t last_punch;
	u32 punched_punches; // number of punched punches
};

typedef enum {
	US_NONE = 0,
	US_PUNCH_START = 1,
	US_PUNCH_END = 2,
	US_ENC_CHANGE = 4,
	US_ERR_CHANGE = 8
} update_state_t;

void pp_init(struct pp_t * pp);
void pp_reinit(struct pp_t * pp);
void pp_reset(struct pp_t * pp);
update_state_t pp_update(struct pp_t * pp, u32 us_period);

static inline void pp_punch(struct pp_t * pp)
{
	atomic_add(1, &pp->punches);
}

static inline int pp_punch_in_progress(struct pp_t * pp)
{
	return atomic_read(&pp->punches) || pp->remaining_punch_time > 0;
}

static inline int pp_fail(struct pp_t * pp)
{
	return (pp->errors & PP_FAIL) || (pp->x_axis.errors & PP_FAIL) || (pp->y_axis.errors & PP_FAIL);
}

#endif

