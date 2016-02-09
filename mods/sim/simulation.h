
#ifndef SIMULATION_H_
#define SIMULATION_H_

#include "types.h"

struct pp_punch_t
{
	s32 x_pos; // x position of the punch, nm from left size of the
	s32 y_pos;
};

struct pp_axis_t
{
	s8 power; // -128 .. 127 speed
	u8 encoder; // quadratic encoder
	s32 velocity; // head velocity on the axis um/s
	s32 head_pos; // head position on the axis (nanometers)
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

	u8 irq_enabled; // 1 means that interrupts are delivered
	u8 failed; // 1 means that we are in FAIL state
	u8 punch; // 1 means that punch was requested	

	s32 remaining_punch_time; // time remaining to complete the current punch (ns)
	struct pp_punch_t last_punch;
	u32 punched_punches; // number of punched punches
};

#define US_NONE		0
#define US_ENC_X0	(1 << 0)
#define US_ENC_X1	(1 << 1)
#define US_ENC_Y0	(1 << 2)
#define US_ENC_Y1	(1 << 3)
#define US_SAFE_L	(1 << 4)
#define US_SAFE_R	(1 << 5)
#define US_SAFE_T	(1 << 6)
#define US_SAFE_B	(1 << 7)
#define US_HEAD_UP	(1 << 8)
#define US_FAIL		(1 << 9)

void pp_init(struct pp_t * pp);
void pp_reinit(struct pp_t * pp);
void pp_reset(struct pp_t * pp);
u32 pp_update(struct pp_t * pp, u32 us_period);

#endif

