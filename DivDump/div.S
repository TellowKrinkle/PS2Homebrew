.set noreorder
.set noat


.equ PACKED, 0

.data

.if PACKED == 1
.balign 16
bitTable:
.word 1 <<  0, 1 <<  1, 1 <<  2, 1 <<  3
.word 1 <<  4, 1 <<  5, 1 <<  6, 1 <<  7
.word 1 <<  8, 1 <<  9, 1 << 10, 1 << 11
.word 1 << 12, 1 << 13, 1 << 14, 1 << 15
.word 1 << 16, 1 << 17, 1 << 18, 1 << 19
.word 1 << 20, 1 << 21, 1 << 22, 1 << 23
.word 1 << 24, 1 << 25, 1 << 26, 1 << 27
.word 1 << 28, 1 << 29, 1 << 30, 1 << 31
.endif

.text

# $a0: Divisor
# $a1: Output buffer
# $a2: Current A Float
# $a3: Current A Compare
# $t0: Loop counter
# $t1: Result
# $t2: Advance A Float
# $t3: Advance A Compare [!PACKED]
# $t3: bitTable [PACKED]
# $at: Exponent Compare
# $t8: 0xffffff
# $t9: 0x800000
.globl divTest
divTest:
	addiu  $sp, $sp, -64
	lui    $a2, 0x3f80
	lui    $t0, 0x80
.if PACKED == 1
	la     $t3, bitTable
.else
	lui    $t3, 0x100
	dsll   $a3, $t3, 23
.endif
	or     $t1, $a0, $a2
	lui    $t8, 0xff
	addiu  $a4, $a2, 1
	addiu  $a5, $a2, 2
	addiu  $a6, $a2, 3
	addiu  $a7, $0,  4
	addiu  $at, $a2, -1
	ori    $t8, 0xffff
.if PACKED == 1
	mtsab  $0,  3
.else
	mtsab  $0,  4
.endif
	pextlw $a2, $a5, $a2
	mtc1   $t1, $f0
	pextlw $a4, $a6, $a4
	qmtc2  $t1, $vf1
	pextlw $a2, $a4, $a2
	ctc2   $0,  $27
	pextlw $t2, $a7, $a7
	pextlw $t2, $t2, $t2
	ppacw  $a0, $a0, $a0
	pextlw $at, $at, $at
	pextlw $at, $at, $at
	pextlw $v1, $v1, $v1
	pextlw $v1, $v1, $v1
	pextlw $t8, $t8, $t8
	pextlw $t8, $t8, $t8
	pextlw $t9, $t0, $t0
	pextlw $t9, $t9, $t9
.if PACKED == 1
	pand   $a3, $a2, $t8
	por    $a3, $a3, $t9
.endif
	qmtc2  $a2, $vf2
	paddw  $a2, $a2, $t2
	sq     $a2,  0($sp)
	paddw  $a2, $a2, $t2
	qmtc2  $a2, $vf3
	paddw  $a2, $a2, $t2
	sq     $a2, 16($sp)
	paddw  $a2, $a2, $t2
	qmtc2  $a2, $vf4
	paddw  $a2, $a2, $t2
	sq     $a2, 32($sp)
	paddw  $a2, $a2, $t2

	# Init registers
.macro INIT cop1, cop2
	vcallms \cop2
	div.s  $f2, $f1, $f0
.if \cop1 > 0
	swc1   $f3, \cop1 -  4($sp)
.endif
	lwc1   $f1, \cop1 +  4($sp)
	div.s  $f3, $f1, $f0
	swc1   $f2, \cop1 +  0($sp)
	lwc1   $f1, \cop1 +  8($sp)
	div.s  $f2, $f1, $f0
	swc1   $f3, \cop1 +  4($sp)
	lwc1   $f1, \cop1 + 12($sp)
	div.s  $f3, $f1, $f0
	swc1   $f2, \cop1 +  8($sp)
	lwc1   $f1, \cop1 + 16($sp)
.endm
	lwc1   $f1,  0($sp)
	INIT cop1= 0 cop2= 0
	INIT cop1=16 cop2=32

	qmtc2  $a2, $vf5
	paddw  $a2, $a2, $t2
	sq     $a2, 48($sp)
	paddw  $a2, $a2, $t2

mainloop:
.macro MAIN off, vf, shift
	div.s   $f2, $f1, $f0
	qmfc2   $a4, \vf
	vcallms \off * 2
	lq      $a5, (\off + 32) & 63($sp)
	pcgtw   $a6, $a4, $at
	swc1    $f3, (\off + 60) & 63($sp)
	pand    $a4, $a4, $t8
	lwc1    $f1, (\off +  4) & 63($sp)
	por     $a4, $a4, $t9
	pand    $a6, $a6, $a4
	paddw   $a4, $a6, $a4
	qmtc2   $a2, \vf
	paddw   $a2, $a2, $t2
	sq      $a2, (\off + 32) & 63($sp)
	paddw   $a2, $a2, $t2
