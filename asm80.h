/* asm80 */



#define BUFSIZE 256
#define EOL		'\n'
#define TAB		'\t'
#define SPACE	' '
#define NUL		'\0'


typedef enum toktype { LPAREN, RPAREN, COMMA, DOT, PLUS, MINUS, INTEGER, HEXNUM, 
    SYMBOL, LABEL, OTHER, FILEEND
} toktype;
typedef enum backtrack { GO, BACK } backtrack;

typedef struct token {
    char ch;
    backtrack flag;
    toktype type;
    char buf[BUFSIZE];
} token;


#define MAX_SYMS   1024
#define MAX_LABEL  8

typedef struct {
    char name[MAX_LABEL + 1];
    uint16_t addr;
} Symbol;


static void gen_label(void);
static void gen_code(void);
static void create_output_file(char *out, size_t outsz, const char *in);
