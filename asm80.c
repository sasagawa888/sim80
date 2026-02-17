#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>
#include "asm80.h"


FILE *input_stream;
FILE *output_stream;

token tok = { NUL, GO, OTHER, { 0 }
};

static Symbol labels[MAX_SYMS];
static int sym_count = 0;
unsigned char ram[0x10000];
unsigned short INDEX = 0;
int pass;
int lineno;
jmp_buf buf;


int main(int argc, char *argv[])
{
    if (argc < 2) {
	printf("usage: asm80 file.asm [out.bin]\n");
	return 1;
    }

    const char *infile = argv[1];
    char outfile[256];

    if (argc >= 3) {
	strncpy(outfile, argv[2], sizeof(outfile));
	outfile[sizeof(outfile) - 1] = '\0';
    } else {
	create_output_file(outfile, sizeof(outfile), infile);
    }

    input_stream = fopen(infile, "r");
    if (!input_stream) {
	printf("cannot open file: %s\n", infile);
	return 1;
    }
    // Initialize RAM 
    memset(ram, 0x00, sizeof(ram));
    INDEX = 0;

    int ret = setjmp(buf);

    if (ret == 0) {
	//pass1
	gen_label();
	//pass2
	input_stream = fopen(infile, "r");
	INDEX = 0;
	lineno = 0;
	gen_code();

	output_stream = fopen(outfile, "wb");
	if (!output_stream) {
	    printf("cannot open output: %s\n", outfile);
	    return 1;
	}
	// output binary to file
	fwrite(ram, 1, INDEX, output_stream);
	fclose(output_stream);

	printf("wrote %s (%u bytes)\n", outfile, (unsigned) INDEX);
	return 0;
    } else if (ret == 2) {
	//error
	fclose(input_stream);
	return 1;
    }
}

static int inttoken(char buf[]);
static int hextoken(char buf[]);
static int issymch(char c);
static int labeltoken(char buf[]);
static int symboltoken(char buf[]);
static void gen_ld(void);
static void gen_lda(void);
static void gen_ldb(void);
static void gen_ldc(void);
static void gen_ldd(void);
static void gen_lde(void);
static void gen_ldh(void);
static void gen_ldl(void);
static void gen_call(void);
static void gen_ret(void);
static void gen_jp(void);
static void gen_jr(void);
static void gen_inc(void);
static void gen_dec(void);
static void gen_and(void);
static void gen_or(void);
static void gen_xor(void);
static void gen_cp(void);
static void gen_add(void);
static void gen_adc(void);
static void gen_sub(void);
static void gen_sbc(void);
static void gen_push(void);
static void gen_pop(void);
static void gen_rst(void);
static void gen_code1(char *op);
static void gen_op1(unsigned int v, char *op);
static void gettoken(void);
static void emit8(unsigned int v);
static void emit16(unsigned int v);
static void error(char *ope, char *msg);
static int sym_find(const char *name);
static int sym_define(char *label, uint16_t addr);
static int eqv(char *x, char *y);


