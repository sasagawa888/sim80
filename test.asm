FOO1:   EQU 0x12
BOO:    EQU 123
    LD A,FOO1
    LD A,BOO
ABCD:
    RES 0,B      ; CB 80
    RES 0,(HL)   ; CB 86
    RES 0,A      ; CB 87
    RES 7,B      ; CB B8
    RES 7,(HL)   ; CB BE
    RES 7,A      ; CB BF
    SET 0,B      ; CB C0
    SET 0,A      ; CB C7
    SET 0,(HL)   ; CB C6
    SET 7,B      ; CB F8
    SET 7,A      ; CB FF
    SET 7,(HL)   ; CB FE
    BIT 0,A
    BIT 1,B
    BIT 2,C
    BIT 3,D
    BIT 4,E
    BIT 5,H
    BIT 6,L
    BIT 7,A
    BIT 0,(HL)
    BIT 3,(HL)
    BIT 7,(HL)
    BIT 0,B
    BIT 7,B
    LD (HL),A
    LD (HL),B 
    LD (HL),C
    LD (HL),D 
    LD (HL),E 
    LD (HL),H
    LD (HL),L
    LD A,(LOOP)
    LD A,LOOP
    JR NZ,0X0077
    JR NZ,ABCD
    JR Z,0x0077
    JR Z,ABCD
    JR NC,0x0077
    JR NC,ABCD 
    JR C,0x0011
    JR C,ABCD
    JR 0x0011
    JR ABCD
    NOP
    LD A,A
    LD A,B 
    LD A,C
    LD A,D 
    LD A,E
    LD A,H 
    LD A,L 
    LD A,(HL)
    LD A,0x34
    LD B,A
    LD B,B 
    LD B,C
    LD B,D 
    LD B,E
    LD B,H
    LD B,L
    LD B,(HL)
    LD B,0x34
    LD C,A
    LD C,B 
    LD C,C
    LD C,D 
    LD C,E
    LD C,H
    LD C,L
    LD C,(HL)
    LD C,0x55
    LD D,A
    LD D,B 
    LD D,C
    LD D,D 
    LD D,E
    LD D,H
    LD D,L
    LD D,(HL)
    LD D,0x55
    LD E,A
    LD E,B 
    LD E,C
    LD E,D 
    LD E,E
    LD E,H
    LD E,L
    LD E,(HL)
    LD E,0x55
    LD H,A
    LD H,B 
    LD H,C
    LD H,D 
    LD H,E
    LD H,H
    LD H,L
    LD H,(HL)
    LD H,0x55
    LD L,A
    LD L,B 
    LD L,C
    LD L,D 
    LD L,E
    LD L,H
    LD L,L
    LD L,(HL)
    LD L,0x55
    JP NZ,LOOP
    AND A 
    AND B
    AND C 
    AND D 
    AND E
    AND (HL)
    AND 0x12
    OR A 
    OR B
    OR C 
    OR D 
    OR E
    OR (HL)
    OR 0x12
    XOR A 
    XOR B
    XOR C 
    XOR D 
    XOR E
    XOR (HL)
    XOR 0x12
    CP A 
    CP B
    CP C 
    CP D 
    CP E
    CP (HL)
    CP 0x12
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
    HALT 

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
;asdf
ABCD1:
    LD A,(LOOP)
    LD A,LOOP
    JR NZ,0X0119
    JR NZ,ABCD1
    JR Z,0x0119
    JR Z,ABCD1
    JR NC,0x0119
    JR NC,ABCD1 
    JR C,0x0119
    JR C,ABCD1
    JR 0x0119
    JR ABCD1

