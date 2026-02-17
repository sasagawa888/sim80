;test sim80
        LD   A,0xFF
        JP LOAD
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
        LD A,B 
        LD A,C
        RST 0x28
        HALT

;end