; test
    NOP
    LD A,1
    LD A,(HL)
    LD A,(BC)
    LD A,(DE)
    LD A,(0x1234)
    LD A,(LOOP)
    LD A,LOOP
    DEC A
    DEC B
    DEC C
    DEC D
    DEC E 
    DEC H 
    DEC L
    INC A
    INC B 
    INC C 
    INC D 
    INC E
    INC H 
    INC L
LOOP:
    JP LOOP
    HALT ;sdf

;loop:
;    HALT

