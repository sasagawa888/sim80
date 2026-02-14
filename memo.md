
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