static void gettoken(void)
{
    char c;
    int pos;

    if (tok.flag == BACK) {
	tok.flag = GO;
	return;
    }

    if (tok.ch == ')') {
	tok.type = RPAREN;
	tok.buf[0] = NUL; 
	tok.ch = NUL;
	return;
    }

    if (tok.ch == '(') {
	tok.type = LPAREN;
	tok.buf[0] = NUL; 
	tok.ch = NUL;
	return;
    }

    if (tok.ch == ',') {
	tok.type = COMMA;
	tok.buf[0] = NUL; 
	tok.ch = NUL;
	return;
    }


    if (tok.ch == '.') {
	tok.type = DOT;
	tok.ch = NUL;
	return;
    }


  skip:
    c = fgetc(input_stream);
    while ((c == SPACE) || (c == EOL) || (c == TAB)) {
		if (c == EOL)
			lineno++;
	c = fgetc(input_stream);
    }
    

    if (c == ';') {
	while (c != EOL && c != EOF)
	    c = fgetc(input_stream);
	if (c == EOF) {
	    tok.type = FILEEND;
	    return;
	}
	if(c == EOL)
		lineno++;
	goto skip;
    }
	

    switch (c) {
    case '(':
	tok.type = LPAREN;
	break;
    case ')':
	tok.type = RPAREN;
	break;
    case ',':
	tok.type = COMMA;
	break;
    case '.':
	tok.type = DOT;
	break;
    case EOF:
	tok.type = FILEEND;
	return;
    default:{
	    pos = 0;
	    tok.buf[pos++] = c;
	    while (((c = fgetc(input_stream)) != EOL)
		   && (pos < BUFSIZE - 1) && (c != SPACE) && (c != '(')
		   && (c != ')') && (c != ',') && (c != '.') && (c != EOL)) {
		tok.buf[pos++] = c;
	    }
	    if (c == EOL)
		lineno++;

	    tok.buf[pos] = NUL;
	    tok.ch = c;
	    if (inttoken(tok.buf)) {
		tok.type = INTEGER;
		break;
	    }
	    if (hextoken(tok.buf)) {
		tok.type = HEXNUM;
		break;
	    }
	    if (labeltoken(tok.buf)) {
		int i;
		for (i = 0; tok.buf[i]; i++)
		    tok.buf[i] = toupper((unsigned char) tok.buf[i]);
		tok.type = LABEL;
		break;
	    }
	    if (symboltoken(tok.buf)) {
		int i;
		for (i = 0; tok.buf[i]; i++)
		    tok.buf[i] = toupper((unsigned char) tok.buf[i]);
		tok.type = SYMBOL;
		break;
	    }
	    tok.type = OTHER;
	}
    }
}



static int inttoken(char buf[])
{
    int i;
    char c;

    i = 0;			// {1234...}
    while ((c = buf[i]) != NUL) {
	if (isdigit(c))
	    i++;
	else
	    return (0);
    }
    return (1);
}


static int hextoken(char buf[])
{
    int i;
    char c;

    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
	if (buf[2] == NUL)	// 0x is not hexnum
	    return (0);

	i = 2;
	while ((c = buf[i]) != NUL) {
	    if (isxdigit(c))
		i++;
	    else
		return (0);
	}
	return (1);
    }
    return (0);
}

static int issymch(char c)
{
    switch (c) {
    case '!':
    case '?':
    case '+':
    case '-':
    case '*':
    case '/':
    case '=':
    case '<':
    case '>':
	return (1);
    default:
	return (0);
    }
}


static int symboltoken(char buf[])
{
    int i;
    char c;


    i = 0;
    while ((c = buf[i]) != NUL)
	if ((isalpha(c)) || (isdigit(c)) || (issymch(c)))
	    i++;
	else
	    return (0);

    return (1);
}


static int labeltoken(char buf[])
{
    int len = strlen(buf);
    if (len <= 1)
	return 0;

    if (buf[len - 1] != ':')
	return 0;

    buf[len - 1] = NUL;
    if (!symboltoken(buf))
	return 0;

    if (strlen(buf) > 8)
	error("Error label length is max 8 chars", buf);

    return 1;

}



static void create_output_file(char *out, size_t outsz, const char *in)
{
    const char *dot = strrchr(in, '.');
    if (dot && dot != in) {
	size_t base = (size_t) (dot - in);
	snprintf(out, outsz, "%.*s.bin", (int) base, in);
    } else {
	snprintf(out, outsz, "%s.bin", in);
    }
}


static void error(char *ope, char *msg)
{
    printf("Error: %s %s in %d\n", ope, msg, lineno);
    longjmp(buf, 1);
}

static int sym_find(const char *name)
{
    for (int i = 0; i < sym_count; i++) {
	if (strcmp(labels[i].name, name) == 0)
	    return i;
    }
    return -1;
}

