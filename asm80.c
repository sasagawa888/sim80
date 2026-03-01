#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdint.h>
#include "asm80.h"

#define version 0.9

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
int include_flag = 0;
jmp_buf buf;

int main(int argc, char *argv[])
{
    printf("asm80 ver %01g\n", version);

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
static void gen_ldhl(void);
static void gen_ldix(void);
static void gen_ldiy(void);
static void gen_ldsp(void);
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
static void gen_bit(void);
static void gen_set(void);
static void gen_res(void);
static void gen_sla(void);
static void gen_sra(void);
static void gen_srl(void);
static void gen_rrc(void);
static void gen_rlc(void);
static void gen_rl(void);
static void gen_rr(void);
static void gen_code1(char *op);
static void gen_op1(unsigned int v, char *op);
static void gen_idx(unsigned char prefix, unsigned char opcode,
		    const char *mnem_rhs_fmt);
static void gen_idx1(unsigned char prefix, unsigned char opcode,
		     unsigned char opcode1, const char *mnem_rhs_fmt);
static void gettoken(void);
static void emit8(unsigned int v);
static void emit16(unsigned int v);
static void error(char *ope, char *msg);
static int sym_find(const char *name);
static int sym_define(char *label, uint16_t addr);
static int eqv(char *x, char *y);
static void gen_include(char* include);


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

    if (tok.ch == '+') {
	tok.type = PLUS;
	tok.ch = NUL;
	return;
    }

    if (tok.ch == '-') {
	tok.type = MINUS;
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
	if (c == EOL)
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
    //case '.':
	//tok.type = DOT;
	//break;
    case '+':
	tok.type = PLUS;
	break;
    case '-':
	tok.type = MINUS;
	break;
    case EOF:
	tok.type = FILEEND;
	return;
    default:{
	    pos = 0;
	    tok.buf[pos++] = c;
	    while (((c = fgetc(input_stream)) != EOL)
		   && (pos < BUFSIZE - 1) && (c != SPACE) && (c != '(')
		   && (c != ')') && (c != ',') && (c != '+')
		   && (c != '-') && (c != EOL)) {
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
    char str[64];
    int arg;

    pass = 1;
    while (1) {
	gettoken();
	if (tok.type == FILEEND) {
	    fclose(input_stream);
	    return;
	}
	if (tok.type == LABEL) {
	    strcpy(str, tok.buf);
	    gettoken();
	    if (!eqv(tok.buf, "EQU")) {
		//normal label
		tok.flag = BACK;
		sym_define(str, INDEX);
	    } else {
		//EQU label
		gettoken();
		if (!(tok.type == INTEGER || tok.type == HEXNUM))
		    error("EQU operand ", tok.buf);
		arg = strtol(tok.buf, NULL, 0);
		sym_define(str, arg);
	    }
	} else {
	    gen_code1(tok.buf);
	}
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
    int arg;
    char str[128];

	if (eqv(op,"INCLUDE")){
	gettoken();
	gen_include(tok.buf);
	tok.type = 0;
	}else if (eqv(op, "DB")) {
	gettoken();
	if (!(tok.type == INTEGER || tok.type == HEXNUM))
	    error("DB operand ", tok.buf);
	arg = strtol(tok.buf, NULL, 0);
	sprintf(str, "DB %s", tok.buf);
	gen_op1(arg, str);
    } else if (eqv(op, "HALT")) {
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
    } else if (eqv(op, "BIT")) {
	gen_bit();
    } else if (eqv(op, "SET")) {
	gen_set();
    } else if (eqv(op, "RES")) {
	gen_res();
    } else if (eqv(op, "SLA")) {
	gen_sla();
    } else if (eqv(op, "SRA")) {
	gen_sra();
    } else if (eqv(op, "SRL")) {
	gen_srl();
    } else if (eqv(op, "RLC")) {
	gen_rlc();
    } else if (eqv(op, "RRC")) {
	gen_rrc();
    } else if (eqv(op, "RL")) {
	gen_rl();
    } else if (eqv(op, "RR")) {
	gen_rr();
    } else if (tok.type == LABEL) {
	if (pass == 2) {
	    strcpy(str, tok.buf);
	    gettoken();
	    if (!eqv(tok.buf, "EQU")) {
		// normal label
		tok.flag = BACK;
		printf("%04X  ", INDEX);
		printf("%s:\n", str);
	    } else {
		// EQU
		printf("%s", str);
		gettoken();
		printf("\t%s %s\n", "EQU", tok.buf);
	    }
	}
	return;
    } else
	error("undefined operation", tok.buf);
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

// extended 3 bytes operation
static void gen_op4(unsigned int v1, unsigned int v2, unsigned int arg,
		    char *op)
{
    if (pass == 2)
	printf("%04X  ", INDEX);
    emit8(v1);
    emit8(v2);
    emit8(arg);
    if (pass == 2)
	printf("\t%s\n", op);
}


// extended 4 bytes operation
static void gen_op5(unsigned int v1, unsigned int v2, unsigned int arg,
		    unsigned int v3, char *op)
{
    if (pass == 2)
	printf("%04X  ", INDEX);
    emit8(v1);
    emit8(v2);
    emit8(arg);
    emit8(v3);
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
    } else if (eqv(tok.buf, "SP")) {
	gen_ldsp();
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gettoken();		//)
	    if (tok.type != RPAREN)
		error("LD operation expected )", tok.buf);
	    gen_ldhl();
	} else if (eqv(tok.buf, "IX")) {
	    gen_ldix();
	} else if (eqv(tok.buf, "IY")) {
	    gen_ldiy();
	}


    } else
	error("LD operand ", tok.buf);
}

static int eval_imm_or_sym(int *out_value)
{
    if (tok.type == INTEGER || tok.type == HEXNUM) {
	char *endp = NULL;
	long v = strtol(tok.buf, &endp, 0);
	if (endp == tok.buf)
	    return 0;
	*out_value = (int) v;
	return 1;
    }
    if (tok.type == SYMBOL) {
	if (pass == 2) {
	    int si = sym_find(tok.buf);
	    if (si < 0)
		return 0;
	    *out_value = labels[si].addr;
	} else {
	    *out_value = 0;	// pass1 は仮
	}
	return 1;
    }
    return 0;
}

// 期待：現在 tok.buf は "IX" または "IY"（LPARENの直後に読んだ状態）
// この関数は +/-, nn を読み、disp8を作って gen_op4 まで行う。
// RPAREN は呼び出し元(gen_ldaなど)が読む設計のままにする。
static void gen_ldidx(unsigned char prefix, unsigned char opcode, const char *mnem_lhs,	// "A" とか "(...)"
		      const char *idxname,	// "IX" or "IY"
		      const char *mnem_rhs_fmt)	// 右側の書式: "(%s%c%s)" のように使う
{
    char nn[32], str[64];
    int sign = +1;
    int arg = 0;
    int signed_disp;
    unsigned char disp8;

    // + / -
    gettoken();
    if (tok.type == PLUS)
	sign = +1;
    else if (tok.type == MINUS)
	sign = -1;
    else
	error("expected + or -", tok.buf);

    // nn
    gettoken();
    strncpy(nn, tok.buf, sizeof(nn) - 1);
    nn[sizeof(nn) - 1] = '\0';

    if (!eval_imm_or_sym(&arg)) {
	error("bad displacement", tok.buf);
    }

    signed_disp = sign * arg;
    if (signed_disp < -128 || signed_disp > 127) {
	error("index displacement out of range", nn);
    }
    disp8 = (unsigned char) (signed_disp & 0xFF);

    // 文字列（例: "LD A,(IX+1)" / "LD (IY-0x10),A" など）
    // rhs 部分をフォーマットで作れるようにしておく
    char rhs[48];
    snprintf(rhs, sizeof(rhs), mnem_rhs_fmt, idxname,
	     (sign > 0 ? '+' : '-'), nn);

    snprintf(str, sizeof(str), "LD %s,%s", mnem_lhs, rhs);

    gen_op4(prefix, opcode, disp8, str);
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
	    gen_op1(0x7E, "LD A,(HL)");
	} else if (eqv(tok.buf, "BC")) {
	    gen_op1(0x0A, "LD A,(BC)");
	} else if (eqv(tok.buf, "DE")) {
	    gen_op1(0x1A, "LD A,(DE)");
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    arg = strtol(tok.buf, NULL, 0);
	    strcpy(str, "LD A,(");
	    strcat(str, tok.buf);
	    strcat(str, ")");
	    gen_op3(0x3A, arg, str);	// LD A,(nn)
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    if (pass == 2) {
		idx = sym_find(tok.buf);
		if (idx < 0) {
		    error("undefined symbol", tok.buf);
		}
		arg = labels[idx].addr;
	    } else {
		arg = 0;
	    }
	    strcpy(str, "LD A,(");
	    strcat(str, tok.buf);
	    strcat(str, ")");
	    gen_op3(0x3A, arg, str);	// LD A,(nn) where nn is label address
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD A,(IX+d) / LD A,(IY+d)  opcode = 0x7E (= LD A,(HL))
	    gen_ldidx(prefix, 0x7E, "A", idxname, "(%s%c%s)");
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
	} else if (eqv(tok.buf, "BC")) {
	    error("LD B,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD B,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD B,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD B,(label) not supported on Z80", tok.buf);
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD B,(IX+d)  opcode = 0x46 (= LD B,(HL))
	    gen_ldidx(prefix, 0x46, "B", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;
    default:
	error("LD operand", tok.buf);
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
    case LPAREN:
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0x4E, "LD C,(HL)");
	} else if (eqv(tok.buf, "BC")) {
	    error("LD C,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD C,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD C,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD C,(label) not supported on Z80", tok.buf);
	} else {
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    gen_ldidx(prefix, 0x4E, "C", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;

    default:
	error("LD operand", tok.buf);
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
	} else if (eqv(tok.buf, "BC")) {
	    error("LD D,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD D,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD D,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD D,(label) not supported on Z80", tok.buf);
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD D,(IX+d) / LD D,(IY+d)
	    // opcode = 0x56 (= LD D,(HL))
	    gen_ldidx(prefix, 0x56, "D", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;

    default:
	error("LD operand", tok.buf);
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
	} else if (eqv(tok.buf, "BC")) {
	    error("LD E,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD E,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD E,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD E,(label) not supported on Z80", tok.buf);
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD E,(IX+d) / LD E,(IY+d)
	    // opcode = 0x5E (= LD E,(HL))
	    gen_ldidx(prefix, 0x5E, "E", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;

    default:
	error("LD operand", tok.buf);
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
	} else if (eqv(tok.buf, "BC")) {
	    error("LD H,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD H,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD H,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD H,(label) not supported on Z80", tok.buf);
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD H,(IX+d) / LD H,(IY+d)
	    // opcode = 0x66 (= LD H,(HL))
	    gen_ldidx(prefix, 0x66, "H", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;

    default:
	error("LD operand", tok.buf);
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
	    gen_op1(0x6E, "LD L,(HL)");
	} else if (eqv(tok.buf, "BC")) {
	    error("LD L,(BC) not supported on Z80", tok.buf);
	} else if (eqv(tok.buf, "DE")) {
	    error("LD L,(DE) not supported on Z80", tok.buf);
	} else if (tok.type == INTEGER || tok.type == HEXNUM) {
	    error("LD L,(nn) not supported on Z80", tok.buf);
	} else if (tok.type == SYMBOL && !eqv(tok.buf, "IX")
		   && !eqv(tok.buf, "IY")) {
	    error("LD L,(label) not supported on Z80", tok.buf);
	} else {
	    // IX+nn IY+nn
	    unsigned char prefix;
	    const char *idxname;

	    if (eqv(tok.buf, "IX")) {
		prefix = 0xDD;
		idxname = "IX";
	    } else if (eqv(tok.buf, "IY")) {
		prefix = 0xFD;
		idxname = "IY";
	    } else {
		error("expected IX or IY", tok.buf);
		return;
	    }

	    // LD L,(IX+d) / LD L,(IY+d)
	    // opcode = 0x6E (= LD L,(HL))
	    gen_ldidx(prefix, 0x6E, "L", idxname, "(%s%c%s)");
	}

	gettoken();
	if (tok.type != RPAREN)
	    error("LD indirect", tok.buf);
	return;

    default:
	error("LD operand", tok.buf);
    }
}

// LD SP,~
static void gen_ldsp(void)
{
    gettoken();			//comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);
    gettoken();
    if (eqv(tok.buf, "HL")) {
	gen_op1(0xF9, "LD SP,HL");
	return;
    } else if (eqv(tok.buf, "IX")) {
	gen_op2(0xDD, 0xF9, "LD SP,IX");
	return;
    } else if (eqv(tok.buf, "IY")) {
	gen_op2(0xFD, 0xF9, "LD SP,IY");
	return;
    }
}


// LD (HL),~
static void gen_ldhl(void)
{

    gettoken();			// comma
    if (tok.type != COMMA)
	error("LD comma expected", tok.buf);

    gettoken();			// src operand

    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    gen_op1(0x77, "LD (HL),A");
	    return;
	} else if (eqv(tok.buf, "B")) {
	    gen_op1(0x70, "LD (HL),B");
	    return;
	} else if (eqv(tok.buf, "C")) {
	    gen_op1(0x71, "LD (HL),C");
	    return;
	} else if (eqv(tok.buf, "D")) {
	    gen_op1(0x72, "LD (HL),D");
	    return;
	} else if (eqv(tok.buf, "E")) {
	    gen_op1(0x73, "LD (HL),E");
	    return;
	} else if (eqv(tok.buf, "H")) {
	    gen_op1(0x74, "LD (HL),H");
	    return;
	} else if (eqv(tok.buf, "L")) {
	    gen_op1(0x75, "LD (HL),L");
	    return;
	} else if (eqv(tok.buf, "(HL)")) {
	    /* 0x76 は HALT。LD (HL),(HL) は存在しない(表ではこう見えても実体はHALT) */
	    error("LD (HL),(HL) is HALT (0x76)", tok.buf);
	} else {
	    error("LD operand", tok.buf);
	}
	break;

    default:
	error("LD operand", tok.buf);
    }
}

// LD (IX+nn),~
static void gen_ldix(void)
{
    int arg = 0;
    int idx;
    int sign = +1;		// +1 or -1
    int signed_disp;		// -128..127
    unsigned char disp8;
    char nn[64], str[256];

    gettoken();			// +/-
    if (tok.type == PLUS) {
	sign = +1;
    } else if (tok.type == MINUS) {
	sign = -1;
    } else {
	error("expected + or -", tok.buf);
    }

    gettoken();			// src operand (nn)
    strncpy(nn, tok.buf, sizeof(nn) - 1);
    nn[sizeof(nn) - 1] = '\0';

    if (tok.type == INTEGER || tok.type == HEXNUM) {
	char *endp = NULL;
	long v = strtol(tok.buf, &endp, 0);
	if (endp == tok.buf) {
	    error("bad number", tok.buf);
	}
	arg = (int) v;
    } else {
	if (pass == 2) {
	    idx = sym_find(tok.buf);
	    if (idx < 0)
		error("undefined symbol", tok.buf);
	    arg = labels[idx].addr;
	} else {
	    arg = 0;
	}
    }

    signed_disp = sign * arg;
    if (signed_disp < -128 || signed_disp > 127) {
	error("IX displacement out of range", nn);
    }
    disp8 = (unsigned char) (signed_disp & 0xFF);

    gettoken();			// )
    gettoken();			// ,
    gettoken();			// Register

    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),A",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x77, disp8, str);
	    return;
	} else if (eqv(tok.buf, "B")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),B",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x70, disp8, str);
	    return;
	} else if (eqv(tok.buf, "C")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),C",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x71, disp8, str);
	    return;
	} else if (eqv(tok.buf, "D")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),D",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x72, disp8, str);
	    return;
	} else if (eqv(tok.buf, "E")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),E",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x73, disp8, str);
	    return;
	} else if (eqv(tok.buf, "H")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),H",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x74, disp8, str);
	    return;
	} else if (eqv(tok.buf, "L")) {
	    snprintf(str, sizeof(str), "LD (IX%c%s),L",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xDD, 0x75, disp8, str);
	    return;
	} else {
	    error("LD operand", tok.buf);
	}
	break;

    default:
	error("LD operand", tok.buf);
    }
}

