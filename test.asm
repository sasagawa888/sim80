; test
    NOP
    LD A,1
    LD A,(HL)
    LD A,(BC)
    LD A,(DE)
    LD A,(0x1234)
    LD A,(LOOP)
    LD A,LOOP
LOOP:
    JP LOOP
    HALT ;sdf

;loop:
;    HALT

