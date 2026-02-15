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
static void gen_ret(void);
static void gen_jp(void);
static void gen_inc(void);
static void gen_dec(void);
static void gen_add(void);
static void gen_sub(void);
static void gen_code1(char* op);
static void gen_op1(unsigned int v,char* op);
static void gettoken(void);
static void emit8(unsigned int v);
static void emit16(unsigned int v);
static void error(char* ope, char* msg);
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
	tok.ch = NUL;
	return;
    }

    if (tok.ch == '(') {
	tok.type = LPAREN;
	tok.ch = NUL;
	return;
    }

    if (tok.ch == ',') {
	tok.type = COMMA;
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
	c = fgetc(input_stream);
    }
    if (c == EOL)
	lineno++;

    if (c == ';') {
	while (c != EOL && c != EOF)
	    c = fgetc(input_stream);
	if (c == EOF) {
	    tok.type = FILEEND;
	    return;
	}
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

    if (strlen(buf) > 8) error("Error label length is max 8 chars",buf);

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
    } else if (eqv(op, "LD")){
	gen_ld();
    } else if (eqv(op, "INC")){
	gen_inc();
    } else if (eqv(op, "DEC")){
	gen_dec();
    } else if (eqv(op, "ADD")){
	gen_add();
    } else if (eqv(op, "SUB")){
	gen_sub();
    } else if (eqv(op, "RET")){
	gen_ret();
    } 
    else if (tok.type == LABEL) {
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
    char str[64];
    gettoken();
    if(eqv(tok.buf,"A")){
        gettoken(); //comma
        if(tok.type != COMMA)
            error("LD comma expected",tok.buf);
        gettoken();
        int arg,idx;
        arg = 0;
        switch(tok.type){
            case SYMBOL:
            if(pass == 2){
            idx = sym_find(tok.buf);
            if (idx < 0) 
                error("undefined symbol", tok.buf);
            arg = labels[idx].addr;
            }   
            imediate:
            strcpy(str,"LD A,");
            strcat(str,tok.buf);
            gen_op2(0x3e,arg,str);
            return;
            case INTEGER:
            case HEXNUM:
            arg = strtol(tok.buf, NULL, 0);
            goto imediate;
            case LPAREN:// e.g. (HL)
            gettoken();
            if(eqv(tok.buf,"HL")){
                gen_op1(0x7e,"LD A,(HL)");
            } else if(eqv(tok.buf,"BC")){
                gen_op1(0x0A,"LD A,(BC)");
            } else if(eqv(tok.buf,"DE")){
                gen_op1(0x1A,"LD A,(DE)");
            } else if(tok.type == INTEGER || tok.type == HEXNUM){
                arg = strtol(tok.buf, NULL, 0);
                strcpy(str,"LD A,(");
                strcat(str,tok.buf);
                strcat(str,")");
                gen_op3(0x3a,arg,str);
            } else if(tok.type == SYMBOL){
                if(pass == 2){
                idx = sym_find(tok.buf);
                if (idx < 0){ 
                    error("undefined symbol", tok.buf);}
                arg = labels[idx].addr;
                }
                strcpy(str,"LD A,(");
                strcat(str,tok.buf);
                strcat(str,")");
                gen_op3(0x3a,arg,str);
            }

            gettoken();
            if(tok.type != RPAREN)
                error("LD indirect",tok.buf);
            return;
            default:
            error("LD operand",tok.buf);
        }
        
    } else
        error("LD operand ",tok.buf);
}

// RET groupe
static void gen_ret(void)
{

}

// JP groupe
static void gen_jp(void)
{
    gettoken();
    if (tok.type == SYMBOL) {
	if (pass == 2)
	    printf("%04X  ", INDEX);
	emit8(0xC3);
	if (pass == 2) {
	    int idx = sym_find(tok.buf);
	    if (idx < 0) {
		error("Error undefined label", tok.buf);
	    }
	    emit16(labels[idx].addr);
	} else {
	    emit16(0);		// dummy
	}
	if (pass == 2)
	    printf("\tJP %s\n", tok.buf);
    } else {
	printf("not label");
    }
}