static int sym_define(char *label, uint16_t addr)
{

    size_t n = strlen(label);
    if (n == 0 || n > MAX_LABEL)
	error("sym_define", "too long or null label");

    int idx = sym_find(label);
    if (idx >= 0)
	error("sym_define", "duplicate label");

    if (sym_count >= MAX_SYMS)
	error("sym_define", "too many label");

    strcpy(labels[sym_count].name, label);
    labels[sym_count].addr = addr;
    return sym_count++;
}


static void gen_label(void)
{
    pass = 1;
    while (1) {
	gettoken();
	if (tok.type == FILEEND) {
	    fclose(input_stream);
	    return;
	}
	if (tok.type == LABEL) {
	    sym_define(tok.buf, INDEX);
	}
	gen_code1(tok.buf);
    }
}

static void gen_code(void)
{
    pass = 2;
    while (1) {
	gettoken();
	if (tok.type == FILEEND) {
	    fclose(input_stream);
	    return;
	}
	gen_code1(tok.buf);
    }
}



static void gen_code1(char *op)
{
    if (eqv(op, "HALT")) {
	gen_op1(0x76, op);
    } else if (eqv(op, "NOP")) {
	gen_op1(0x00, op);
    } else if (eqv(op, "JP")) {
	gen_jp();
    } else if (eqv(op, "JR")) {
	gen_jr();
    } else if (eqv(op, "LD")) {
	gen_ld();
    } else if (eqv(op, "INC")) {
	gen_inc();
    } else if (eqv(op, "DEC")) {
	gen_dec();
    } else if (eqv(op, "AND")) {
	gen_and();
    } else if (eqv(op, "OR")) {
	gen_or();
    } else if (eqv(op, "XOR")) {
	gen_xor();
    } else if (eqv(op, "CP")) {
	gen_cp();
    } else if (eqv(op, "ADD")) {
	gen_add();
    } else if (eqv(op, "ADC")) {
	gen_adc();
    } else if (eqv(op, "SUB")) {
	gen_sub();
    } else if (eqv(op, "SBC")) {
	gen_sbc();
    } else if (eqv(op, "CALL")) {
	gen_call();
    } else if (eqv(op, "RET")) {
	gen_ret();
    } else if (eqv(op, "PUSH")) {
	gen_push();
    } else if (eqv(op, "POP")) {
	gen_pop();
    } else if (eqv(op, "RST")) {
	gen_rst();
    } else if (tok.type == LABEL) {
	if (pass == 2) {
	    printf("%04X  ", INDEX);
	    printf("%s:\n", op);
	}
	return;
    }
}

// 1 bytes operation
static void gen_op1(unsigned int v, char *op)
{
    if (pass == 2)
	printf("%04X  ", INDEX);
    emit8(v);
    if (pass == 2)
	printf("\t%s\n", op);
}


// 2 bytes operation
static void gen_op2(unsigned int v, unsigned int arg, char *op)
{
    if (pass == 2)
	printf("%04X  ", INDEX);
    emit8(v);
    emit8(arg);
    if (pass == 2)
	printf("\t%s\n", op);
}


// 3 bytes operation
static void gen_op3(unsigned int v, unsigned int arg, char *op)
{
    if (pass == 2)
	printf("%04X  ", INDEX);
    emit8(v);
    emit16(arg);
    if (pass == 2)
	printf("\t%s\n", op);
}

// LD groupe
static void gen_ld(void)
{
    gettoken();
    if (eqv(tok.buf, "A")) {
	gen_lda();
    } else if (eqv(tok.buf, "B")) {
	gen_ldb();
    } else if (eqv(tok.buf, "C")) {
	gen_ldc();
    } else if (eqv(tok.buf, "D")) {
	gen_ldd();
    } else if (eqv(tok.buf, "E")) {
	gen_lde();
    } else if (eqv(tok.buf, "H")) {
	gen_ldh();
    } else if (eqv(tok.buf, "L")) {
	gen_ldl();
    } 
	else
	error("LD operand ", tok.buf);
}

