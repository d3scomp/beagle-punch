
.origin 0
.entrypoint start

#define C_PRUCFG	c4

#define SPI_ADDR	0x48030000 // 0x48030000 = McSPI0, 0x481A0000 = McSPI1
#define SPI_STAT_OFFSET	0x130
#define SPI_TX_OFFSET	0x138
#define SPI_RX_OFFSET	0x13c
#define SPI_CH0CTRL_OFFSET	0x134

#define LAST_VALID_CMD	6

#define SLAVE_SELECT_REG	r31.t14 // slave select GPI

#define PUNCH_IRQ	0x20 // value to be written to r31 when punch request is received

// ****************************************************************************
// variables
#define SIM_STATE_VAR		0x0 // first byte contains renspose to the encoders command, second byte response to the errors command; the simulator writes this value
#define PWRX_VAR		0x4
#define PWRY_VAR		0x5
#define IRQ_EN_VAR		0x6
#define PUNCH_VAR		0x7

#define BRANCH_TABLE_VAR	0x100

// ****************************************************************************
// macro definitions

// waits in loop for the spi rx to be full
.macro wait_spi
.mparam reg0, reg1
	mov	reg0, SPI_ADDR + SPI_STAT_OFFSET
lloop:
	qbbs	wait_for_ss, SLAVE_SELECT_REG // if we are deselected, go waiting until we get selected again

	lbbo	reg1, reg0, 0, 4 // load spi stat register to r1
	qbbc	lloop, reg1.t0 // if bit RXS == 0, nothing is received -> loop again
.endm

.macro read_spi
.mparam reg
	mov	reg, SPI_ADDR + SPI_RX_OFFSET
	lbbo	reg, reg, 0, 4
.endm

.macro write_spi
.mparam reg, addrreg
	mov	addrreg, SPI_ADDR + SPI_TX_OFFSET
	sbbo	reg, addrreg, 0, 4
.endm

.macro write_spi_zero
.mparam reg, addrreg
	mov	reg, 0
	write_spi	reg, addrreg
.endm

.macro load_sim_state
.mparam reg
	mov	reg, SIM_STATE_VAR
	lbbo	reg, reg, 0, 4
.endm

.macro store_sim_act
.mparam reg, addrreg, addr
	mov	addrreg, addr
	sbbo	reg.b0, addrreg, 0, 1
.endm

// ****************************************************************************

start:
	// prepare branch table
	ldi	r0, cmd_none_l
	ldi	r1, cmd_encoders_l
	ldi	r2, cmd_errors_l
	ldi	r3, cmd_punch_l
	ldi	r4, cmd_pwrx_l
	ldi	r5, cmd_pwry_l
	ldi	r6, cmd_irq_en_l
	mov	r7, BRANCH_TABLE_VAR
	sbbo	&r0, r7, 0, 4 * 7

	// enable OCP master port
	lbco	r0, C_PRUCFG, 4, 4
	clr	r0.t4 // clear SYSCFG[STANDBY_INIT] to enable OCP master port
	sbco	r0, C_PRUCFG, 4, 4

// ****************************************************************************

.macro wait
	MOV r7, 0x00f00000
del2:
	SUB r7, r7, 1
	QBNE del2, r7, 0
.endm

.macro blink
	set	r30.t15

	MOV r7, 0x00f00000
del1:
	SUB r7, r7, 1
	QBNE del1, r7, 0

	clr	r30.t15
.endm

// blinking on PRU gpo 15
//	mov	r1, 1
//loop1:
//	xor	r1, r1, 1
//	qbeq	set_pin, r1, 1
//
//reset_pin:
//	clr	r30.t15
//	jmp	end_pin
//set_pin:
//	set	r30.t15
//
//end_pin:
//	MOV r0, 0x00f00000
//DEL1:
//	SUB r0, r0, 1
//	QBNE DEL1, r0, 0
//
//	jmp	loop1

// ****************************************************************************

	mov	r1, 0x8
	mov	r0, 255
	sbbo	r0, r1, 0, 4