// DEC groupe
static void gen_dec(void)
{
    gettoken();
    if(tok.type == SYMBOL){
        if(eqv(tok.buf,"A")){
            gen_op1(0x3d,"DEC A");
        } else if(eqv(tok.buf,"B")){
            gen_op1(0x05,"DEC B");
        } else if(eqv(tok.buf,"C")){
            gen_op1(0x0d,"DEC C");
        } else if(eqv(tok.buf,"D")){
            gen_op1(0x15,"DEC D");
        } else if(eqv(tok.buf,"E")){
            gen_op1(0x1d,"DEC E");
        } else if(eqv(tok.buf,"H")){
            gen_op1(0x25,"DEC H");
        } else if(eqv(tok.buf,"L")){
            gen_op1(0x2d,"DEC L");
        } 
    }
}


// INC groupe
static void gen_inc(void)
{
    gettoken();
    if(tok.type == SYMBOL){
        if(eqv(tok.buf,"A")){
            gen_op1(0x3c,"INC A");
        } else if(eqv(tok.buf,"B")){
            gen_op1(0x04,"INC B");
        } else if(eqv(tok.buf,"C")){
            gen_op1(0x0c,"INC C");
        } else if(eqv(tok.buf,"D")){
            gen_op1(0x14,"INC D");
        } else if(eqv(tok.buf,"E")){
            gen_op1(0x1c,"INC E");
        } else if(eqv(tok.buf,"H")){
            gen_op1(0x24,"INC H");
        } else if(eqv(tok.buf,"L")){
            gen_op1(0x2c,"INC L");
        } 
    }
}

// ADD groupe
static void gen_add(void)
{
    char str[128];

    gettoken();
    if(eqv(tok.buf,"A")){
        gettoken(); //comma
        if(tok.type != COMMA)
            error("ADD operation expected commma",tok.buf);
        gettoken();
    if(tok.type == SYMBOL){
        if(eqv(tok.buf,"A")){
            gen_op1(0x87,"ADD A,A");
        } else if(eqv(tok.buf,"B")){
            gen_op1(0x80,"ADD A,B");
        } else if(eqv(tok.buf,"C")){
            gen_op1(0x81,"ADD A,C");
        } else if(eqv(tok.buf,"D")){
            gen_op1(0x82,"ADD A,D");
        } else if(eqv(tok.buf,"E")){
            gen_op1(0x83,"ADD A,E");
        } else if(eqv(tok.buf,"H")){
            gen_op1(0x84,"ADD A,H");
        } else if(eqv(tok.buf,"L")){
            gen_op1(0x85,"ADD A,L");
        } 
    } else if(tok.type == LPAREN){
        gettoken();
        if(eqv(tok.buf,"HL")){
             gen_op1(0x86,"ADD A,(HL)");
        }
        gettoken(); // )
        if(tok.type != RPAREN)
            error("ADD operation expected right paren",tok.buf);
    } else if(tok.type == INTEGER || tok.type == HEXNUM){
        int arg; 
        arg=0;
        arg = strtol(tok.buf, NULL, 0);
        strcpy(str,"ADD A,");
        strcat(str,tok.buf);
        gen_op2(0xc6,arg,str);
    }
    }
}

// SUB groupe
static void gen_sub(void)
{
    char str[128];

    gettoken();
    if(tok.type == SYMBOL){
        if(eqv(tok.buf,"A")){
            gen_op1(0x97,"SUB A");
        } else if(eqv(tok.buf,"B")){
            gen_op1(0x90,"SUB B");
        } else if(eqv(tok.buf,"C")){
            gen_op1(0x91,"SUB C");
        } else if(eqv(tok.buf,"D")){
            gen_op1(0x92,"SUB D");
        } else if(eqv(tok.buf,"E")){
            gen_op1(0x93,"SUB E");
        } else if(eqv(tok.buf,"H")){
            gen_op1(0x94,"SUB H");
        } else if(eqv(tok.buf,"L")){
            gen_op1(0x95,"SUB L");
        } 
    } else if(tok.type == LPAREN){
        gettoken();
        if(eqv(tok.buf,"HL")){
             gen_op1(0x96,"SUB (HL)");
        }
        gettoken(); // )
        if(tok.type != RPAREN)
            error("SUB operation expected right paren",tok.buf);
    } else if(tok.type == INTEGER || tok.type == HEXNUM){
        int arg; 
        arg=0;
        arg = strtol(tok.buf, NULL, 0);
        strcpy(str,"SUB ");
        strcat(str,tok.buf);
        gen_op2(0xd6,arg,str);
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
    if(strcmp(x,y)==0)
        return(1);
    else 
        return(0);
}