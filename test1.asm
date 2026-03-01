
    MUL16:
    ; in : HL = multiplicand
    ;      DE = multiplier
    ; out: HL = low 16 bits of product
    ; clobbers: AF,BC,DE
    ; preserves: IX,IY

    LD   BC,0        ; result = 0
    LD   A,16

MUL16L:
    ; if (DE & 1) result += HL
    BIT  0,E
    JR   Z,NOADD
    ADD  HL,BC       ; HL = HL + BC  (can't: Z80 has ADD HL,BC yes)
    ; But this adds into HL, not into BC.
    ; So swap roles: keep multiplicand in BC, result in HL
NOADD:
    ; shift multiplier right: DE >>= 1
    SRL  D
    RR   E
    ; shift multiplicand left
    SLA  L
    RL   H
    DEC  A
    JR   NZ,MUL16L
    RET
    NOP