// LD (IY+nn),~
static void gen_ldiy(void)
{
    int arg = 0;
    int idx;
    int sign = +1;
    int signed_disp;
    unsigned char disp8;
    char nn[64], str[256];

    gettoken();			// +/-
    if (tok.type == PLUS) {
	sign = +1;
    } else if (tok.type == MINUS) {
	sign = -1;
    } else {
	error("expected + or -", tok.buf);
    }

    gettoken();			// src operand (nn)
    strncpy(nn, tok.buf, sizeof(nn) - 1);
    nn[sizeof(nn) - 1] = '\0';

    if (tok.type == INTEGER || tok.type == HEXNUM) {
	char *endp = NULL;
	long v = strtol(tok.buf, &endp, 0);
	if (endp == tok.buf) {
	    error("bad number", tok.buf);
	}
	arg = (int) v;
    } else {
	if (pass == 2) {
	    idx = sym_find(tok.buf);
	    if (idx < 0)
		error("undefined symbol", tok.buf);
	    arg = labels[idx].addr;
	} else {
	    arg = 0;
	}
    }

    signed_disp = sign * arg;
    if (signed_disp < -128 || signed_disp > 127) {
	error("IY displacement out of range", nn);
    }
    disp8 = (unsigned char) (signed_disp & 0xFF);

    gettoken();			// )
    gettoken();			// ,
    gettoken();			// Register

    switch (tok.type) {
    case SYMBOL:
	if (eqv(tok.buf, "A")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),A",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x77, disp8, str);
	    return;
	} else if (eqv(tok.buf, "B")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),B",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x70, disp8, str);
	    return;
	} else if (eqv(tok.buf, "C")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),C",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x71, disp8, str);
	    return;
	} else if (eqv(tok.buf, "D")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),D",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x72, disp8, str);
	    return;
	} else if (eqv(tok.buf, "E")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),E",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x73, disp8, str);
	    return;
	} else if (eqv(tok.buf, "H")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),H",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x74, disp8, str);
	    return;
	} else if (eqv(tok.buf, "L")) {
	    snprintf(str, sizeof(str), "LD (IY%c%s),L",
		     (sign > 0 ? '+' : '-'), nn);
	    gen_op4(0xFD, 0x75, disp8, str);
	    return;
	} else {
	    error("LD operand", tok.buf);
	}
	break;

    default:
	error("LD operand", tok.buf);
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
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0xE9, "JP (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_op2(0xDD, 0xE9, "JP (IX)");
	} else if (eqv(tok.buf, "IY")) {
	    gen_op2(0xFD, 0xE9, "JP (IY)");
	}
	gettoken();		// )
    } else
	error("JP opetation", tok.buf);
}


