;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  miKroTetRIS  v 2.1  Started 4/6/1992  by MAK-TRAXON's Prophet           ;;
;;  bootable version, adapted 14/2/1993                                     ;;
;;  latest update: 14/2/1993                                                ;;
;;  a TETRIS in less than 1/2 K !                                           ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; the bootable version is executed at 0:7c00 or 7c0:0, which it translates
; to 7b0:100. Vars are held in the 7b0:0 .. 7b0:ff area.

%define by byte
%define wo word
%define jmps jmp short

;;;;;;;;;;;;;;;;;;;;;;;;;;;;; VARS at 7b0:0 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

                org 0
%define arrlines 0             ; lines arrray: number of filled squares in
                               ; each line
                               ; must be at least 1 byte longer than the
                               ; actual nb of lines (for optimisation in
                               ; the "lines" procedure)

%define timecorr arrlines+24   ; clock ticks at the start of prog
%define randseed timecorr+2    ; for the pseudo-random number succession
                       ; these 2 vars must be just after arrlines (for
                       ; optimisation at init)

;;;;;;;;;;;;;;;;;;;;;;;;;; ASSEMBLER CONSTANTS ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

%define wwidth    10  ; width of Tetris well
%define height    20  ; height of Tetris well
%define firstline 3   ; first screen line used for Tetris well
%define firstcol  30  ; first screen col used for Tetris well
                      ; must be even

%define empty     178 ; char used to fill empty positions
%define falling   255 ; char used to draw the falling piece
%define steady    32  ; char used to draw the steady pieces
                      ; must be lower than the other 2 (for optimisation in
                      ; the "hits" procedure)

                      ; the three are used in reverse video mode
                      ; (attribute 70h)

%define left      71  ; scan code of key used to move left ( num. pad 7 )
%define right     73  ; scan code of key used to move right ( num. pad 9 )
%define krotation 72  ; scan code of key used for rotation ( num. pad 8 )
%define kdrop     57  ; scan code of key used to drop ( space )

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; MAIN PROG ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

org 0x100

entpt:          jmp short start ; real start of code
dosdata         times 0x30 db 0
                db 'BooTris by MAK-TRAXON s Prophet'

start:          db 0eah    ; opcode of immediate inter-segment jump
                dw continue
                dw 7b0h    ; segment
                ; sets CS to 7b0

continue:       cld  ; assumed in the rest of the program

restart:        xor ax,ax
                mov ss,ax
                mov sp,7b00h    ; create a stack

                push cs
                pop es
                push cs
                pop ds          ; set data segments

                mov di,arrlines
                xor ax,ax
                mov cx,height+4  ; must be the exact size of the buffer
                rep stosb        ; clear lines array

; set mode 3, remove cursor, set ES = 0b800h, randomize

                mov al,3         ; ah = 0
                int 10h
                mov ch,0b8h
                mov es,cx

                int 1ah          ; get time (ah=0)
                mov [di],dx      ; save ticks in "timecorr"
                mov [di+2],dx    ; randomize (save ticks in "randseed")

                inc ah           ; -> ah = 1
                mov ch,20h
                int 10h          ; remove cursor

; display the Tetris screen:
; ² = chr(178) is the background, spaces are used for the steady pieces,
; chr(255) is used for falling pieces
; all chars have the inverse video (70h) attribute
; the well is surrounded by spaces => no need for special checks for the
; border of the well.

                mov di,160*firstline+2*firstcol  ; position on the screen
                mov bp,height                    ; nb of lines
do1line:        mov ax,7000h+empty               ; attrib and char
                mov cx,2*wwidth                  ; nb of chars
                rep stosw
                add di,byte 160-4*wwidth              ; next line
                dec bp
                jnz do1line                      ; loop

                ; now BP = 0 (used for time handling)

gameloop:
; 8 * (random between 0 and 6) -> AX
randloop:       mov ax,1001
                mul word [randseed]
                inc ax
                mov [randseed],ax   ; calc next random
                and ax,38h
                cmp al,30h
                ja randloop       ; if not between 0 and 30h -> try again

                mov [rotation],ah   ; ah = 0

                add ax,pieces
                mov [thepiece],ax       ; calc & save adr of current piece
                mov bx,7+100h*height  ; bl = x position of the falling piece
                                      ; bh = y position of the falling piece
                                      ; start position :  x = 7, y = height
                call hits       ; test if current piece hits something
                jc restart      ; if it does, restart game

whilefalling:   call drawfalling; draw the piece with the 'falling' char
                add bp,byte 3        ; ticks+3 -> bp