.if PACKED == 1
	pextlw  $a6, $0,  $a4
	pextuw  $a4, $0,  $a4

	div.s   $f3, $f1, $f0
	pcgtw   $a7, $a5, $at
	swc1    $f2, (\off +  0) & 63($sp)
	pand    $a5, $a5, $t8
	lwc1    $f1, (\off +  8) & 63($sp)
	por     $a5, $a5, $t9
	pand    $a7, $a7, $a5
	pmultuw $a6, $a6, $a0
	paddw   $a5, $a7, $a5
	pmultuw $a4, $a4, $a0
	pextlw  $a7, $0,  $a5

	div.s   $f2, $f1, $f0
	qfsrv   $v0, $a6, $a6
	swc1    $f3, (\off +  4) & 63($sp)
	ppacw   $a6, $a4, $a6
	lwc1    $f1, (\off + 12) & 63($sp)
	pextuw  $a5, $0,  $a5
	pmultuw $a7, $a7, $a0
	qfsrv   $a4, $a4, $a4
	pmultuw $a5, $a5, $a0
	ppacw   $a4, $a4, $v0

	div.s   $f3, $f1, $f0
	pand    $a6, $a6, $t8
	swc1    $f2, (\off +  8) & 63($sp)
	pcgtw   $a6, $a6, $0
	lwc1    $f1, (\off + 16) & 63($sp)
	psubw   $a4, $a4, $a6
	qfsrv   $v0, $a7, $a7
	ppacw   $a7, $a5, $a7
	qfsrv   $a5, $a5, $a5
	ppacw   $a5, $a5, $v0
	pand    $a7, $a7, $t8
	pcgtw   $a7, $a7, $0
	psubw   $a5, $a5, $a7
	lq      $a6, (\off * 2 + 64) & 127($t3)
	pcgtw   $a4, $a4, $a3
	lq      $a7, (\off * 2 + 80) & 127($t3)
	paddw   $a3, $a3, $t2
	pcgtw   $a5, $a5, $a3
	paddw   $a3, $a3, $t2
	pand    $a4, $a4, $a6
	pand    $a5, $a5, $a7
.if \off == 32
	por     $t1, $a4, $a5
.else
	por     $a4, $a4, $a5
	por     $t1, $t1, $a4
.endif
.else
	pmultuw $a6, $a4, $a0
	qfsrv   $a4, $a4, $a4

	div.s   $f3, $f1, $f0
	pcgtw   $a7, $a5, $at
	swc1    $f2, (\off +  0) & 63($sp)
	pand    $a5, $a5, $t8
	lwc1    $f1, (\off +  8) & 63($sp)
	por     $a5, $a5, $t9
	pand    $a7, $a7, $a5
	pmultuw $a4, $a4, $a0
	paddw   $a5, $a7, $a5
	pmultuw $a7, $a5, $a0
	qfsrv   $a5, $a5, $a5
	pmultuw $a5, $a5, $a0
.if \shift == 0
	slt     $t1, $a3, $a6
.else
	slt     $v0, $a3, $a6
.endif
	daddu   $a3, $a3, $t3

	div.s   $f2, $f1, $f0
.if \shift == 0
	slt     $v1, $a3, $a4
	daddu   $a3, $a3, $t3
	sll     $v1, $v1, 1
.else
	sll     $v0, $v0, \shift
	slt     $v1, $a3, $a4
	daddu   $a3, $a3, $t3
	or      $t1, $t1, $v0
	sll     $v1, $v1, \shift + 1
.endif
	pcpyud  $a6, $a6, $a6
	swc1    $f3, (\off +  4) & 63($sp)
	pcpyud  $a4, $a4, $a4
	lwc1    $f1, (\off + 12) & 63($sp)
	slt     $v0, $a3, $a6
	or      $t1, $t1, $v1
	daddu   $a3, $a3, $t3
	sll     $v0, $v0, \shift + 2
	slt     $v1, $a3, $a4
	or      $t1, $t1, $v0

	div.s   $f3, $f1, $f0
	daddu   $a3, $a3, $t3
	sll     $v1, $v1, \shift + 3
	slt     $v0, $a3, $a7
	or      $t1, $t1, $v1
	daddu   $a3, $a3, $t3
	sll     $v0, $v0, \shift + 4
	slt     $v1, $a3, $a5
	or      $t1, $t1, $v0
	daddu   $a3, $a3, $t3
	pcpyud  $a7, $a7, $a7
	swc1    $f2, (\off +  8) & 63($sp)
	pcpyud  $a5, $a5, $a5
	lwc1    $f1, (\off +  16) & 63($sp)
	sll     $v1, $v1, \shift + 5
	slt     $v0, $a3, $a7
	or      $t1, $t1, $v1
	daddu   $a3, $a3, $t3
	sll     $v0, $v0, \shift + 6
	slt     $v1, $a3, $a5
	or      $t1, $t1, $v0
	daddu   $a3, $a3, $t3
	sll     $v1, $v1, \shift + 7
	or      $t1, $t1, $v0
	or      $t1, $t1, $v1
.endif
.endm

	MAIN off=32 vf=$vf2 shift= 0
	MAIN off=48 vf=$vf3 shift= 8
	MAIN off= 0 vf=$vf4 shift=16
	MAIN off=16 vf=$vf5 shift=24

.if PACKED == 1
	pcpyud $v1, $t1, $t1
	or     $t1, $t1, $v1
	addiu  $t0, $t0, -32
	dsrl   $v1, $t1, 32
	addiu  $a1, $a1, 4
	or     $t1, $t1, $v1
.else
	addiu  $t0, $t0, -32
	addiu  $a1, $a1, 4
.endif
	bne    $t0, $0, mainloop
	sw     $t1, -4($a1)

	jr     $ra
	addiu  $sp,  $sp,  64