// LD A,~
static void gen_lda(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x7f, "LD A,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x78, "LD A,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x79, "LD A,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x7A, "LD A,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x7B, "LD A,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x7C, "LD A,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x7D, "LD A,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD A,");
	    strcat(str, tok.buf);
	    gen_op2(0x3e, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x7e, "LD A,(HL)");
	} else if (eqv(tok.buf, "BC")) {
	    gen_op1(0x0A, "LD A,(BC)");
	} else if (eqv(tok.buf, "DE")) {
	    gen_op1(0x1A, "LD A,(DE)");
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    arg = strtol(tok.buf, NULL, 0);
	    strcpy(str, "LD A,(");
	    strcat(str, tok.buf);
	    strcat(str, ")");
	    gen_op3(0x3a, arg, str);
	} else if (tok.type == SYMBOL) {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0) {
		    error("undefined symbol", tok.buf);
		}
		arg = labels[idx].addr;
	    }
	    strcpy(str, "LD A,(");
	    strcat(str, tok.buf);
	    strcat(str, ")");
	    gen_op3(0x3a, arg, str);
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
}

// LD B,~
static void gen_ldb(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x47, "LD B,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x40, "LD B,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x41, "LD B,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x42, "LD B,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x43, "LD B,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x44, "LD B,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x45, "LD B,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD B,");
	    strcat(str, tok.buf);
	    gen_op2(0x06, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x46, "LD B,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}

// LD C,~
static void gen_ldc(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x4F, "LD C,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x48, "LD C,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x49, "LD C,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x4A, "LD C,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x4B, "LD C,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x4C, "LD C,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x4D, "LD C,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD C,");
	    strcat(str, tok.buf);
	    gen_op2(0x0E, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x4E, "LD C,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}


// LD D,~
static void gen_ldd(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x57, "LD D,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x50, "LD D,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x51, "LD D,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x52, "LD D,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x53, "LD D,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x54, "LD D,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x55, "LD D,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD D,");
	    strcat(str, tok.buf);
	    gen_op2(0x16, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x56, "LD D,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}


// LD E,~
static void gen_lde(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x5F, "LD E,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x58, "LD E,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x59, "LD E,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x5A, "LD E,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x5B, "LD E,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x5C, "LD E,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x5D, "LD E,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD E,");
	    strcat(str, tok.buf);
	    gen_op2(0x1E, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x5E, "LD E,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}


// LD H,~
static void gen_ldh(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x67, "LD H,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x60, "LD H,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x61, "LD H,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x62, "LD H,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x63, "LD H,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x64, "LD H,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x65, "LD H,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD H,");
	    strcat(str, tok.buf);
	    gen_op2(0x26, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x66, "LD H,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}


// LD L,~
static void gen_ldl(void)
{
    char str[64];
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    int arg, idx;
    arg = 0;
    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x6F, "LD L,A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x68, "LD L,B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x69, "LD L,C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x6A, "LD L,D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x6B, "LD L,E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x6C, "LD L,H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x6D, "LD L,L");
	    return;
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
	    }
	  imediate:
	    strcpy(str, "LD L,");
	    strcat(str, tok.buf);
	    gen_op2(0x2E, arg, str);
	    return;
	}
    case INTEGER:
    case HEXNUM:
	arg = strtol(tok.buf, NULL, 0);
	goto imediate;
    case LPAREN:		// e.g. (HL)
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x66, "LD L,(HL)");
	
	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect expected )", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
    }
	}
}


