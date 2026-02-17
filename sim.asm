;test sim80
        LD   A,0xFF
        JP LOOP1
LOOP:
        DEC  A
        RST 0x28
        JP   NZ,LOOP
        HALT

LOOP1:
        DEC  A
        RST 0x28
        JR   NZ,LOOP1
        HALT

LOAD:
        LD B,0x12
        LD A,B 
        ADD A,B
        RST 0x28
        HALT

;end