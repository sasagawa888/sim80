#include <stdio.h>
#include <stdint.h>

unsigned char ram[0x10000];
unsigned char  A,Z,C;
uint16_t HL,BC,DE,PC,SP;

#define HI(x) ((uint8_t)((x) >> 8))
#define LO(x) ((uint8_t)((x) & 0xFF))

#define SET_HI(x,v) (x = ((x) & 0x00FF) | ((v) << 8))
#define SET_LO(x,v) (x = ((x) & 0xFF00) | (v))

int main(int argc, char *argv[])
{
    if (argc < 2) {
	printf("usage: sim80 file.bin\n");
	return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
	printf("cannot open file: %s\n", argv[1]);
	return 1;
    }

    size_t size = fread(ram, 1, 0x10000, fp);
    fclose(fp);

    printf("loaded %zu bytes\n", size);

    PC = 0;
    while (1) {
	switch (ram[PC++]) {
    case 0x00:      //NOP
        break;
    case 0x3c:      //INC A
        A++;
        if(A==0) Z = 1;
        break;
    case 0x3d:      //DEC A
        A--;
        if(A==0) Z=1;
        break;
	case 0x3e:		//LD A,nn
	    A = ram[PC++];
	    break;
	case 0x76:		//HALT
        printf("halt %02X at %04X\n", ram[PC-1], PC-1);
        printf("A=%X BC=%X DE=%X HL=%X PC=%X SP=%X\n",A,BC,DE,HL,PC,SP);
	    return 0;
    case 0xc2:      //JP NZ nn
        uint8_t lo = ram[PC++];
        uint8_t hi = ram[PC++];
        uint16_t nn = (uint16_t)lo | ((uint16_t)hi << 8);
        if (Z == 0) PC = nn;
        break;
    case 0xEF:      //RST 0x00
        printf("%02X",A);
        break;
    default:
        printf("unknown opcode %02X at %04X\n", ram[PC-1], PC-1);
        return 1;
    }
    }
    return 0;
}
