;
; Copyright (C) 2017 Tobias Diedrich
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

	.module startup

; global variables
	.globl _main_stack
	.globl _setup
	.globl _loop
	.globl _usb_reg_data
	.globl _usb_reset_regs

;--------------------------------------------------------
; defines
;--------------------------------------------------------

	.area DSEG    (DATA)
; Local vars


;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
DPL1	=	0x0084
DPH1	=	0x0085
DPS	=	0x0092
SRAM_BASE	=	0x8000
SRAM_SIZE	=	0x0800
AR0	=	0
AR1	=	1
AR2	=	2
AR3	=	3
AR4	=	4
AR5	=	5
AR6	=	6
AR7	=	7

; Stack
	.org 0xb0
_main_stack: ; 1 x 32 ( 0xb0 - 0xcf )
	.ds 32

	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
	.area REG_BANK_1	(REL,OVR,DATA)
	.ds 8
	.area REG_BANK_2	(REL,OVR,DATA)
	.ds 8
	.area REG_BANK_3	(REL,OVR,DATA)
	.ds 8

;--------------------------------------------------------
; reset & interrupt vectors
;--------------------------------------------------------
	.area CSEG    (ABS,CODE)
	.org SRAM_BASE
reset:
	clr	PSW
	mov	DPS, #1

	mov	DPH, #0x11
	mov	DPL, #0x22

	clr	A
	mov	R0, A
clear_iram:
	mov	@R0, A
	djnz	R0, clear_iram

	mov	DPTR, #SRAM_BASE
	; R0 is 0 again
	mov	R1, #(SRAM_SIZE / 256)
clear_sram:
	movx	@DPTR, A
	inc	DPTR
	djnz	R0, clear_sram
	djnz	R1, clear_sram

	; Setup SP so lcall will store PC in R6/R7
	mov	SP, #5
	mov	DPTR, #SRAM_BASE
	mov	A, #0x22  ; RET opcode
	movx	@DPTR, A
	clr	A
	dec	DPS
	lcall	#SRAM_BASE
dptr_marker:
	; dptr_marker is now in R6/R7
	mov	A, R6
	clr	C
	subb	A, #(dptr_marker - reset)
	mov	DPL, A
	mov	A, R7
	subb	A, #0
	mov	DPH, A

	; copy code starting at mainloop
	mov	R0, #(endofdata - SRAM_BASE)
	mov	R1, #((endofdata + 0x100 - SRAM_BASE) >> 8)
relocate_loop:
	clr	A
	movc	A, @A+DPTR
	inc	DPTR
	dec	DPS
	movx	@DPTR, A
	inc	DPTR
	inc	DPS
	djnz	R0, relocate_loop
	djnz	R1, relocate_loop

	clr	DPS
	mov	SP, #_main_stack
	lcall _setup
	ljmp	mainloop

mainloop:
	acall _loop
	sjmp mainloop

_usb_reset_regs:
	mov	DPTR, #_usb_reg_data
usb_reset_regs_loop:
	movx	A, @DPTR
	jz	usb_reset_regs_exit
	inc	DPTR
	mov	DPL1, A
	movx	A, @DPTR
	inc	DPTR
	inc	DPS
	mov	DPH1, #0xc7
	movx	@DPTR, A
	dec	DPS
	sjmp	usb_reset_regs_loop
usb_reset_regs_exit:
	ret

	.area CONST   (CODE)

	.area XINIT   (CODE)

	.area XISEG   (CODE)

	.area ENDOFDATA  (CODE)
endofdata:
