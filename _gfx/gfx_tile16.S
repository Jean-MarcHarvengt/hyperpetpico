// *****************************
// Tile 16x16 blit functions
// *****************************
	.syntax unified
	.section .time_critical, "ax"
	.cpu cortex-m0plus
	.thumb

.macro key4pixlo
	ldr	    r3,[r6]
	tst     r3,r5
	beq		1f
	strb	r3,[r0,#0]
1:
	lsrs	r3,#8     
	tst     r3,r5
	beq		2f
	strb	r3,[r0,#1]
2:
	lsrs	r3,#8     
	tst     r3,r5
	beq		3f
	strb	r3,[r0,#2]
3:
	lsrs	r3,#8     
	tst     r3,r5
	beq		4f
	strb	r3,[r0,#3]
4:
.endm

.macro key4pixhi
	ldr	    r3,[r6,#4]
	tst     r3,r5
	beq		5f
	strb	r3,[r0,#4]
5:
	lsrs	r3,#8     
	tst     r3,r5
	beq		6f
	strb	r3,[r0,#5]
6:
	lsrs	r3,#8     
	tst     r3,r5
	beq		7f
	strb	r3,[r0,#6]
7:
	lsrs	r3,#8     
	tst     r3,r5
	beq		8f
	strb	r3,[r0,#7]
8:
.endm


.macro key4pixlo2
	ldr	    r3,[r6,#8]
	tst     r3,r5
	beq		1f
	strb	r3,[r0,#8]
1:
	lsrs	r3,#8     
	tst     r3,r5
	beq		2f
	strb	r3,[r0,#9]
2:
	lsrs	r3,#8     
	tst     r3,r5
	beq		3f
	strb	r3,[r0,#10]
3:
	lsrs	r3,#8     
	tst     r3,r5
	beq		4f
	strb	r3,[r0,#11]
4:
.endm

.macro key4pixhi2
	ldr	    r3,[r6,#12]
	tst     r3,r5
	beq		5f
	strb	r3,[r0,#12]
5:
	lsrs	r3,#8     
	tst     r3,r5
	beq		6f
	strb	r3,[r0,#13]
6:
	lsrs	r3,#8     
	tst     r3,r5
	beq		7f
	strb	r3,[r0,#14]
7:
	lsrs	r3,#8     
	tst     r3,r5
	beq		8f
	strb	r3,[r0,#15]
8:
.endm

// Registers:
//  R0 ... destination buffer
//  R1 ... source buffer (tile buffer)
//  R2 ... screen_width in tiles 
//  R3 ... bg color
//  Param1 (R4) ... tile definition [sp,#20]       
//  Param2      ... scroll [sp,#24]
//  Param3      ... offset [sp,#28]
//  R6 ... (temporary)
//  R7 ... (temporary)

.macro TileBlitKey n
	// push registers
	push	{r4-r7,lr}

	ldr r4, [sp,#20]        // Param1: tile def 
	ldr r7, [sp,#28] 		// Param2: offset

	ldr 	r5,[sp,#24]		// Param2: scroll	
	cmp     r5,#0
	beq 	11f				// no scroll

    // handle first tile scroll
	ldrb	r6,[r1,r7]		// tile * 256
	lsls  	r6,#8
	adds    r6,r4			// + tile buffer start
	movs    r4,#16
	subs    r4,r5
	add     r6,r5
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
	ldr r4, [sp,#20]        // Param1: tile def (restore)
	movs    r5,#0xff
	bal     13f	

    // handle middle tiles scroll
11:
	movs    r5,#0xff
12:
	ldrb	r6,[r1,r7]		// tile * 256
	lsls  	r6,#8
	adds    r6,r4			// + tile buffer start
	key4pixlo
	key4pixhi
	key4pixlo2
	key4pixhi2
	adds    r0,#16
13:
	adds    r7,#1
	cmp     r7,#\n
	bne   	14f
	movs    r7,#0  
14:
	subs	r2,#1
	bne	 	12b	

    // handle last tile scroll
	ldr 	r5,[sp,#24]		// Param2: scroll	
	cmp     r5,#0
	beq 	17f				// no scroll
	ldrb	r6,[r1,r7]		// tile * 256
	lsls  	r6,#8
	adds    r6,r4			// + tile buffer start
15:	
	ldrb	r4,[r6]			// get tile byte
	cmp 	r4,#0
	beq		16f
	strb	r4,[r0]
16:
	adds    r6,#1
	adds    r0,#1
	subs    r5,#1
	bne     15b

17:
	// pop registers and return from function
	pop	{r4-r7,pc}
.endm


.thumb_func
.global TileBlitKey16_16
TileBlitKey16_16:
	TileBlitKey 16

.thumb_func
.global TileBlitKey16_20
TileBlitKey16_20:
	TileBlitKey 20

.thumb_func
.global TileBlitKey16_40
TileBlitKey16_40:
	TileBlitKey 40
