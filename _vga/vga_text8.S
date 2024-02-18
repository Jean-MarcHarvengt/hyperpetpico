// *****************************
// Text 8x8 font blit functions
// *****************************
	.syntax unified
	.section .time_critical, "ax"
	.cpu cortex-m0plus
	.thumb

.macro bit2pix n
	asrs    r6,#1
	bcc		3f
	strb	r3,[r0,#\n]	
3:
.endm


// Registers:
//  R0 ... destination buffer
//  R1 ... source buffer (char buffer)
//  R2 ... screen_width in chars
//  R3 ... fg color
//  Param1 (R4) ... font definition  [sp,#20]       
//  Param2      ... scroll [sp,#24]
//  Param3      ... offset [sp,#28]
//  R6 ... (temporary)
//  R7 ... (temporary)

.macro TextBlitKey n
	// push registers
	push	{r4-r7,lr}

	ldr r4, [sp,#20]        // Param1: font def 
	ldr r7, [sp,#28] 		// Param2: offset

	movs 	r5,#8
	ldr 	r6,[sp,#24]		// Param2: scroll	
	cmp     r6,#0
	beq 	6f				// no scroll

    // handle first char scroll
	subs	r5,r6			// 7-(scroll)
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r6,[r6]			// get char line bits
	rors    r6,r5
4:
	lsls	r6,#1    		// show first pixels
	bcc		5f
	strb	r3,[r0]	
5:	adds    r0,#1
	subs    r5,#1
	bne	 	4b
	bal     7f

    // handle middle chars scroll
6:
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r6,[r6]			// get char line bits
	bit2pix 7
	bit2pix 6
	bit2pix 5
	bit2pix 4
	bit2pix 3
	bit2pix 2
	bit2pix 1
	bit2pix 0	
	adds	r0,#8
7:
	adds    r7,#1
	cmp     r7,#\n
	bne   	8f
	movs    r7,#0  
8:
	subs	r2,#1
	bne	 	6b	

    // handle last char scroll
	ldr 	r5,[sp,#24]		// Param2: scroll	
	cmp     r5,#0
	beq 	11f				// no scroll
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r6,[r6]			// get char line bits
	
	movs    r7,#8
	rors    r6,r7
9:
	lsls	r6,#1    		// show first pixels
	bcc		10f
	strb	r3,[r0]	
10:	adds    r0,#1
	subs    r5,#1
	bne	 	9b
11:

	// pop registers and return from function
	pop	{r4-r7,pc}
.endm


.thumb_func
.global TextBlitKey32
TextBlitKey32:
	TextBlitKey 32

.thumb_func
.global TextBlitKey40
TextBlitKey40:
	TextBlitKey 40

.thumb_func
.global TextBlitKey80
TextBlitKey80:
	TextBlitKey 80


// Registers:
//  R0 ... destination buffer
//  R1 ... source buffer (char buffer)
//  R2 ... screen_width in chars
//  R3 ... fgcolorlut
//  Param1 (R4) ... font definition  [sp,#20]       
//  Param2      ... scroll [sp,#24]
//  Param3      ... offset [sp,#28]
//  R6 ... (temporary)
//  R7 ... (temporary)

.macro TextBlit n
	// push registers
	push	{r4-r7,lr}

	ldr r4, [sp,#20]        // Param1: font def 
	ldr r7, [sp,#28] 		// Param2: offset

	movs 	r5,#8
	ldr 	r6,[sp,#24]		// Param2: scroll	
	cmp     r6,#0
	beq 	12f				// no scroll

    // handle first char scroll
	subs	r5,r6			// 7-(scroll)
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r6,[r6]			// get char line bits
	rors    r6,r5
12:
	lsls	r6,#1    		// show first pixels
	bcc		13f
	strb	r3,[r0]	
13:	adds    r0,#1
	subs    r5,#1
	bne	 	12b
	bal     13f

    // handle middle chars scroll
12:
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r5,[r6]			// get char line bits
	lsrs    r5,#4           // highest 4 bits
	lsls    r5,#4
	lsls    r5,#2
;	movs     r5,#4
	adds    r5,r3           // fgcolor lut
	ldr     r5,[r5]
	strb	r5,[r0,#0]
	lsrs    r5,#8
	strb	r5,[r0,#1]
	lsrs    r5,#8
	strb	r5,[r0,#2]
	lsrs    r5,#8
	strb	r5,[r0,#3]
 	
	ldrb	r5,[r6]			// get char line bits
	lsrs    r5,#4           // lowest 4 bits
	lsls    r5,#4
	lsls    r5,#2	
;	movs    r5,#4
	adds    r5,r3           // fgcolor lut
	ldr     r5,[r5]
	strb	r5,[r0,#4]
	lsrs    r5,#8
	strb	r5,[r0,#5]
	lsrs    r5,#8
	strb	r5,[r0,#6]
	lsrs    r5,#8
	strb	r5,[r0,#7]



	adds	r0,#8
13:
	adds    r7,#1
	cmp     r7,#\n
	bne   	14f
	movs    r7,#0  
14:
	subs	r2,#1
	bne	 	12b	

    // handle last char scroll
	ldr 	r5,[sp,#24]		// Param2: scroll	
	cmp     r5,#0
	beq 	17f				// no scroll
	ldrb	r6,[r1,r7]		// char * 8
	lsls  	r6,#3
	adds    r6,r4			// + font buffer start
	ldrb	r6,[r6]			// get char line bits
	
	movs    r7,#8
	rors    r6,r7
15:
	lsls	r6,#1    		// show first pixels
	bcc		16f
	strb	r3,[r0]	
16:	adds    r0,#1
	subs    r5,#1
	bne	 	15b
17:

	// pop registers and return from function
	pop	{r4-r7,pc}
.endm


.thumb_func
.global TextBlit32
TextBlit32:
	TextBlit 32

.thumb_func
.global TextBlit40
TextBlit40:
	TextBlit 40

.thumb_func
.global TextBlit80
TextBlit80:
	TextBlit 80