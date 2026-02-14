; test
ORG 0
    LD A,1
LOOP:
    LD HL,100h
    LD A,(HL)
    JP LOOP
    HALT