// JP groupe
static void gen_jr(void)
{
    unsigned short arg, idx;
    unsigned char rel;
    int offset;
    char str[128];
    arg = 0;
    rel = 0;
    gettoken();			// flag or label
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "NZ")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR NZ,");
		strcat(str, tok.buf);
		offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x20, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR NZ,");
		    strcat(str, tok.buf);
		    offset = arg - (INDEX + 2);
		    if (offset < -128 || offset > 127) {
			error("JR operation", tok.buf);
		    }
		    rel = (unsigned char) (offset & 0xFF);
		}
		gen_op2(0x20, rel, str);
	    }
	} else if (eqv(tok.buf, "Z")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR Z,");
		strcat(str, tok.buf);
		offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x28, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR Z,");
		    strcat(str, tok.buf);
		    offset = arg - (INDEX + 2);
		    if (offset < -128 || offset > 127) {
			error("JR operation", tok.buf);
		    }
		    rel = (unsigned char) (offset & 0xFF);
		}
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
		rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x30, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR NC,");
		    strcat(str, tok.buf);
		    offset = arg - (INDEX + 2);
		    if (offset < -128 || offset > 127) {
			error("JR operation", tok.buf);
		    }
		    rel = (unsigned char) (offset & 0xFF);
		}
		gen_op2(0x30, rel, str);
	    }
	} else if (eqv(tok.buf, "C")) {
	    gettoken();		//comma
	    gettoken();
	    if (tok.type == INTEGER || tok.type == HEXNUM) {
		arg = strtol(tok.buf, NULL, 0);
		strcpy(str, "JR C,");
		strcat(str, tok.buf);
		offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		rel = (unsigned char) (offset & 0xFF);
		gen_op2(0x38, rel, str);
	    } else if (tok.type == SYMBOL) {
		if (pass == 2) {
		    idx = sym_find(tok.buf);
		    if (idx < 0)
			error("undefined symbol", tok.buf);
		    arg = labels[idx].addr;
		    strcpy(str, "JR C,");
		    strcat(str, tok.buf);
		    offset = arg - (INDEX + 2);
		    if (offset < -128 || offset > 127) {
			error("JR operation", tok.buf);
		    }
		    rel = (unsigned char) (offset & 0xFF);
		}
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
		offset = arg - (INDEX + 2);
		if (offset < -128 || offset > 127) {
		    error("JR operation", tok.buf);
		}
		rel = (unsigned char) (offset & 0xFF);
	    }

	    gen_op2(0x18, rel, str);
	}
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "JR ");
	strcat(str, tok.buf);
	offset = arg - (INDEX + 2);
	if (offset < -128 || offset > 127) {
	    error("JR operation", tok.buf);
	}
	rel = (unsigned char) (offset & 0xFF);
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
    } else if (tok.type == LPAREN) {
	gettoken();		// IX or IY
	if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0x35, "DEC (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0x35, "DEC (IY");
	}
	gettoken();		// )
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
    } else if (tok.type == LPAREN) {
	gettoken();		// (
	if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0x34, "INC (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0x34, "INC (IY");
	}
	gettoken();		//)
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
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0xA6, "AND (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0xA6, "AND (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0xA6, "AND (IY");
	}
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
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0xB6, "OR (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0xB6, "OR (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0xB6, "OR (IY");
	}
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
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0xAE, "XOR (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0xAE, "XOR (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0xAE, "XOR (IY");
	}
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
	if (eqv(tok.buf, "HL")) {
	    gen_op1(0xBE, "CP (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0xBE, "CP (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0xBE, "CP (IY");
	}
	gettoken();		// )
    } else if (tok.type == INTEGER || tok.type == HEXNUM) {
	arg = strtol(tok.buf, NULL, 0);
	strcpy(str, "CP ");
	strcat(str, tok.buf);
	gen_op2(0xFE, arg, str);
    }
}