// wait until we are selected
wait_for_ss:
	//set	r30.t15
	// disable spi module
	mov	r0, 0
	mov	r1, SPI_ADDR + SPI_CH0CTRL_OFFSET
	sbbo	r0, r1, 0, 4
wait_for_ss_inner:
	qbbs	wait_for_ss_inner, SLAVE_SELECT_REG // when slave select is high, cycle again

// write spi configuration, this should reset inner spi module bit sending/receiving counter according to one ti forum post
	mov	r1, SPI_ADDR + SPI_CH0CTRL_OFFSET
	lbbo	r0, r1, 0, 4
	sbbo	r0, r1, 0, 4

// enable spi module
	mov	r0, 1
	mov	r1, SPI_ADDR + SPI_CH0CTRL_OFFSET
	sbbo	r0, r1, 0, 4

// write tx, so that we do not get underflow
	write_spi_zero	r0, r1
	//clr	r30.t15

// ****************************************************************************

	clr	r30.t15
	clr	r20.t15
loop0:

	// wait for value to be received through spi
	wait_spi	r0, r1
	//blink
	//wait

	// process received command
	read_spi	r0

	//mov	r3, SPI_ADDR + SPI_STAT_OFFSET
	//lbbo	r3, r3, 0, 4 // load spi stat register to r3
	//blink
	//wait
	//qbbc	normal, r3.t0
	//blink
	//wait
	//wait
	//wait

//normal:
	and	r0, r0.b0, 0x7f // we send commands with the MSB set to 1 from the controller IRQ handler
	qblt	bad_cmd, r0.b0, LAST_VALID_CMD // check validity of the received command

	// command is valid, branch to the correct processing method
	mov	r1, BRANCH_TABLE_VAR
	lsl	r0, r0.b0, 2
	add	r1, r1, r0.b0
	lbbo	r1, r1, 0, 4
	jmp	r1

cmd_none_l: // do nothing, write arbitrary response
	write_spi_zero	r0, r1
	jmp	loop_end

cmd_encoders_l:
	load_sim_state r0
	write_spi	r0.b0, r1
	jmp	loop_end

cmd_errors_l:
	load_sim_state r0
	// if punch start is set, make sure that we return that punch is in progress
	mov	r1, PUNCH_VAR
	lbbo	r1.b0, r1, 0, 1 // load punch request status
	qbbc	errors_write, r1.t0 // skip following instruction, when punch request is not set
	or	r0.b1, r0.b1, 1
errors_write:
	write_spi	r0.b1, r1
	jmp	loop_end

cmd_punch_l:
	// write empty response
	write_spi_zero	r0, r1
	// set punch interrupt
	//ldi	r31, PUNCH_IRQ
	// set actuators
	mov	r0, 1
	store_sim_act	r0, r1, PUNCH_VAR
	jmp	loop_end

cmd_pwrx_l:
	write_spi_zero	r0, r1
	wait_spi	r0, r1
	read_spi	r0
	write_spi_zero	r1, r2
	// set actuators
	store_sim_act	r0, r1, PWRX_VAR
	jmp	loop_end

cmd_pwry_l:
	write_spi_zero	r0, r1
	wait_spi	r0, r1
	read_spi	r0
	write_spi_zero	r1, r2
	// set actuators
	store_sim_act	r0, r1, PWRY_VAR
	jmp	loop_end

cmd_irq_en_l:
	write_spi_zero	r0, r1
	wait_spi	r0, r1
	read_spi	r0
	write_spi_zero	r1, r2
	// set actuators
	store_sim_act	r0, r1, IRQ_EN_VAR
	jmp	loop_end

bad_cmd:
	write_spi_zero	r1, r2
	qbbs	loop_end, r20.t15
	// write the bad value into address 0x10
	mov	r3, 0x10
	sbbo	r0, r3, 0, 1
	//light a LED
	set	r30.t15
	set	r20.t15
	jmp	loop_end

loop_end:
	jmp	loop0

	halt

