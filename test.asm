; test
    NOP
    JP NZ,LOOP
    LD A,1
    LD A,(HL)
    LD A,(BC)
    LD A,(DE)
    LD A,(0x1234)
ABCD:
    LD A,(LOOP)
    LD A,LOOP
    JR NZ,0X0000
    JR NZ,ABCD
    JR Z,0x0000
    JR Z,ABCD
    JR NC,0x0000
    JR NC,ABCD 
    JR C,0x0000
    JR C,DEFG
    JR 0x0000
    JR DEFG
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
DEFG:
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
 LOOP2:
    RET
    CALL LOOP
    CALL 0x12
    CALL NZ,LOOP
    CALL NZ,0x1234
    CALL Z,LOOP
    CALL Z,0x1234
    CALL NC,LOOP
    CALL NC,0x1234
    JP NZ,LOOP
    JP Z,LOOP
    JP NC,LOOP2
    JP C,LOOP2
    JP PE,LOOP
    JP PO,LOOP 
    JP P,LOOP 
    JP M,LOOP 

LOOP:
    JP LOOP3
    JP 0x1234
    HALT ;sdf

    RST 0x00
    RST 0x08
    RST 0x10
    RST 0x18
    RST 0x20
    RST 0x28
    RST 0x30
    RST 0x38

    ADC A,A
    ADC A,B 
    ADC A,C
    ADC A,D
    ADC A,E 
    ADC A,H 
    ADC A,L 
    ADC A,(HL)
    ADC A,0x12

    SBC A,A
    SBC A,B 
    SBC A,C
    SBC A,D
    SBC A,E 
    SBC A,H 
    SBC A,L 
    SBC A,(HL)
    SBC A,0x12

LOOP3:
;loop:
;    HALT

