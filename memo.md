
# asm80 – Unsupported Z80 Instructions (Memo)

This is a memo listing Z80 instructions that are currently **not supported** in `asm80`.

At present, the assembler recognizes only the following mnemonics:

HALT, NOP, JP, JR, LD, INC, DEC,
AND, OR, XOR, CP,
ADD, ADC, SUB, SBC,
CALL, RET, PUSH, POP, RST


All other Z80 instructions are currently unsupported.

---

## 1. Control Flow & Interrupts

- DJNZ
- JP (HL)
- JP (IX)
- JP (IY)
- RETI
- RETN
- DI
- EI
- IM 0
- IM 1
- IM 2

---

## 2. Exchange Instructions

- EX AF,AF'
- EX DE,HL
- EX (SP),HL
- EX (SP),IX
- EX (SP),IY
- EXX

---

## 3. Block Transfer & Compare

- LDI
- LDIR
- LDD
- LDDR
- CPI
- CPIR
- CPD
- CPDR

---

## 4. I/O Instructions

- IN (all forms)
- OUT (all forms)

---

## 5. Bit Operations (CB Prefix)

- BIT b,r
- SET b,r
- RES b,r

---

## 6. Shift & Rotate (CB Prefix)

- RLC r
- RRC r
- RL r
- RR r
- SLA r
- SRA r
- SRL r

### Accumulator Variants

- RLCA
- RRCA
- RLA
- RRA

---

## 7. BCD and Flag Operations

- DAA
- CPL
- SCF
- CCF
- NEG

---

## 8. Extended (ED/DD/FD/CB Prefix Groups)

- RLD
- RRD
- All ED-prefixed extended instructions
- All IX/IY displacement-based variants not yet implemented

---

## Notes

This list includes:

- Instructions not recognized at all
- Instruction families not yet implemented
- Prefix-based instruction groups (CB, ED, DD, FD)

Future implementation may prioritize:

1. CP/M compatibility
2. IX/IY support
3. CB prefix bit operations
4. ED block instructions
5. Full Z80 coverage


# main

main(){
    入力、出力ファイル設定
    各種初期化
    setjmpエラー処理
    if(ret==0){
        gen_label();
        gen_code();
        バイナリ出力
    } else if(ret==1){
        エラー処理
        return 1;
    }
    正常終了
    return 0;
}

# パーサー

lisp用流用
gettoken()
tok.*  

tok.type  {SYMBOL,LABEL,HEXNUM,DEXNUM,COMMMA,LPAREN,RPAREN,EOF}
;コメント　スキップ

char_to_num(char*)  数値トークンを10進数に変換

#　マシン語生成
pass {1,2}  

void gen_label()  ラベル位置を計算
void gen_code() マシン語を生成、ジャンプ先計算
途中エラーがおきたときにはerror(msg,opc,arg)を呼び出す。
errorはlongjumpでmainに復帰して異常終了

opecode(char*) トークンに応じてenumを返す。{LD,JP,JR,DEC,...}
operand(char*) トークンに応じてenumを返す {A,B,HL,DE,(HL),(nn),...}

gen_*()

gen_ld(),gen_jp(),gen_call(),gen_ret(),...
passの値によって動作を変える。1のときはラベル計算、2のときは機械語生成

命令は３つに大別される。

op3  LD A,(HL) ...
op2  DEC A ...
op1  HALT,

op1グループは一つの関数で処理する。

# error

error(message,opecode,arg)
行位置を表示する。開いているファイルを閉じる。
longjmpでmainに復帰、異常終了させる。

# 大域変数
ram[0x10000]  RAM領域　
label[1024] 　ラベルとアドレスの対応表
pass pass1,pass2 切り替え


# パーツ
def_sym(char,addr)　ラベル登録
find_sym(char)　　　ラベル検索
elmit8(v) 1バイト機械語
elmit16(v)　2バイト機械語

