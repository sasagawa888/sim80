;test sim80
        LD   A,0xFF
LOOP:
        DEC  A
        RST 0x28
        JP   NZ,LOOP
        HALT
;end