// JP groupe
static void gen_jp(void)
{
    int arg, idx;
    char str[128];
    arg = 0;
    gettoken();			// flag or label
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "NZ")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP NZ,");
		strcat(str, tok.buf);
		gen_op3(0xc2, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP NZ,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xc2, arg, str);
	    }
	} else if (eqv(tok.buf, "Z")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP Z,");
		strcat(str, tok.buf);
		gen_op3(0xca, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP Z,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xca, arg, str);
	    }
	} else if (eqv(tok.buf, "NC")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP NC,");
		strcat(str, tok.buf);
		gen_op3(0xd2, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP NC,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xd2, arg, str);
	    }
	} else if (eqv(tok.buf, "C")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP C,");
		strcat(str, tok.buf);
		gen_op3(0xda, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP C,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xda, arg, str);
	    }
	} else if (eqv(tok.buf, "PO")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP PO,");
		strcat(str, tok.buf);
		gen_op3(0xe2, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP PO,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xe2, arg, str);
	    }
	} else if (eqv(tok.buf, "PE")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP PE,");
		strcat(str, tok.buf);
		gen_op3(0xea, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP PE,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xea, arg, str);
	    }
	} else if (eqv(tok.buf, "P")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP P,");
		strcat(str, tok.buf);
		gen_op3(0xf2, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP P,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xf2, arg, str);
	    }
	} else if (eqv(tok.buf, "M")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JP M,");
		strcat(str, tok.buf);
		gen_op3(0xfa, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JP M,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xfa, arg, str);
	    }
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
		strcpy(str, "JP ");
		strcat(str, tok.buf);
	    }
	    gen_op3(0xc3, arg, str);
	}
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "JP ");
	strcat(str, tok.buf);
	gen_op3(0xc3, arg, str);
    } else
	error("JP opetation", tok.buf);
}


// JP groupe
static void gen_jr(void)
{
    int arg, idx;
    char str[128];
    arg = 0;
    gettoken();			// flag or label
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "NZ")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR NZ,");
		strcat(str, tok.buf);
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x20, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR NZ,");
		    strcat(str, tok.buf);
		}
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x20, rel, str);
	    }
	} else if (eqv(tok.buf, "Z")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR Z,");
		strcat(str, tok.buf);
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x28, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR Z,");
		    strcat(str, tok.buf);
		}
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x28, rel, str);
	    }
	} else if (eqv(tok.buf, "NC")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR NC,");
		strcat(str, tok.buf);
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x30, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR NC,");
		    strcat(str, tok.buf);
		}
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x30, rel, str);
	    }
	} else if (eqv(tok.buf, "C")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR C,");
		strcat(str, tok.buf);
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x38, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR C,");
		    strcat(str, tok.buf);
		}
		int offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		unsigned char rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x38, rel, str);
	    }
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
		strcpy(str, "JR ");
		strcat(str, tok.buf);
	    }
	    int offset = arg - (INDEX + 2);
	    if (offset < -128 || offset > 127) {
		error("JR operation", tok.buf);
	    }
	    unsigned char rel = (unsigned char) (offset & 0xFF);
	    gen_op2(0x18, rel, str);
	}
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "JR ");
	strcat(str, tok.buf);
	int offset = arg - (INDEX + 2);
	if (offset < -128 || offset > 127) {
	    error("JR operation", tok.buf);
	}
	unsigned char rel = (unsigned char) (offset & 0xFF);
	gen_op2(0x18, rel, str);
    } else
	error("JR opetation", tok.buf);
}



// DEC groupe
static void gen_dec(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x3d, "DEC A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x05, "DEC B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x0d, "DEC C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x15, "DEC D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x1d, "DEC E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x25, "DEC H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x2d, "DEC L");
	}
    }
}


// INC groupe
static void gen_inc(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x3c, "INC A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x04, "INC B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x0c, "INC C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x14, "INC D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x1c, "INC E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x24, "INC H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x2c, "INC L");
	}
    }
}


// AND groupe
static void gen_and(void)
{
    char str[128];
    int arg;
    arg = 0;
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0xA7, "AND A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0xA0, "AND B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0xA1, "AND C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0xA2, "AND D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0xA3, "AND E");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL"))
	    gen_op1(0xA6, "AND (HL)");
	gettoken();		// )
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "AND ");
	strcat(str, tok.buf);
	gen_op2(0xE6, arg, str);
    }
}


