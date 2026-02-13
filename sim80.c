#include <stdio.h>

unsigned char ram[0x10000];
unsigned char PC;
unsigned char  A, B, C, D, E, H, L;


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
	case 0x3e:		//LD A,nn
	    A = ram[PC++];
	    break;
	case 0x76:		//HALT
        printf("halt %02X at %04X\n", ram[PC-1], PC-1);
	    return 0;
    default:
        printf("unknown opcode %02X at %04X\n", ram[PC-1], PC-1);
        return 1;
    }
    }
    return 0;
}
