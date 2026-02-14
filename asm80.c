#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <setjmp.h>
#include <float.h>

#define BUFSIZE 256
#define EOL		'\n'
#define TAB		'\t'
#define SPACE	' '
#define NUL		'\0'
FILE *input_stream;

typedef enum toktype {LPAREN,RPAREN,COMMA,INTEGER,HEXNUM,HEXNUM1,SYMBOL,LABEL,OTHER,FILEEND} toktype;
typedef enum backtrack {GO,BACK} backtrack;

typedef struct token {
	char ch;
    backtrack flag;
	toktype type;
    char buf[BUFSIZE];
} token;

token stok = { GO, OTHER };

int main(int argc, char *argv[])
{
    return 0;
}


int inttoken(char buf[])
{
    int i;
    char c;

	i = 0;			// {1234...}
	while ((c = buf[i]) != NUL){
	    if (isdigit(c))
		i++;
	    else
		return (0);
    }
    return (1);
}


int hextoken(char buf[])
{
    int i;
    char c;

    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')){
    i = 0;
    while ((c = buf[i]) != NUL){
	if (isxdigit(c))
	    i++;
	else
	    return (0);
    }}
	return (1);
}

int hextoken1(char buf[])
{
    int i,len;

    len = strlen(buf);
    if(buf[len] == 'H' || buf[len] == 'h'){
        i = len-1;
        while(i>=0){
            if(!isxdigit(buf[i]))
                return(0);
        i--;
        }
        buf[len] = NUL;
        return(1);
    }
    return(0);
}

int issymch(char c)
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


int symboltoken(char buf[])
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


int labeltoken(char buf[])
{

    if(!symboltoken(buf))
	    return (0);
    else{
        int len = strlen(buf);
        if(buf[len] == ':') 
        return(1);
    }

    return (0);
}


void gettoken(void)
{
    char c;
    int pos;

    if (stok.flag == BACK) {
	stok.flag = GO;
	return;
    }

    if (stok.ch == ')') {
	stok.type = RPAREN;
	stok.ch = NUL;
	return;
    }

    if (stok.ch == '(') {
	stok.type = LPAREN;
	stok.ch = NUL;
	return;
    }

    skip:
    c = fgetc(input_stream);
    while ((c == SPACE) || (c == EOL) || (c == TAB))
	c = fgetc(input_stream);

    if(c == ';'){
        while (c != EOL){
	        c = fgetc(input_stream);
        }
        goto skip;
    }

    switch (c) {
    case '(':
	stok.type = LPAREN;
	break;
    case ')':
	stok.type = RPAREN;
	break;
	stok.type = COMMA;
	break;
    case EOF:
	stok.type = FILEEND;
	return;
    default:{
	    pos = 0;
	    stok.buf[pos++] = c;
	    while (((c = fgetc(input_stream)) != EOL) && (pos < BUFSIZE-1) &&
		   (c != SPACE) && (c != '(') && (c != ')') &&
		   (c != '`') && (c != ',') && (c != '@'))
		stok.buf[pos++] = c;

	    stok.buf[pos] = NUL;
	    stok.ch = c;
	    if (inttoken(stok.buf)) {
		stok.type = INTEGER;
		break;
	    }
        if (hextoken(stok.buf)) {
		stok.type = HEXNUM;
		break;
	    }
         if (hextoken1(stok.buf)) {
		stok.type = HEXNUM1;
		break;
	    }
        if (labeltoken(stok.buf)) {
        int len = strlen(stok.buf);
        stok.buf[len] = NUL;
		stok.type = LABEL;
		break;
	    }
	    if (symboltoken(stok.buf)) {
		stok.type = SYMBOL;
		break;
	    }
	    stok.type = OTHER;
	}
    }
}