PPUCTRL = $2000
PPUMASK = $2001
PPUSTATUS = $2002
PPUSCROLL = $2005
PPUADDR = $2006
PPUDATA = $2007
OAMDMA = $4014
JOY1 = $4016

ControllerMirror = $10
InputA = ControllerMirror | $07
InputB = ControllerMirror | $06
InputSelect = ControllerMirror | $05
InputStart = ControllerMirror | $04
InputUp = ControllerMirror | $03
InputDown = ControllerMirror | $02
InputLeft = ControllerMirror | $01
InputRight = ControllerMirror | $00

OAMMirror = $02

	.org $C000

Reset:
	sei
	cld
	lda #$FF
	tax
	inx
	stx PPUCTRL
	stx PPUMASK

	bit PPUSTATUS ; Reset VBlank flag (bit 7)
VBlank1:
	lda PPUSTATUS
	bpl VBlank1

	; RAM Reset
	jsr InitRAMPtr
	
ClearRAM:
	dey
	sta ($00),Y
	bne ClearRAM
	dec $01
	bpl ClearRAM

VBlank2:
	lda PPUSTATUS
	bpl VBlank2

	; PPU is warmed up
	; Fill nametables (in VRAM) with tile 0
	lda #$20
	sta PPUADDR
	jsr InitRAMPtr
	sta PPUADDR
ClearVRAM:
	dey
	sta PPUDATA
	bne ClearVRAM
	dec $01
	bpl ClearVRAM

	; Palette initialization
	lda #$3F
	sta PPUADDR
	lda #$00
	sta PPUADDR
	ldx #$00
WritePalette:
	lda Palette,X
	sta PPUDATA
	inx
	cpx #$20
	bne WritePalette

	; Sprite 0 initialization
	lda #$01
	sta (OAMMirror << 8) | $01 ; Pattern $01


	; Ready to render
	lda #%10010000
	sta PPUCTRL
	lda #%00011110
	sta PPUMASK

InfiniteLoop:
	jmp InfiniteLoop

InitRAMPtr:
	ldy #$07
	sty $01 ; High byte of RAM address
	ldy #$00 ; Low byte
	sty $00
	tya
	rts

NMI:
	lda #OAMMirror ; DMA
	sta OAMDMA

	; Controller reading
	lsr
	sta JOY1
	lsr
	sta JOY1

	ldx #$08
ReadInput:
	; Weird but efficient way to fit controller status in one byte
	; The idea behind this is to avoid long RMW instructions, so we instead handle logic (ANDs, ASLs, ORAs) directly with A, using the carry flag as medium between "past A and future A" (before and after PLA)
	; pha
	; lda JOY1
	; lsr ; Button status is now carry flag
	; pla
	; rol ; Shifts left, bit 0 is now current button's status
	; dex
	; bne ReadInput
	; sta ControllerMirror

	; We don't actually need the extra memory so it's best to just use 8 bytes
	lda JOY1
	and #%00000001
	dex
	sta ControllerMirror,X
	bne ReadInput

	; Sprite 0 position update based on controller status
	lda InputUp
	cmp #$01
	bne ReadDown
	dec (OAMMirror << 8) | $00
ReadDown:
	lda InputDown
	cmp #$01
	bne ReadLeft
	inc (OAMMirror << 8) | $00
ReadLeft:
	lda InputLeft
	cmp #$01
	bne ReadRight
	dec (OAMMirror << 8) | $03
ReadRight:
	lda InputRight
	cmp #$01
	bne EndInputRead
	inc (OAMMirror << 8) | $03

EndInputRead:
	rti

Palette:
	.db $00,$16,$2A,$02 ; BG0 : default + RGB
	.db $00,$00,$00,$00
	.db $00,$00,$00,$00
	.db $00,$00,$00,$00
	.db $00,$16,$2A,$02 ; SP0 : default + RGB
	.db $00,$00,$00,$00
	.db $00,$00,$00,$00
	.db $00,$00,$00,$00

	.org $FFFA
	.dw NMI
	.dw Reset
	.dw Reset ; IRQ unused