// OR groupe
static void gen_or(void)
{
    char str[128];
    int arg;
    arg = 0;
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0xB7, "OR A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0xB0, "OR B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0xB1, "OR C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0xB2, "OR D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0xB3, "OR E");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL"))
	    gen_op1(0xB6, "OR (HL)");
	gettoken();		// )
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "OR ");
	strcat(str, tok.buf);
	gen_op2(0xF6, arg, str);
    }
}


// XOR groupe
static void gen_xor(void)
{
    char str[128];
    int arg;
    arg = 0;
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0xAF, "XOR A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0xA8, "XOR B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0xA9, "XOR C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0xAA, "XOR D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0xAB, "XOR E");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL"))
	    gen_op1(0xAE, "XOR (HL)");
	gettoken();		// )
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "XOR ");
	strcat(str, tok.buf);
	gen_op2(0xEE, arg, str);
    }
}


// CP groupe
static void gen_cp(void)
{
    char str[128];
    int arg;
    arg = 0;
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0xBF, "CP A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0xB8, "CP B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0xB9, "CP C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0xBA, "CP D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0xBB, "CP E");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL"))
	    gen_op1(0xBE, "CP (HL)");
	gettoken();		// )
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "CP ");
	strcat(str, tok.buf);
	gen_op2(0xFE, arg, str);
    }
}


// ADD groupe
static void gen_add(void)
{
    char str[128];

    gettoken();
    if (eqv(tok.buf, "A")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("ADD operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "A")) {
		gen_op1(0x87, "ADD A,A");
	    } else if (eqv(tok.buf, "B")) {
		gen_op1(0x80, "ADD A,B");
	    } else if (eqv(tok.buf, "C")) {
		gen_op1(0x81, "ADD A,C");
	    } else if (eqv(tok.buf, "D")) {
		gen_op1(0x82, "ADD A,D");
	    } else if (eqv(tok.buf, "E")) {
		gen_op1(0x83, "ADD A,E");
	    } else if (eqv(tok.buf, "H")) {
		gen_op1(0x84, "ADD A,H");
	    } else if (eqv(tok.buf, "L")) {
		gen_op1(0x85, "ADD A,L");
	    }
	} else if (tok.type == LPAREN) {
	    gettoken();
	    if (eqv(tok.buf, "HL")) {
		gen_op1(0x86, "ADD A,(HL)");
	    }
	    gettoken();		// )
	    if (tok.type != RPAREN)
		error("ADD operation expected right paren", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    int arg;
	    arg = 0;
	    arg = strtol(tok.buf, NULL, 0);
	    strcpy(str, "ADD A,");
	    strcat(str, tok.buf);
	    gen_op2(0xc6, arg, str);
	}
    }
}


// ADC groupe
static void gen_adc(void)
{
    char str[128];

    gettoken();
    if (eqv(tok.buf, "A")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("ADC operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "A")) {
		gen_op1(0x8F, "ADC A,A");
	    } else if (eqv(tok.buf, "B")) {
		gen_op1(0x88, "ADC A,B");
	    } else if (eqv(tok.buf, "C")) {
		gen_op1(0x89, "ADC A,C");
	    } else if (eqv(tok.buf, "D")) {
		gen_op1(0x8A, "ADC A,D");
	    } else if (eqv(tok.buf, "E")) {
		gen_op1(0x8B, "ADC A,E");
	    } else if (eqv(tok.buf, "H")) {
		gen_op1(0x8C, "ADC A,H");
	    } else if (eqv(tok.buf, "L")) {
		gen_op1(0x8D, "ADC A,L");
	    }
	} else if (tok.type == LPAREN) {
	    gettoken();
	    if (eqv(tok.buf, "HL")) {
		gen_op1(0x8E, "ADC A,(HL)");
	    }
	    gettoken();		// )
	    if (tok.type != RPAREN)
		error("ADD operation expected right paren", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    int arg;
	    arg = 0;
	    arg = strtol(tok.buf, NULL, 0);
	    strcpy(str, "ADC A,");
	    strcat(str, tok.buf);
	    gen_op2(0xcE, arg, str);
	}
    }
}


