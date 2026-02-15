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
    ADD A,A
    ADD A,B 
    ADD A,C
    ADD A,D
    ADD A,E 
    ADD A,H 
    ADD A,L 
    ADD A,(HL)
    ADD A,0x12
    SUB A
    SUB B 
    SUB C
    SUB D
    SUB E 
    SUB H 
    SUB L 
    SUB (HL)
    SUB 0x12
    PUSH BC 
    PUSH DE 
    PUSH HL 
    PUSH AF
    POP BC 
    POP DE 
    POP HL 
    POP AF
    RET NZ
    RET Z
    RET NC
    RET C
    RET
    CALL LOOP
    CALL 0x12
    CALL NZ LOOP
    CALL NZ 0x1234
    CALL Z LOOP
    CALL Z 0x1234
    CALL NC LOOP
    CALL NC 0x1234
LOOP:
    JP LOOP
    HALT ;sdf

;loop:
;    HALT