looptime:       call keyboard
                xor ah,ah
                int 1ah         ; read ticks
                sub dx,[timecorr] ; calc ticks from start of game
                cmp dx,bp
                jb looptime     ; loop keyboard for 3 clock ticks

; Time handling will fail if the game is more than 65536/18.21 = 3598s long,
; and at midnight when the ticks value drops to 0.
; If the game is paused using Ctrl-NumLock (or Pause on AT keyboards),
; the pieces will "free-fall" afterwards !

		nop
                call putempty   ; remove piece
                dec bh          ; one down
                call hits       ; if does not hit anything
                jnc whilefalling; then loop
                inc bh          ; up again
                mov al,steady
                call put        ; draw it steady

; if some lines are full, remove them

lines:          mov cx,height
                mov si,arrlines
looplines:      lodsb
                cmp al,wwidth
                jnb line        ; scan lines array for full lines
                loop looplines

                jmps gameloop   ; return to game when there are no more
                                ; lines to remove

line:           push si         ; needed for later
                push cx         ; needed for later

; remove line; cx=line number from 1 to 20 (20=bottom)

                mov si,160*firstline - 2          ; position on the screen
                mov di,160+160*firstline - 2
                dec cx
                mov al,160
                mul cl
                mov cx,ax                         ; length to move
                push ds
                push es
                pop ds                            ; set segments
                add di,ax                         ; end position (for std move)
                add si,ax
                std
                rep movsb                         ; scroll lines
                cld                               ; cld assumed by the prog
                pop ds                            ; reset segment

                pop cx
                pop si                            ; recover saved regs

fix_arr:        lodsb               ; fix lines array
                mov [si-2],al
                loop fix_arr
                jmps lines          ; restart the lines procedure

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; STRUCTURES ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

struc descrpiece       ; encoding: for each piece, the 4 positions are
.cubes1_2       resb 1 ; stored in 2 words each, split into 8 2-bit fields
.cubes3_4       resb 1 ; for the X and Y coord of each square in a 4x4
endstruc               ; matrix

struc piece
.shape1         resb 2
.shape2         resb 2
.shape3         resb 2
.shape4         resb 2
endstruc

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; CONSTANTS ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

pieces:
piece1:
;                ++++++++

istruc descrpiece
	db 0x26, 0xae
iend
istruc descrpiece
	db 0x89, 0xab
iend
istruc descrpiece
	db 0x26, 0xae
iend
istruc descrpiece
	db 0x89, 0xab
iend

piece2:
;               ++++
;               ++++
istruc descrpiece
	db 0x6a, 0x59
iend
istruc descrpiece
	db 0x6a, 0x59
iend
istruc descrpiece
	db 0x6a, 0x59
iend
istruc descrpiece
	db 0x6a, 0x59
iend

piece3:
;               ++++++
;                 ++
istruc descrpiece
	db 0x96, 0xae
iend
istruc descrpiece
	db 0x9a, 0xbe
iend
istruc descrpiece
	db 0x59, 0xda
iend
istruc descrpiece
	db 0x69, 0xab
iend

piece4:
;               ++++++
;                   ++
istruc descrpiece
	db 0x6a, 0xde
iend
istruc descrpiece
	db 0x9a, 0xbf
iend
istruc descrpiece
	db 0x59, 0xd6
iend
istruc descrpiece
	db 0x59, 0xab
iend

piece5:
;               ++++++
;               ++
istruc descrpiece
	db 0x56, 0xae
iend
istruc descrpiece
	db 0x9a, 0xbd
iend
istruc descrpiece
	db 0x59, 0xde
iend
istruc descrpiece
	db 0x79, 0xab
iend

piece6:
;                 ++++
;               ++++
istruc descrpiece
	db 0x59, 0xae
iend
istruc descrpiece
	db 0xab, 0xde
iend
istruc descrpiece
	db 0x59, 0xae
iend
istruc descrpiece
	db 0xab, 0xde
iend

piece7:
;               ++++
;                 ++++
istruc descrpiece
	db 0x69, 0xad
iend
istruc descrpiece
	db 0x9a, 0xef
iend
istruc descrpiece
	db 0x69, 0xad
iend
istruc descrpiece
	db 0x9a, 0xef
iend

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; PROCEDURES ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

readpos:
; input : CX = number of the square in the current piece ( 1 to 4 )
; output: DI = offset in screen memory, DX = line number
;         CX is saved
                push cx         ; save CX
                dec cx
                shr cx,1
                pushf
                add cl,[rotation]
                mov si,cx       ; calc address of byte to get
                                ; 2 bytes / position

                db 08ah, 84h    ; opcode of mov al,[si+constant]