// SUB groupe
static void gen_sub(void)
{
    char str[128];

    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x97, "SUB A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x90, "SUB B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x91, "SUB C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x92, "SUB D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x93, "SUB E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x94, "SUB H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x95, "SUB L");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x96, "SUB (HL)");
	}
	gettoken();		// )
	if (tok.type != RPAREN)
	    error("SUB operation expected right paren", tok.buf);
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	int arg;
	arg = 0;
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "SUB ");
	strcat(str, tok.buf);
	gen_op2(0xd6, arg, str);
    }

}


// SBC groupe
static void gen_sbc(void)
{
    char str[128];

    gettoken();
    if (eqv(tok.buf, "A")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("SBC operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "A")) {
		gen_op1(0x9F, "SBC A,A");
	    } else if (eqv(tok.buf, "B")) {
		gen_op1(0x98, "SBC A,B");
	    } else if (eqv(tok.buf, "C")) {
		gen_op1(0x99, "SBC A,C");
	    } else if (eqv(tok.buf, "D")) {
		gen_op1(0x9A, "SBC A,D");
	    } else if (eqv(tok.buf, "E")) {
		gen_op1(0x9B, "SBC A,E");
	    } else if (eqv(tok.buf, "H")) {
		gen_op1(0x9C, "SBC A,H");
	    } else if (eqv(tok.buf, "L")) {
		gen_op1(0x9D, "SBC A,L");
	    }
	} else if (tok.type == LPAREN) {
	    gettoken();
	    if (eqv(tok.buf, "HL")) {
		gen_op1(0x9E, "SBC A,(HL)");
	    }
	    gettoken();		// )
	    if (tok.type != RPAREN)
		error("SBC operation expected right paren", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    int arg;
	    arg = 0;
	    arg = strtol(tok.buf, NULL, 0);
	    strcpy(str, "SBC A,");
	    strcat(str, tok.buf);
	    gen_op2(0xDE, arg, str);
	}
    }
}


// PUSH groupe
static void gen_push(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "BC")) {
	    gen_op1(0xc5, "PUSH BC");
	} else if (eqv(tok.buf, "DE")) {
	    gen_op1(0xd5, "PUSH DE");
	} else if (eqv(tok.buf, "HL")) {
	    gen_op1(0xe5, "PUSH HL");
	} else if (eqv(tok.buf, "AF")) {
	    gen_op1(0xf5, "PUSH AF");
	} else
	    error("PUSH operation", tok.buf);
    } else
	error("PUSH operation", tok.buf);
}

// POP groupe
static void gen_pop(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "BC")) {
	    gen_op1(0xc1, "POP BC");
	} else if (eqv(tok.buf, "DE")) {
	    gen_op1(0xd1, "POP DE");
	} else if (eqv(tok.buf, "HL")) {
	    gen_op1(0xe1, "POP HL");
	} else if (eqv(tok.buf, "AF")) {
	    gen_op1(0xf1, "POP AF");
	} else
	    error("POP operation", tok.buf);
    } else
	error("POP operation", tok.buf);
}

// RET groupe
static void gen_ret(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "NZ")) {
	    gen_op1(0xc0, "RET NZ");
	} else if (eqv(tok.buf, "Z")) {
	    gen_op1(0xc8, "RET Z");
	} else if (eqv(tok.buf, "NC")) {
	    gen_op1(0xd0, "RET NC");
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0xd8, "RET C");
	} else {
	    tok.flag = BACK;
	    gen_op1(0xc9, "RET");
	}

    } else {
	if (tok.type == LABEL) {
	    tok.flag = BACK;
	    gen_op1(0xc9, "RET");
	} else
	    error("RET opetation", tok.buf);
    }
}