static void gen_idx(unsigned char prefix,
		    unsigned char opcode, const char *mnem_rhs_fmt)
{
    char nn[32], str[64];
    int sign = +1;
    int arg = 0;
    int signed_disp;
    unsigned char disp8;

    // + / -
    gettoken();
    if (tok.type == PLUS)
	sign = +1;
    else if (tok.type == MINUS)
	sign = -1;
    else
	error("expected + or -", tok.buf);

    // nn
    gettoken();
    strncpy(nn, tok.buf, sizeof(nn) - 1);
    nn[sizeof(nn) - 1] = '\0';

    if (!eval_imm_or_sym(&arg)) {
	error("bad displacement", tok.buf);
    }

    signed_disp = sign * arg;
    if (signed_disp < -128 || signed_disp > 127) {
	error("index displacement out of range", nn);
    }
    disp8 = (unsigned char) (signed_disp & 0xFF);


    snprintf(str, sizeof(str), "%s%c%s)", mnem_rhs_fmt,
	     (sign > 0 ? '+' : '-'), nn);

    gen_op4(prefix, opcode, disp8, str);
}


static void gen_idx1(unsigned char prefix,
		     unsigned char opcode, unsigned char opcode1,
		     const char *mnem_rhs_fmt)
{
    char nn[32], str[64];
    int sign = +1;
    int arg = 0;
    int signed_disp;
    unsigned char disp8;

    // + / -
    gettoken();
    if (tok.type == PLUS)
	sign = +1;
    else if (tok.type == MINUS)
	sign = -1;
    else
	error("expected + or -", tok.buf);

    // nn
    gettoken();
    strncpy(nn, tok.buf, sizeof(nn) - 1);
    nn[sizeof(nn) - 1] = '\0';

    if (!eval_imm_or_sym(&arg)) {
	error("bad displacement", tok.buf);
    }

    signed_disp = sign * arg;
    if (signed_disp < -128 || signed_disp > 127) {
	error("index displacement out of range", nn);
    }
    disp8 = (unsigned char) (signed_disp & 0xFF);


    snprintf(str, sizeof(str), "%s%c%s)", mnem_rhs_fmt,
	     (sign > 0 ? '+' : '-'), nn);

    gen_op5(prefix, opcode, disp8, opcode1, str);
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
	    } else if (eqv(tok.buf, "IX")) {
		gen_idx(0xDD, 0x86, "ADD A,(IX");
	    } else if (eqv(tok.buf, "IY")) {
		gen_idx(0xFD, 0x86, "ADD A,(IY");
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
    } else if (eqv(tok.buf, "HL")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("ADD operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "BC")) {
		gen_op1(0x09, "ADD HL,BC");
	    } else if (eqv(tok.buf, "DE")) {
		gen_op1(0x19, "ADD HL,DE");
	    } else if (eqv(tok.buf, "HL")) {
		gen_op1(0x29, "ADD HL,HL");
	    } else if (eqv(tok.buf, "SP")) {
		gen_op1(0x39, "ADD HL,SP");
	    }
	}
    } else if (eqv(tok.buf, "IX")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("ADD operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "BC")) {
		gen_op2(0xDD, 0x09, "ADD IX,BC");
	    } else if (eqv(tok.buf, "DE")) {
		gen_op2(0xDD, 0x19, "ADD IX,DE");
	    } else if (eqv(tok.buf, "IX")) {
		gen_op2(0xDD, 0x29, "ADD IX,IX");
	    } else if (eqv(tok.buf, "SP")) {
		gen_op2(0xDD, 0x39, "ADD IX,SP");
	    }
	}
    } else if (eqv(tok.buf, "IY")) {
	gettoken();		//comma
	if (tok.type != COMMA)
	    error("ADD operation expected commma", tok.buf);
	gettoken();
	if (tok.type == SYMBOL) {
	    if (eqv(tok.buf, "BC")) {
		gen_op2(0xFD, 0x09, "ADD IY,BC");
	    } else if (eqv(tok.buf, "DE")) {
		gen_op2(0xFD, 0x19, "ADD IY,DE");
	    } else if (eqv(tok.buf, "IY")) {
		gen_op2(0xFD, 0x29, "ADD IY,IY");
	    } else if (eqv(tok.buf, "SP")) {
		gen_op2(0xFD, 0x39, "ADD IY,SP");
	    }
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
	    } else if (eqv(tok.buf, "IX")) {
		gen_idx(0xDD, 0x8E, "ADC A,(IX");
	    } else if (eqv(tok.buf, "IY")) {
		gen_idx(0xFD, 0x8E, "ADC A,(IY");
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
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx(0xDD, 0x96, "SUB (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx(0xFD, 0x96, "SUB (IY");
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
	    } else if (eqv(tok.buf, "IX")) {
		gen_idx(0xDD, 0x9E, "SBC A,(IX");
	    } else if (eqv(tok.buf, "IY")) {
		gen_idx(0xFD, 0x9E, "SBC A,(IY");
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
	} else if (eqv(tok.buf, "IX")) {
	    gen_op2(0xDD, 0xe5, "PUSH IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_op2(0xFD, 0xe5, "PUSH IY");
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
	} else if (eqv(tok.buf, "IX")) {
	    gen_op2(0xDD, 0xe1, "POP IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_op2(0xFD, 0xe1, "POP IY");
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

// BIT groupe  
static void gen_bit(void)
{
    char str[128];
    int bit;

    gettoken();			// bit number

    if (!(tok.type == INTEGER || tok.type == HEXNUM)) {
	error("BIT operation expected bit number", tok.buf);
	return;
    }

    bit = (int) strtol(tok.buf, NULL, 0);
    if (bit < 0 || bit > 7) {
	error("BIT operation bit number out of range (0-7)", tok.buf);
	return;
    }

    gettoken();			// comma
    if (tok.type != COMMA) {
	error("BIT operation expected comma", tok.buf);
	return;
    }

    gettoken();			// operand: reg or (HL)
    if (tok.type == SYMBOL) {
	// BIT b,r  => CB (0x40 + (b<<3) + rcode)
	unsigned char op = 0;

	if (eqv(tok.buf, "B")) {
	    op = 0x40 + (bit << 3) + 0;
	} else if (eqv(tok.buf, "C")) {
	    op = 0x40 + (bit << 3) + 1;
	} else if (eqv(tok.buf, "D")) {
	    op = 0x40 + (bit << 3) + 2;
	} else if (eqv(tok.buf, "E")) {
	    op = 0x40 + (bit << 3) + 3;
	} else if (eqv(tok.buf, "H")) {
	    op = 0x40 + (bit << 3) + 4;
	} else if (eqv(tok.buf, "L")) {
	    op = 0x40 + (bit << 3) + 5;
	} else if (eqv(tok.buf, "A")) {
	    op = 0x40 + (bit << 3) + 7;
	} else {
	    error("BIT operation expected register A/B/C/D/E/H/L",
		  tok.buf);
	    return;
	}

	// debug string
	sprintf(str, "BIT %d,%s", bit, tok.buf);

	// emit
	gen_op2(0xCB, op, str);

    } else if (tok.type == LPAREN) {
	// BIT b,(HL) => CB (0x40 + (b<<3) + 6)
	gettoken();
	if (!eqv(tok.buf, "HL")) {
	    error("BIT operation expected (HL)", tok.buf);
	    return;
	}
	gettoken();		// )
	if (tok.type != RPAREN) {
	    error("BIT operation expected right paren", tok.buf);
	    return;
	}

	sprintf(str, "BIT %d,(HL)", bit);
	gen_op2(0xCB, (unsigned char) (0x40 + (bit << 3) + 6), str);

    } else {
	error("BIT operation expected register or (HL)", tok.buf);
	return;
    }
}

// SET groupe  (IX/IY variants are omitted; you will handwrite them)
static void gen_set(void)
{
    char str[128];
    int bit;

    gettoken();			// bit number

    if (!(tok.type == INTEGER || tok.type == HEXNUM)) {
	error("SET operation expected bit number", tok.buf);
	return;
    }

    bit = (int) strtol(tok.buf, NULL, 0);
    if (bit < 0 || bit > 7) {
	error("SET operation bit number out of range (0-7)", tok.buf);
	return;
    }

    gettoken();			// comma
    if (tok.type != COMMA) {
	error("SET operation expected comma", tok.buf);
	return;
    }

    gettoken();			// operand: reg or (HL)
    if (tok.type == SYMBOL) {
	// SET b,r  => CB (0xC0 + (b<<3) + rcode)
	unsigned char op = 0;

	if (eqv(tok.buf, "B")) {
	    op = 0xC0 + (bit << 3) + 0;
	} else if (eqv(tok.buf, "C")) {
	    op = 0xC0 + (bit << 3) + 1;
	} else if (eqv(tok.buf, "D")) {
	    op = 0xC0 + (bit << 3) + 2;
	} else if (eqv(tok.buf, "E")) {
	    op = 0xC0 + (bit << 3) + 3;
	} else if (eqv(tok.buf, "H")) {
	    op = 0xC0 + (bit << 3) + 4;
	} else if (eqv(tok.buf, "L")) {
	    op = 0xC0 + (bit << 3) + 5;
	} else if (eqv(tok.buf, "A")) {
	    op = 0xC0 + (bit << 3) + 7;
	} else {
	    error("SET operation expected register A/B/C/D/E/H/L",
		  tok.buf);
	    return;
	}

	sprintf(str, "SET %d,%s", bit, tok.buf);
	gen_op2(0xCB, op, str);

    } else if (tok.type == LPAREN) {
	// SET b,(HL) => CB (0xC0 + (b<<3) + 6)
	gettoken();
	if (!eqv(tok.buf, "HL")) {
	    error("SET operation expected (HL)", tok.buf);
	    return;
	}
	gettoken();		// )
	if (tok.type != RPAREN) {
	    error("SET operation expected right paren", tok.buf);
	    return;
	}

	sprintf(str, "SET %d,(HL)", bit);
	gen_op2(0xCB, (unsigned char) (0xC0 + (bit << 3) + 6), str);

    } else {
	error("SET operation expected register or (HL)", tok.buf);
	return;
    }
}

// RES groupe  (IX/IY variants are omitted; you will handwrite them)
static void gen_res(void)
{
    char str[128];
    int bit;

    gettoken();			// bit number

    if (!(tok.type == INTEGER || tok.type == HEXNUM)) {
	error("RES operation expected bit number", tok.buf);
	return;
    }

    bit = (int) strtol(tok.buf, NULL, 0);
    if (bit < 0 || bit > 7) {
	error("RES operation bit number out of range (0-7)", tok.buf);
	return;
    }

    gettoken();			// comma
    if (tok.type != COMMA) {
	error("RES operation expected comma", tok.buf);
	return;
    }

    gettoken();			// operand: reg or (HL)
    if (tok.type == SYMBOL) {
	unsigned char op = 0;

	if (eqv(tok.buf, "B")) {
	    op = 0x80 + (bit << 3) + 0;
	} else if (eqv(tok.buf, "C")) {
	    op = 0x80 + (bit << 3) + 1;
	} else if (eqv(tok.buf, "D")) {
	    op = 0x80 + (bit << 3) + 2;
	} else if (eqv(tok.buf, "E")) {
	    op = 0x80 + (bit << 3) + 3;
	} else if (eqv(tok.buf, "H")) {
	    op = 0x80 + (bit << 3) + 4;
	} else if (eqv(tok.buf, "L")) {
	    op = 0x80 + (bit << 3) + 5;
	} else if (eqv(tok.buf, "A")) {
	    op = 0x80 + (bit << 3) + 7;
	} else {
	    error("RES operation expected register A/B/C/D/E/H/L",
		  tok.buf);
	    return;
	}

	sprintf(str, "RES %d,%s", bit, tok.buf);
	gen_op2(0xCB, op, str);

    } else if (tok.type == LPAREN) {
	gettoken();
	if (!eqv(tok.buf, "HL")) {
	    error("RES operation expected (HL)", tok.buf);
	    return;
	}
	gettoken();		// )
	if (tok.type != RPAREN) {
	    error("RES operation expected right paren", tok.buf);
	    return;
	}

	sprintf(str, "RES %d,(HL)", bit);
	gen_op2(0xCB, (unsigned char) (0x80 + (bit << 3) + 6), str);

    } else {
	error("RES operation expected register or (HL)", tok.buf);
	return;
    }
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



// SLA groupe
static void gen_sla(void)
{

    gettoken();
    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x27, "SLA A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x20, "SLA B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x21, "SLA C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x22, "SLA D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x23, "SLA E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x24, "SLA H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x25, "SLA L");
	}
    } else if (tok.type == LPAREN) {
	gettoken();
	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x26, "SLA (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    gen_idx1(0xDD, 0xCB, 0x26, "SLA (IX");
	} else if (eqv(tok.buf, "IY")) {
	    gen_idx1(0xFD, 0xCB, 0x26, "SLA (IY");
	}
	gettoken();		// )
	if (tok.type != RPAREN)
	    error("SLA operation expected right paren", tok.buf);
    }

}

// SRA groupe
static void gen_sra(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x2F, "SRA A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x28, "SRA B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x29, "SRA C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x2A, "SRA D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x2B, "SRA E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x2C, "SRA H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x2D, "SRA L");
	} else {
	    error("SRA operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x2E, "SRA (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 2E
	    gen_idx1(0xDD, 0xCB, 0x2E, "SRA (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 2E
	    gen_idx1(0xFD, 0xCB, 0x2E, "SRA (IY");
	} else {
	    error("SRA operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("SRA operation expected right paren", tok.buf);

    } else {
	error("SRA operation expected register or (..)", tok.buf);
    }
}

// SRL groupe
static void gen_srl(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x3F, "SRL A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x38, "SRL B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x39, "SRL C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x3A, "SRL D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x3B, "SRL E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x3C, "SRL H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x3D, "SRL L");
	} else {
	    error("SRL operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x3E, "SRL (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 3E
	    gen_idx1(0xDD, 0xCB, 0x3E, "SRL (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 3E
	    gen_idx1(0xFD, 0xCB, 0x3E, "SRL (IY");
	} else {
	    error("SRL operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("SRL operation expected right paren", tok.buf);

    } else {
	error("SRL operation expected register or (..)", tok.buf);
    }
}

// RR groupe
static void gen_rr(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x1F, "RR A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x18, "RR B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x19, "RR C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x1A, "RR D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x1B, "RR E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x1C, "RR H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x1D, "RR L");
	} else {
	    error("RR operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x1E, "RR (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 1E
	    gen_idx1(0xDD, 0xCB, 0x1E, "RR (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 1E
	    gen_idx1(0xFD, 0xCB, 0x1E, "RR (IY");
	} else {
	    error("RR operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("RR operation expected right paren", tok.buf);

    } else {
	error("RR operation expected register or (..)", tok.buf);
    }
}

// RL groupe
static void gen_rl(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x17, "RL A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x10, "RL B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x11, "RL C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x12, "RL D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x13, "RL E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x14, "RL H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x15, "RL L");
	} else {
	    error("RL operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x16, "RL (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 16
	    gen_idx1(0xDD, 0xCB, 0x16, "RL (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 16
	    gen_idx1(0xFD, 0xCB, 0x16, "RL (IY");
	} else {
	    error("RL operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("RL operation expected right paren", tok.buf);

    } else {
	error("RL operation expected register or (..)", tok.buf);
    }
}

// RLC groupe
static void gen_rlc(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x07, "RLC A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x00, "RLC B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x01, "RLC C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x02, "RLC D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x03, "RLC E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x04, "RLC H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x05, "RLC L");
	} else {
	    error("RLC operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x06, "RLC (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 06
	    gen_idx1(0xDD, 0xCB, 0x06, "RLC (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 06
	    gen_idx1(0xFD, 0xCB, 0x06, "RLC (IY");
	} else {
	    error("RLC operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("RLC operation expected right paren", tok.buf);

    } else {
	error("RLC operation expected register or (..)", tok.buf);
    }
}


// RRC groupe
static void gen_rrc(void)
{
    gettoken();

    if (tok.type == SYMBOL) {
	if (eqv(tok.buf, "A")) {
	    gen_op2(0xCB, 0x0F, "RRC A");
	} else if (eqv(tok.buf, "B")) {
	    gen_op2(0xCB, 0x08, "RRC B");
	} else if (eqv(tok.buf, "C")) {
	    gen_op2(0xCB, 0x09, "RRC C");
	} else if (eqv(tok.buf, "D")) {
	    gen_op2(0xCB, 0x0A, "RRC D");
	} else if (eqv(tok.buf, "E")) {
	    gen_op2(0xCB, 0x0B, "RRC E");
	} else if (eqv(tok.buf, "H")) {
	    gen_op2(0xCB, 0x0C, "RRC H");
	} else if (eqv(tok.buf, "L")) {
	    gen_op2(0xCB, 0x0D, "RRC L");
	} else {
	    error("RRC operation expected register", tok.buf);
	}

    } else if (tok.type == LPAREN) {
	gettoken();

	if (eqv(tok.buf, "HL")) {
	    gen_op2(0xCB, 0x0E, "RRC (HL)");
	} else if (eqv(tok.buf, "IX")) {
	    // emits: DD CB disp 0E
	    gen_idx1(0xDD, 0xCB, 0x0E, "RRC (IX");
	} else if (eqv(tok.buf, "IY")) {
	    // emits: FD CB disp 0E
	    gen_idx1(0xFD, 0xCB, 0x0E, "RRC (IY");
	} else {
	    error("RRC operation expected HL/IX/IY", tok.buf);
	}

	gettoken();		// should read ')'
	if (tok.type != RPAREN)
	    error("RRC operation expected right paren", tok.buf);

    } else {
	error("RRC operation expected register or (..)", tok.buf);
    }
}


//----------INCLUDE---------
/*
	INCLUDE filename.ext
*/

static void gen_include(char* include)
{
	FILE *save1;
	int save2,save3;
	char str[64];

	if(include_flag)
		error("INCLUDE nested!",include);

	strcpy(str,include);
	save1 = input_stream;
	save2 = INDEX;
	save3 = lineno;
	include_flag = 1;
	input_stream = fopen(str, "r");
    if (!input_stream) 
		error("cannot open file: \n", include);
	
	if(pass == 1){
		//pass1
		printf("INCLUDE %s\n", include);
		INDEX = save2;
		lineno = 0;
		gen_label();
	} else if(pass == 2){
		//pass2
		INDEX = save2;
		lineno = 0;
		gen_code();
	}
	input_stream = save1;
	lineno = save3;
	include_flag = 0;
}