thepiece        dw 0            ; self-modifying prog: 
                                ; thepiece = adr of current piece data

                popf
                mov cl,4
                jnc norott
                shr al,cl       ; get the correct bits in the low nybble
norott:         and ax,0fh      ; clear high nybble
                mov dx,ax
                and dl,03h
                shr cl,1        ; cl = 4 -> cl = 2
                shr ax,cl
                add al,bl       ; al = col , ah = 0
                add dl,bh       ; dl = line , dh = 0

;  firstline*160 + firstcol*2 + 160*(19-dx) + 4*ax + 3*160 - 16      ->  DI
;  firstline*160 + firstcol*2 - 160*dx + 160*19 + 4*ax + 3*160 - 16  ->  DI
;  4 * [ 40*19+firstcol/2+40*firstline+3*40-4 + ax - 40*dx ]         ->  DI
;  ( 19 is height-1 )
;  without changing DX; assumes CL=2
                mov di,ax
                mov al,40
                mul dl
                add di,40*(height - 1) + firstcol/2 + 40*firstline + 3*40 - 4
                sub di,ax
                shl di,cl       ; do the calculation

                pop cx          ; restore cx
                ret

putempty:
                mov al,empty
                                 ; no RET: enters "put" with al=empty

put:
; input: AL = char
; draws the current piece in the current position, with the char in AL
; (AL=empty to clear the piece).
; does not check if it "hits" anything
                mov cx,4         ; 4 squares
loopput:        push ax
                call readpos     ; calc position in screen mem in DI,
                                 ; line number in DX
                pop ax
                mov si,dx
                stosb            ; put char
                inc di
                stosb            ; 1 square = 2 chars
                cmp al,steady+1
	        ;adc by arrlines[-3][si],0  ; update lines array
                adc byte [si-0x3],0  ; update lines array
nosteady:       loop loopput
;               ret             ; this is SAVAGE optimization: executing
                                ; the next procedure here does no harm,
                                ; so why not remove the RET ?


hits:
; input: none
; output: CF = 1 if the current piece in the current position hits other
; pieces on the screen or does not fit in the well, else CF = 0
                mov cx,4
loophits:       call readpos              ; calc position in screen mem in DI
                cmp byte [es:di],steady+1   ; is steady?
                jc a_ret                  ; yes -> exit with CF=1
                loop loophits             ; test the 4 squares
a_ret:          ret                       ; no need for CLC: loop does not 
                                          ; change CF


keyboard:
; read the keyboard, process keys
                mov ah,1
                int 16h         ; keypressed?
                jz a_ret        ; if not keypressed -> ret

                call putempty   ; remove piece from screen
                mov ah,0
                int 16h         ; read key
                mov al,ah       ; scan code in AL

                cmp al,left
                jnz notleft     ; if key is left
                dec bl          ; move left
                call hits       ; does it hit ?
                adc bl,0        ; if it does, move right again

drawfalling:                    ; call here to draw a falling piece
put_exit:       mov al,falling
                jmps put        ; draw piece in new position
                                ; "put" will do its ret

notleft:        cmp al,right
                jnz notright    ; if key is right
                inc bl          ; move right
                call hits       ; does it hit?
                sbb bl,0        ; if it does: move left again
right_ok:       jmps put_exit   ; draw piece & exit

notright:       cmp al,krotation; if key is rotation
                jnz notrot

                db 0xb0         ; opcode for mov al,immediate
rotation:       db 0            ; self-modifying code: mov al,rotation
                                ; rotation = current rotation of the piece
                                ; always even, from 0 to 6

                push ax         ; save rotation
                inc ax
                inc ax
                and al,6        ; calc new rot: add 2 and modulo 6
                                ; even number => modulo 6 = and 6
again:          mov [rotation],al ; set new rot
                call hits       ; does it hit ?
                pop ax          ; recover AX
                jnc put_exit    ; if it does
                push ax         ; AX back in the stack
                jmps again      ; try again (with old AX so it will not fail)

notrot:         cmp al,kdrop    ; key is drop?
                jnz nodrop      ; if not, redraw piece & exit
loopdrop:       dec bh          ; down 1 level
                call hits       ; does it hit?
                jnc loopdrop    ; if not, loop
                inc bh          ; up 1 level
                jmps put_exit   ; draw piece & exit

nodrop:         cmp al,1
                jnz put_exit
                int 19h

; the DROP key only brings the piece as low as it can go, so the playes still
; has time to move it sidewise.



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;  END  ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
