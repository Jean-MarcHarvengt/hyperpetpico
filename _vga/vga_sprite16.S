// *****************************
// Sprite 16x16 functions
// *****************************
#define SPR_H 24

	.syntax unified
	.section .time_critical, "ax"
	.cpu cortex-m0plus
	.thumb

.macro key4pixlo
	ldr	    r4,[r6]
	tst     r4,r5
	beq		1f
	strb	r4,[r7,#0]
1:
	lsrs	r4,#8     
	tst     r4,r5
	beq		2f
	strb	r4,[r7,#1]
2:
	lsrs	r4,#8     
	tst     r4,r5
	beq		3f
	strb	r4,[r7,#2]
3:
	lsrs	r4,#8     
	tst     r4,r5
	beq		4f
	strb	r4,[r7,#3]
4:
.endm

.macro key4pixhi
	ldr	    r4,[r6,#4]
	tst     r4,r5
	beq		5f
	strb	r4,[r7,#4]
5:
	lsrs	r4,#8     
	tst     r4,r5
	beq		6f
	strb	r4,[r7,#5]
6:
	lsrs	r4,#8     
	tst     r4,r5
	beq		7f
	strb	r4,[r7,#6]
7:
	lsrs	r4,#8     
	tst     r4,r5
	beq		8f
	strb	r4,[r7,#7]
8:
.endm


.macro key4pixlo2
	ldr	    r4,[r6,#8]
	tst     r4,r5
	beq		9f
	strb	r4,[r7,#8]
9:
	lsrs	r4,#8     
	tst     r4,r5
	beq		10f
	strb	r4,[r7,#9]
10:
	lsrs	r4,#8     
	tst     r4,r5
	beq		11f
	strb	r4,[r7,#10]
11:
	lsrs	r4,#8     
	tst     r4,r5
	beq		12f
	strb	r4,[r7,#11]
12:
.endm

.macro key4pixhi2
	ldr	    r4,[r6,#12]
	tst     r4,r5
	beq		13f
	strb	r4,[r7,#12]
13:
	lsrs	r4,#8     
	tst     r4,r5
	beq		14f
	strb	r4,[r7,#13]
14:
	lsrs	r4,#8     
	tst     r4,r5
	beq		15f
	strb	r4,[r7,#14]
15:
	lsrs	r4,#8     
	tst     r4,r5
	beq		16f
	strb	r4,[r7,#15]
16:
.endm


// flip H

.macro fkey4pixlo
	ldr	    r4,[r6]
	tst     r4,r5
	beq		1f
	strb	r4,[r7,#15]
1:
	lsrs	r4,#8     
	tst     r4,r5
	beq		2f
	strb	r4,[r7,#14]
2:
	lsrs	r4,#8     
	tst     r4,r5
	beq		3f
	strb	r4,[r7,#13]
3:
	lsrs	r4,#8     
	tst     r4,r5
	beq		4f
	strb	r4,[r7,#12]
4:
.endm

.macro fkey4pixhi
	ldr	    r4,[r6,#4]
	tst     r4,r5
	beq		5f
	strb	r4,[r7,#11]
5:
	lsrs	r4,#8     
	tst     r4,r5
	beq		6f
	strb	r4,[r7,#10]
6:
	lsrs	r4,#8     
	tst     r4,r5
	beq		7f
	strb	r4,[r7,#9]
7:
	lsrs	r4,#8     
	tst     r4,r5
	beq		8f
	strb	r4,[r7,#8]
8:
.endm

.macro fkey4pixlo2
	ldr	    r4,[r6,#8]
	tst     r4,r5
	beq		9f
	strb	r4,[r7,#7]
9:
	lsrs	r4,#8     
	tst     r4,r5
	beq		10f
	strb	r4,[r7,#6]
10:
	lsrs	r4,#8     
	tst     r4,r5
	beq		11f
	strb	r4,[r7,#5]
11:
	lsrs	r4,#8     
	tst     r4,r5
	beq		12f
	strb	r4,[r7,#4]
12:
.endm

.macro fkey4pixhi2
	ldr	    r4,[r6,#12]
	tst     r4,r5
	beq		13f
	strb	r4,[r7,#3]
13:
	lsrs	r4,#8     
	tst     r4,r5
	beq		14f
	strb	r4,[r7,#2]
14:
	lsrs	r4,#8     
	tst     r4,r5
	beq		15f
	strb	r4,[r7,#1]
15:
	lsrs	r4,#8     
	tst     r4,r5
	beq		16f
	strb	r4,[r7,#0]
16:
.endm




// Registers:
//  R0 ... nb sprite
//  R1 ... screen_width
//  R2 ... scanline 
//  R3 ... spr data
//  Param1 ... sprite definition [sp,#20]       
//  Param2 ... destination buffer [sp,#28]
//  R4 ... (temporary)
//  R5 ... (temporary)
//  R6 ... (temporary)
//  R7 ... (temporary)


.thumb_func
.global Sprite16x
Sprite16x:
	// push registers
	push	{r4-r7,lr}

17:
	ldrb	r4,[r3]			// spry
	subs    r4,r2,r4        // scanline-spry
	bmi     24f             // skip
	cmp     r4,#SPR_H
	bpl     24f             // skip
	ldrb    r6,[r3,#1]		// sprid
	lsls    r6, #8          // * 256

	ldrb    r7,[r3,#3]		// flip V
	cmp     r7,#0
	beq 	18f
	movs    r7,#(SPR_H-1)
	subs    r4,r7,r4
18:

	lsls    r4, #4          // spr yoff*16
	adds    r6,r4           // spr data line pt
	ldr 	r4, [sp,#20]    // Param1: sprite def 
	adds    r6,r4

	ldr     r4,[r3,#4] 		// sprx

	cmp     r4,r1
	bpl     24f         	// skip  
	mov     r7,r1
	subs    r7,#16
	cmp     r4,r7	
	bpl     19f 			// crop right

	// all sprite fits on line
	ldr 	r7, [sp,#24]    // Param2: dst
	adds	r7,r4
	movs    r5,#0xff		// pixel mask

	ldrb    r4,[r3,#3]		// flip H
	cmp     r4,#0
	bne 	25f

	key4pixlo               // No flip H
	key4pixhi
	key4pixlo2
	key4pixhi2
	bal 	24f

	// crop right
19:
	ldr 	r7, [sp,#24]    // dst
	adds	r7,r4
	subs    r4,r1,r4
	ldrb    r5,[r3,#3]		// flip H?
	cmp     r5,#0
	beq 	22f
	
20:	
	ldrb	r5,[r6,#15]		// yes, get tile byte
	cmp 	r5,#0
	beq		21f
	strb	r5,[r7]
21:
	subs    r6,#1
	adds    r7,#1
	subs    r4,#1
	bne     20b
	bal 	24f

22:							// no flip H
	ldrb	r5,[r6]			// get tile byte
	cmp 	r5,#0
	beq		23f
	strb	r5,[r7]
23:
	adds    r6,#1
	adds    r7,#1
	subs    r4,#1
	bne     22b


24:
	adds    r3,#8 			// Next sprite...
	subs	r0,#1
	bne	 	17b

	// pop registers and return from function
	pop	{r4-r7,pc}

25:
	fkey4pixlo        		// flip H
	fkey4pixhi
	fkey4pixlo2
	fkey4pixhi2
	bal 	24b

//-------------- 
.thumb_func
.global Sprite16
Sprite16:
	// push registers
	push	{r4-r7,lr}
	sub     sp,#4
	movs	r4,r0
	lsrs    r4,#8
	str     r4,[sp,#0]
	movs    r4,#0xff
	ands    r0,r4

17:
	ldrb	r4,[r3]			// spry
	subs    r4,r2,r4        // scanline-spry
	bmi     26f             // skip
	cmp     r4,#SPR_H
	bpl     26f             // skip
	ldrb    r6,[r3,#1]		// sprid
	lsls    r6, #8          // * 256

	ldrb    r7,[r3,#3]		// flip V
	cmp     r7,#0
	beq 	18f
	movs    r7,#(SPR_H-1)
	subs    r4,r7,r4
18:

	lsls    r4, #4          // spr yoff*16
	adds    r6,r4           // spr data line pt
	ldr 	r4, [sp,#24]    // Param1: sprite def 
	adds    r6,r4

	ldr     r4,[r3,#4] 		// sprx

	cmp     r4,r1
	bpl     26f         	// skip  
	mov     r7,r1
	subs    r7,#16
	cmp     r4,r7	
	bpl     19f 			// crop right

	// all sprite fits on line
	ldr 	r7, [sp,#28]    // Param2: dst
	adds	r7,r4
	movs    r5,#0xff		// pixel mask

	ldrb    r4,[r3,#3]		// flip H
	cmp     r4,#0
	bne 	25f

	key4pixlo               // No flip H
	key4pixhi
	key4pixlo2
	key4pixhi2
	bal 	24f

	// crop right
19:
	ldr 	r7, [sp,#28]    // dst
	adds	r7,r4
	subs    r4,r1,r4
	ldrb    r5,[r3,#3]		// flip H?
	cmp     r5,#0
	beq 	22f
	
20:	
	ldrb	r5,[r6,#15]		// yes, get tile byte
	cmp 	r5,#0
	beq		21f
	strb	r5,[r7]
21:
	subs    r6,#1
	adds    r7,#1
	subs    r4,#1
	bne     20b
	bal 	24f

22:							// no flip H
	ldrb	r5,[r6]			// get tile byte
	cmp 	r5,#0
	beq		23f
	strb	r5,[r7]
23:
	adds    r6,#1
	adds    r7,#1
	subs    r4,#1
	bne     22b

24: 
	ldr     r4,[sp,#0]     // max sprites per line drawn reached
	subs    r4,#1
	beq     27f
	str     r4,[sp,#0]
26:
	adds    r3,#8 			// Next sprite...    
	subs	r0,#1
	bne	 	17b

27:
	add     sp,#4
	// pop registers and return from function
	pop	{r4-r7,pc}

25:
	fkey4pixlo        		// flip H
	fkey4pixhi
	fkey4pixlo2
	fkey4pixhi2
	bal 	24b