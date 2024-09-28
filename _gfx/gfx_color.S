// *****************************
// Mem copy/blit functions
// *****************************
	.syntax unified
	.section .time_critical, "ax"
	.cpu cortex-m0plus
	.thumb

// Registers:
//  R0 ... pointer to destination data buffer (32bits aligned)
//  R1 ... color pattern 4-pixels
//  R2 ... width of this segment as multiply of 4 pixels (=width in pixels/4)

.thumb_func
.global RenderColor
RenderColor:

	// push registers
	push	{r4,r5,r6,r7,lr}

	// duplicate color pattern
	mov	r3,r1
	mov	r4,r1
	mov	r5,r1
	mov	r6,r1
	mov	r7,r1

	// go to big transfer
	b	3f

// ---- [38 per loop] big transfer 120 pixels, speed 0.317 clk per pixel

	// [38] store 30 words (=120 pixels)
2:	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
3:	subs	r2,#30		// [1] decrement number of words
	bge	2b		// [1,2] loop next 30 words
	adds	r2,#30		// [1] restore

	// go to small transfer
	b	6f

// ---- [10 per loop] small transfer 24 pixels, speed 0.417 clk per pixel

	// [8] store 6 words (=24 pixels)
4:	stmia	r0!,{r1,r3,r4,r5,r6,r7} // [7] 6 words, 24 pixels
6:	subs	r2,#6		// [1] decrement number of words
	bge	4b		// [1,2] loop next 6 words
	adds	r2,#6		// [1] restore

	// go to single transfer
	b	8f

// ---- [5 per loop] single transfer 4 pixels, speed 1.25 clk per pixel

	// [4,5] store 1 word (=4 pixels)
7:	stmia	r0!,{r1}	// [2] 1 word, 4 pixels
8:	subs	r2,#1		// [1] loop counter
	bge	7b		// [1,2] next word

	// pop registers
	pop	{r4,r5,r6,r7,pc}


.macro key4pixlo
	ldr	    r7,[r6]
	tst     r7,r5
	beq		1f
	strb	r7,[r0,#0]
1:
	lsrs	r7,#8     
	tst     r7,r5
	beq		2f
	strb	r7,[r0,#1]
2:
	lsrs	r7,#8     
	tst     r7,r5
	beq		3f
	strb	r7,[r0,#2]
3:
	lsrs	r7,#8     
	tst     r7,r5
	beq		4f
	strb	r7,[r0,#3]
4:
.endm

.macro key4pixhi
	ldr	    r7,[r6,#4]
	tst     r7,r5
	beq		5f
	strb	r7,[r0,#4]
5:
	lsrs	r7,#8     
	tst     r7,r5
	beq		6f
	strb	r7,[r0,#5]
6:
	lsrs	r7,#8     
	tst     r7,r5
	beq		7f
	strb	r7,[r0,#6]
7:
	lsrs	r7,#8     
	tst     r7,r5
	beq		8f
	strb	r7,[r0,#7]
8:
.endm



// Registers:
//  R0          ... dst: 	destination buffer
//  R1          ... src:	source buffer
//  R2          ... w:		width in pixels
//  R3          ... scroll: source offset compare to width
//  Param1 (R4) ... offset [sp,#20]
.macro LineBlitKey n
	// push registers
	push	{r4-r7,lr}

	ldr 	r4,[sp,#20] 	// Param1: offset
	lsrs    r2,#3           // w/8
	cmp     r3,#0           // no scroll ?
	beq 	11f				

    // handle first 8 pixels
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1
	movs    r4,#8           // + 8-scroll
	subs    r4,r3
	add     r6,r3
9:	
	ldrb	r5,[r6]			// get tile byte
	cmp 	r5,#0
	beq		10f
	strb	r5,[r0]
10:
	adds    r6,#1
	adds    r0,#1
	subs    r4,#1
	bne     9b
	ldr 	r4,[sp,#20]     // Param1: offset
	movs    r5,#0xff
	bal     13f	

    // handle middle pixels scroll
11:
	movs    r5,#0xff
12:
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1
	key4pixlo
	key4pixhi
	adds    r0,#8
13:
	adds    r4,#1
	cmp     r4,#\n
	bne   	14f
	movs    r4,#0  
14:
	subs	r2,#1
	bne	 	12b	

    // handle last 8 pixels scroll
	cmp     r3,#0
	beq 	17f				// no scroll?
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1	
15:	
	ldrb	r5,[r6]			// get tile byte
	cmp 	r5,#0
	beq		16f
	strb	r5,[r0]
16:
	adds    r6,#1
	adds    r0,#1
	subs    r3,#1
	bne     15b

17:
	// pop registers and return from function
	pop	{r4-r7,pc}
.endm


.thumb_func
.global LineBlitKey32
LineBlitKey32:
	LineBlitKey 32

.thumb_func
.global LineBlitKey40
LineBlitKey40:
	LineBlitKey 40


.macro blt4pixlo
	ldr	    r7,[r6]
	strb	r7,[r0,#0]
	strb	r7,[r0,#1]
	lsrs	r7,#8     
	strb	r7,[r0,#2]
	strb	r7,[r0,#3]
	lsrs	r7,#8     
	strb	r7,[r0,#4]
	strb	r7,[r0,#5]
	lsrs	r7,#8     
	strb	r7,[r0,#6]
	strb	r7,[r0,#7]
.endm

.macro blt4pixhi
	ldr	    r7,[r6,#4]
	strb	r7,[r0,#8]
	strb	r7,[r0,#9]
	lsrs	r7,#8     
	strb	r7,[r0,#10]
	strb	r7,[r0,#11]
	lsrs	r7,#8     
	strb	r7,[r0,#12]
	strb	r7,[r0,#13]
	lsrs	r7,#8     
	strb	r7,[r0,#14]
	strb	r7,[r0,#15]
.endm


.thumb_func
.global LineBlit80
LineBlit80:
	// push registers
	push	{r4-r7,lr}

	ldr 	r4,[sp,#20] 	// Param1: offset
	lsrs    r2,#3           // w/8
	cmp     r3,#0           // no scroll ?
	beq 	29f				

    // handle first 8 pixels
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1
	movs    r4,#8           // + 8-scroll
	subs    r4,r3
	add     r6,r3
26:	
	ldrb	r5,[r6]			// get tile byte
	strb	r5,[r0]
	strb	r5,[r0,#1]
	adds    r6,#1
	adds    r0,#2
	subs    r4,#1
	bne     26b
	ldr 	r4,[sp,#20]     // Param1: offset
	bal     30f	

    // handle middle pixels scroll
29:
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1
	blt4pixlo
	blt4pixhi
	adds    r0,#16
30:
	adds    r4,#1
	cmp     r4,#40
	bne   	31f
	movs    r4,#0  
31:
	subs	r2,#1
	bne	 	29b	

    // handle last 8 pixels scroll
	cmp     r3,#0
	beq 	34f				// no scroll?
	mov     r6,r4     		// src+offset*8
	lsls    r6,#3
	adds    r6,r1	
32:	
	ldrb	r5,[r6]			// get tile byte
	strb	r5,[r0]
	strb	r5,[r0,#1]
	adds    r6,#1
	adds    r0,#2
	subs    r3,#1
	bne     32b

34:
	// pop registers and return from function
	pop	{r4-r7,pc}