// CALL groupe
static void gen_call(void)
{
    int arg, idx;
    char str[128];
    arg = 0;
    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "NZ")) {
	    gettoken();		//comma
	    gettoken();		// nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL NZ,");
		strcat(str, tok.buf);
		gen_op3(0xc4, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL NZ,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xc4, arg, str);
	    }
	} else if (eqv(tok.buf, "Z")) {
	    gettoken();		//comma
	    gettoken();		//nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL Z,");
		strcat(str, tok.buf);
		gen_op3(0xcc, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL Z,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xcc, arg, str);
	    }
	} else if (eqv(tok.buf, "NC")) {
	    gettoken();		//comma
	    gettoken();		//nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL NC,");
		strcat(str, tok.buf);
		gen_op3(0xd4, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL NC,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xD4, arg, str);
	    }
	} else if (eqv(tok.buf, "C")) {
	    gettoken();		//comma
	    gettoken();		//nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL C,");
		strcat(str, tok.buf);
		gen_op3(0xdc, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL C,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xdc, arg, str);
	    }
	} else if (eqv(tok.buf, "PO")) {
	    gettoken();		//comma
	    gettoken();		//label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL PO,");
		strcat(str, tok.buf);
		gen_op3(0xe4, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL PO,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xe4, arg, str);
	    }
	} else if (eqv(tok.buf, "PE")) {
	    gettoken();		//comma
	    gettoken();		//nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL PE,");
		strcat(str, tok.buf);
		gen_op3(0xec, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL PE,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xec, arg, str);
	    }
	} else if (eqv(tok.buf, "P")) {
	    gettoken();		//comma
	    gettoken();		// nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL P,");
		strcat(str, tok.buf);
		gen_op3(0xf4, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL P,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xf4, arg, str);
	    }
	} else if (eqv(tok.buf, "M")) {
	    gettoken();		//comma
	    gettoken();		// nn or label
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "CALL M,");
		strcat(str, tok.buf);
		gen_op3(0xfc, arg, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "CALL M,");
		    strcat(str, tok.buf);
		}
		gen_op3(0xfc, arg, str);
	    }
	} else {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0)
		    error("undefined symbol", tok.buf);
		arg = labels[idx].addr;
		strcpy(str, "CALL ");
		strcat(str, tok.buf);
	    }
	    gen_op3(0xcd, arg, str);
	}
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "CALL ");
	strcat(str, tok.buf);
	gen_op3(0xcd, arg, str);
    } else
	error("CALL opetation", tok.buf);
}

static void gen_rst(void)
{
    int arg;
    gettoken();
    if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	switch (arg) {
	case 0x00:
	    gen_op1(0xc7, "RST 0x00");
	    break;
	case 0x08:
	    gen_op1(0xcf, "RST 0x08");
	    break;
	case 0x10:
	    gen_op1(0xd7, "RST 0x10");
	    break;
	case 0x18:
	    gen_op1(0xdf, "RST 0x18");
	    break;
	case 0x20:
	    gen_op1(0xe7, "RST 0x20");
	    break;
	case 0x28:
	    gen_op1(0xef, "RST 0x28");
	    break;
	case 0x30:
	    gen_op1(0xf7, "RST 0x30");
	    break;
	case 0x38:
	    gen_op1(0xff, "RST 0x38");
	    break;
	default:
	    error("RST operation", tok.buf);
	}
    } else
	error("RST operation", tok.buf);

}


static void emit8(unsigned int v)
{
    if (pass == 2) {
	printf("%02X ", v);
	ram[INDEX++] = (unsigned char) (v & 0xFF);
    } else
	INDEX++;
}


static void emit16(unsigned int v)
{
    // Z80　0x1234 -> 0x34 0x12
    emit8(v & 0xFF);
    emit8((v >> 8) & 0xFF);
}

static int eqv(char *x, char *y)
{
    if (strcmp(x, y) == 0)
	return (1);
    else
	return (0);
}
