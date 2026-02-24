#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define version 0.9

uint8_t ram[0x10000];

/* 8-bit regs */
uint8_t A, F, B, C, D, E, H, L;
/* 16-bit regs */
uint16_t PC, SP, IX, IY;
/* alternate regs */
uint8_t A2, F2, B2, C2, D2, E2, H2, L2;

/* flags bits (Z80) */
#define FLAG_S 0x80
#define FLAG_Z 0x40
#define FLAG_Y 0x20
#define FLAG_H 0x10
#define FLAG_X 0x08
#define FLAG_PV 0x04
#define FLAG_N 0x02
#define FLAG_C 0x01

static inline uint8_t parity8(uint8_t x){
    /* even parity -> 1 */
    x ^= x >> 4;
    x &= 0x0F;
    return (0x6996 >> x) & 1;
}

static inline uint16_t HL(void){ return (uint16_t)((H<<8)|L); }
static inline void SET_HL(uint16_t v){ H=(uint8_t)(v>>8); L=(uint8_t)v; }

static inline uint16_t BC(void){ return (uint16_t)((B<<8)|C); }
static inline void SET_BC(uint16_t v){ B=(uint8_t)(v>>8); C=(uint8_t)v; }

static inline uint16_t DE(void){ return (uint16_t)((D<<8)|E); }
static inline void SET_DE(uint16_t v){ D=(uint8_t)(v>>8); E=(uint8_t)v; }

static inline uint8_t mem_rd(uint16_t a){ return ram[a]; }
static inline void mem_wr(uint16_t a, uint8_t v){ ram[a]=v; }

static inline uint8_t fetch8(void){ return mem_rd(PC++); }
static inline uint16_t fetch16(void){
    uint8_t lo = fetch8();
    uint8_t hi = fetch8();
    return (uint16_t)(lo | (hi<<8));
}

static inline void push16(uint16_t v){
    mem_wr(--SP, (uint8_t)(v>>8));
    mem_wr(--SP, (uint8_t)(v));
}
static inline uint16_t pop16(void){
    uint8_t lo = mem_rd(SP++);
    uint8_t hi = mem_rd(SP++);
    return (uint16_t)(lo | (hi<<8));
}

/* ---------- register access by index r (0..7 = B,C,D,E,H,L,(HL),A) ---------- */
static inline uint8_t get_r(uint8_t r){
    switch(r){
        case 0: return B;
        case 1: return C;
        case 2: return D;
        case 3: return E;
        case 4: return H;
        case 5: return L;
        case 6: return mem_rd(HL());
        default: return A;
    }
}
static inline void set_r(uint8_t r, uint8_t v){
    switch(r){
        case 0: B=v; break;
        case 1: C=v; break;
        case 2: D=v; break;
        case 3: E=v; break;
        case 4: H=v; break;
        case 5: L=v; break;
        case 6: mem_wr(HL(), v); break;
        default: A=v; break;
    }
}

/* IX/IY displacement memory helpers */
static inline uint8_t mem_ixd_rd(uint16_t base, int8_t d){ return mem_rd((uint16_t)(base + d)); }
static inline void mem_ixd_wr(uint16_t base, int8_t d, uint8_t v){ mem_wr((uint16_t)(base + d), v); }

/* ---------- flag helpers (minimum sufficient) ---------- */
static inline void set_SZP(uint8_t v){
    /* keep C/H/N etc as is; caller usually sets them */
    F = (F & ~(FLAG_S|FLAG_Z|FLAG_PV|FLAG_Y|FLAG_X))
      | (v & (FLAG_Y|FLAG_X))
      | (v==0 ? FLAG_Z : 0)
      | (v & 0x80 ? FLAG_S : 0)
      | (parity8(v) ? FLAG_PV : 0);
}

/* ADD/ADC/SUB/SBC/AND/OR/XOR/CP on A with value v */
static inline void alu_add(uint8_t v, int use_carry){
    uint16_t a = A;
    uint16_t c = use_carry ? (F & FLAG_C) : 0;
    uint16_t r = a + v + c;
    uint8_t rr = (uint8_t)r;

    uint8_t hf = ((a & 0x0F) + (v & 0x0F) + c) & 0x10 ? FLAG_H : 0;
    uint8_t cf = (r & 0x100) ? FLAG_C : 0;
    uint8_t of = (~(a ^ v) & (a ^ rr) & 0x80) ? FLAG_PV : 0;

    F = (rr & (FLAG_Y|FLAG_X)) | hf | of | cf;
    if(rr==0) F |= FLAG_Z;
    if(rr & 0x80) F |= FLAG_S;
    /* N=0 */
    A = rr;
}
static inline void alu_sub(uint8_t v, int use_carry){
    uint16_t a = A;
    uint16_t c = use_carry ? (F & FLAG_C) : 0;
    uint16_t r = a - v - c;
    uint8_t rr = (uint8_t)r;

    uint8_t hf = ((a & 0x0F) - (v & 0x0F) - c) & 0x10 ? FLAG_H : 0;
    uint8_t cf = (r & 0x100) ? FLAG_C : 0;
    uint8_t of = ((a ^ v) & (a ^ rr) & 0x80) ? FLAG_PV : 0;

    F = (rr & (FLAG_Y|FLAG_X)) | FLAG_N | hf | of | cf;
    if(rr==0) F |= FLAG_Z;
    if(rr & 0x80) F |= FLAG_S;
    A = rr;
}
static inline void alu_and(uint8_t v){
    A &= v;
    F = FLAG_H | (A & (FLAG_Y|FLAG_X));
    if(A==0) F |= FLAG_Z;
    if(A & 0x80) F |= FLAG_S;
    if(parity8(A)) F |= FLAG_PV;
}
static inline void alu_or(uint8_t v){
    A |= v;
    F = (A & (FLAG_Y|FLAG_X));
    if(A==0) F |= FLAG_Z;
    if(A & 0x80) F |= FLAG_S;
    if(parity8(A)) F |= FLAG_PV;
}
static inline void alu_xor(uint8_t v){
    A ^= v;
    F = (A & (FLAG_Y|FLAG_X));
    if(A==0) F |= FLAG_Z;
    if(A & 0x80) F |= FLAG_S;
    if(parity8(A)) F |= FLAG_PV;
}
static inline void alu_cp(uint8_t v){
    uint8_t oldA = A;
    alu_sub(v, 0);
    A = oldA;
}

/* INC/DEC on 8-bit value (carry preserved) */
static inline uint8_t inc8(uint8_t x){
    uint8_t r = (uint8_t)(x + 1);
    uint8_t cf = F & FLAG_C;
    F = cf | (r & (FLAG_Y|FLAG_X));
    if(r==0) F |= FLAG_Z;
    if(r & 0x80) F |= FLAG_S;
    if(((x & 0x0F) + 1) & 0x10) F |= FLAG_H;
    if(x == 0x7F) F |= FLAG_PV; /* overflow */
    /* N=0 */
    return r;
}
static inline uint8_t dec8(uint8_t x){
    uint8_t r = (uint8_t)(x - 1);
    uint8_t cf = F & FLAG_C;
    F = cf | FLAG_N | (r & (FLAG_Y|FLAG_X));
    if(r==0) F |= FLAG_Z;
    if(r & 0x80) F |= FLAG_S;
    if(((x & 0x0F) - 1) & 0x10) F |= FLAG_H;
    if(x == 0x80) F |= FLAG_PV; /* overflow */
    return r;
}

/* ---------- CB prefix (bit/shift) on reg or (HL) ---------- */
static uint8_t cb_shift(uint8_t op, uint8_t val){
    /* op: 0..7 = RLC,RRC,RL,RR,SLA,SRA,SLL,SRL */
    uint8_t out = val;
    uint8_t c = 0;
    switch(op){
        case 0: c = (val>>7)&1; out = (uint8_t)((val<<1) | c); break;          /* RLC */
        case 1: c = val & 1;   out = (uint8_t)((val>>1) | (c<<7)); break;      /* RRC */
        case 2: { uint8_t oldc = (F&FLAG_C)?1:0; c = (val>>7)&1; out=(uint8_t)((val<<1)|oldc); } break; /* RL */
        case 3: { uint8_t oldc = (F&FLAG_C)?1:0; c = val&1; out=(uint8_t)((val>>1)|(oldc<<7)); } break; /* RR */
        case 4: c = (val>>7)&1; out = (uint8_t)(val<<1); break;                /* SLA */
        case 5: c = val&1; out = (uint8_t)((val>>1) | (val & 0x80)); break;    /* SRA */
        case 6: c = (val>>7)&1; out = (uint8_t)((val<<1) | 1); break;          /* SLL (undoc) */
        case 7: c = val&1; out = (uint8_t)(val>>1); break;                     /* SRL */
    }
    F = (out & (FLAG_Y|FLAG_X)) | (c?FLAG_C:0);
    if(out==0) F |= FLAG_Z;
    if(out & 0x80) F |= FLAG_S;
    if(parity8(out)) F |= FLAG_PV;
    /* H=0 N=0 */
    return out;
}

static void exec_cb(uint16_t *hl_like, int use_index, uint16_t index_base){
    /* If use_index=0: operate on r/(HL)
       If use_index=1: DDCB/FDCB form already consumed d and will operate on (IX+d)/(IY+d) and possibly store into r */
    uint8_t op = fetch8();
    uint8_t x = op >> 6;           /* 0=shift,1=BIT,2=RES,3=SET */
    uint8_t y = (op >> 3) & 7;     /* bit or shift-op */
    uint8_t z = op & 7;            /* r */

    if(!use_index){
        if(x==0){
            uint8_t v = get_r(z);
            v = cb_shift(y, v);
            set_r(z, v);
        } else if(x==1){
            uint8_t v = get_r(z);
            uint8_t bit = (v >> y) & 1;
            uint8_t cf = F & FLAG_C;
            F = cf | FLAG_H | (v & (FLAG_Y|FLAG_X));
            if(bit==0) F |= FLAG_Z | FLAG_PV;
            if(y==7 && (v&0x80)) F |= FLAG_S;
            /* N=0 */
        } else if(x==2){
            uint8_t v = get_r(z);
            v = (uint8_t)(v & ~(1u<<y));
            set_r(z, v);
        } else {
            uint8_t v = get_r(z);
            v = (uint8_t)(v | (1u<<y));
            set_r(z, v);
        }
        return;
    }

    /* DDCB/FDCB: op is after displacement; memory is (index_base+d) and result may go to reg z */
    /* NOTE: in DDCB/FDCB encoding, z selects destination reg (including (HL) which means write back to memory). */
    /* here hl_like unused */
    /* index_base already includes displacement in caller */
    uint16_t addr = index_base;
    uint8_t v = mem_rd(addr);

    if(x==0){
        uint8_t r = cb_shift(y, v);
        mem_wr(addr, r);
        /* if z != 6 then also load into reg */
        if(z != 6) set_r(z, r);
    } else if(x==1){
        uint8_t bit = (v >> y) & 1;
        uint8_t cf = F & FLAG_C;
        F = cf | FLAG_H | (v & (FLAG_Y|FLAG_X));
        if(bit==0) F |= FLAG_Z | FLAG_PV;
        if(y==7 && (v&0x80)) F |= FLAG_S;
    } else if(x==2){
        uint8_t r = (uint8_t)(v & ~(1u<<y));
        mem_wr(addr, r);
        if(z != 6) set_r(z, r);
    } else {
        uint8_t r = (uint8_t)(v | (1u<<y));
        mem_wr(addr, r);
        if(z != 6) set_r(z, r);
    }
}

/* ---------- ED prefix (subset: enough to "網羅の骨格") ---------- */
static int exec_ed(void){
    uint8_t op = fetch8();
    switch(op){
        /* IN r,(C) */
        case 0x40: case 0x48: case 0x50: case 0x58:
        case 0x60: case 0x68: case 0x70: case 0x78:
            /* I/O未実装: 0返す */
        {
            uint8_t r = (op>>3)&7;
            uint8_t v = 0;
            if(r==6) { /* IN (C) uses F from value, but no reg */
                set_SZP(v);
            } else {
                set_r(r, v);
                set_SZP(v);
            }
            F &= ~FLAG_N;
            F &= ~FLAG_H;
            return 0;
        }

        /* OUT (C),r */
        case 0x41: case 0x49: case 0x51: case 0x59:
        case 0x61: case 0x69: case 0x71: case 0x79:
            /* I/O未実装: 捨てる */
            return 0;

        /* SBC HL,rr / ADC HL,rr */
        case 0x42: case 0x52: case 0x62: case 0x72: /* SBC HL,BC/DE/HL/SP */
        case 0x4A: case 0x5A: case 0x6A: case 0x7A: /* ADC HL,BC/DE/HL/SP */
        {
            uint16_t rr;
            switch((op>>4)&3){
                case 0: rr = BC(); break;
                case 1: rr = DE(); break;
                case 2: rr = HL(); break;
                default: rr = SP; break;
            }
            uint32_t hl = HL();
            uint32_t c = (F & FLAG_C) ? 1 : 0;
            uint32_t r;
            if((op & 0x08)==0){ /* SBC */
                r = hl - rr - c;
                uint16_t res = (uint16_t)r;
                SET_HL(res);
                F = FLAG_N;
                if(res==0) F |= FLAG_Z;
                if(res & 0x8000) F |= FLAG_S;
                if(r & 0x10000) F |= FLAG_C;
                /* H/PVは厳密でなくてもOKならここまで */
            } else { /* ADC */
                r = hl + rr + c;
                uint16_t res = (uint16_t)r;
                SET_HL(res);
                F = 0;
                if(res==0) F |= FLAG_Z;
                if(res & 0x8000) F |= FLAG_S;
                if(r & 0x10000) F |= FLAG_C;
            }
            return 0;
        }

        /* LD (nn),rr / LD rr,(nn) */
        case 0x43: case 0x53: case 0x63: case 0x73: /* (nn)<=BC/DE/HL/SP */
        case 0x4B: case 0x5B: case 0x6B: case 0x7B: /* rr<=(nn) */
        {
            uint16_t nn = fetch16();
            uint16_t *rrp = NULL;
            uint16_t tmp;
            switch((op>>4)&3){
                case 0: tmp = BC(); break;
                case 1: tmp = DE(); break;
                case 2: tmp = HL(); break;
                default: tmp = SP; break;
            }
            if((op & 0x08)==0){
                mem_wr(nn, (uint8_t)tmp);
                mem_wr((uint16_t)(nn+1), (uint8_t)(tmp>>8));
            } else {
                uint8_t lo = mem_rd(nn);
                uint8_t hi = mem_rd((uint16_t)(nn+1));
                uint16_t v = (uint16_t)(lo | (hi<<8));
                switch((op>>4)&3){
                    case 0: SET_BC(v); break;
                    case 1: SET_DE(v); break;
                    case 2: SET_HL(v); break;
                    default: SP = v; break;
                }
            }
            (void)rrp;
            return 0;
        }

        /* NEG (multiple opcodes are aliases) */
        case 0x44: case 0x4C: case 0x54: case 0x5C:
        case 0x64: case 0x6C: case 0x74: case 0x7C:
        {
            uint8_t v = A;
            A = 0;
            alu_sub(v, 0);
            return 0;
        }

        /* RETN/RETI */
        case 0x45: case 0x4D:
            PC = pop16();
            return 0;

        /* IM 0/1/2 (簡易：保持だけ) */
        case 0x46: case 0x56: case 0x5E:
            return 0;

        /* RRD/RLD（厳密じゃない簡易版でも形はいる） */
        case 0x67: /* RRD */
        {
            uint8_t m = mem_rd(HL());
            uint8_t a_lo = A & 0x0F;
            uint8_t m_hi = (m & 0xF0)>>4;
            uint8_t m_lo = (m & 0x0F);
            A = (A & 0xF0) | m_lo;
            mem_wr(HL(), (uint8_t)((a_lo<<4) | m_hi));
            set_SZP(A);
            F &= ~FLAG_H; F &= ~FLAG_N;
            return 0;
        }
        case 0x6F: /* RLD */
        {
            uint8_t m = mem_rd(HL());
            uint8_t a_lo = A & 0x0F;
            uint8_t m_hi = (m & 0xF0)>>4;
            uint8_t m_lo = (m & 0x0F);
            A = (A & 0xF0) | m_hi;
            mem_wr(HL(), (uint8_t)((m_lo<<4) | a_lo));
            set_SZP(A);
            F &= ~FLAG_H; F &= ~FLAG_N;
            return 0;
        }

        /* LDI/LDD/LDIR/LDDR/CP* / IN* / OUT* は「網羅」したければここに追加 */
        default:
            /* EDの未実装はNOP扱いにすると動くものは増える */
            /* ただしデバッグしたいなら unknown にして止めるのもアリ */
            return -1;
    }
}

/* ---------- core step (handles prefixes) ---------- */
static int halted = 0;

static int exec_one(uint8_t prefix, int have_prefix){
    /* have_prefix: 0=normal, 1=DD or FD already read
       prefix: 0=none, 0xDD=IX, 0xFD=IY
    */
    uint16_t *HL_LIKE = NULL;
    uint16_t INDEX_BASE = 0;
    int use_index = 0;

    if(have_prefix){
        use_index = 1;
        if(prefix==0xDD){ HL_LIKE = &IX; INDEX_BASE = IX; }
        else { HL_LIKE = &IY; INDEX_BASE = IY; }
    }

    uint8_t op = fetch8();

    /* handle double prefixes and ED/CB */
    if(op==0xDD || op==0xFD){
        /* repeated prefix: last wins */
        return exec_one(op, 1);
    }
    if(op==0xED){
        return exec_ed();
    }
    if(op==0xCB){
        if(!use_index){
            exec_cb(HL_LIKE, 0, 0);
            return 0;
        } else {
            /* DD CB d op / FD CB d op */
            int8_t d = (int8_t)fetch8();
            uint16_t addr = (uint16_t)(INDEX_BASE + d);
            exec_cb(HL_LIKE, 1, addr);
            return 0;
        }
    }

    /* If DD/FD: (HL) and H/L refer to IXH/IXL? ここは簡略：H/Lはそのまま、(HL)だけ差し替えたい */
    /* 実務的には IXH/IXL の LD/INC/DEC も必要だが、まずは (HL)->(IX+d) 置換を優先 */

    /* group decode */
    if(op == 0x76){ /* HALT */
        halted = 1;
        return 0;
    }

    /* -------- LD r,r' (01 r r') and HALT already handled -------- */
    if((op & 0xC0) == 0x40){
        uint8_t dst = (op>>3)&7;
        uint8_t src = op&7;
        if(use_index && (dst==6 || src==6)){
            /* (HL) becomes (IX+d)/(IY+d) with displacement */
            int8_t d = (int8_t)fetch8();
            uint16_t addr = (uint16_t)(INDEX_BASE + d);
            uint8_t v;
            if(src==6) v = mem_rd(addr);
            else v = get_r(src);
            if(dst==6) mem_wr(addr, v);
            else set_r(dst, v);
            return 0;
        }
        uint8_t v = get_r(src);
        set_r(dst, v);
        return 0;
    }

    /* -------- ALU A,r (10 op r) -------- */
    if((op & 0xC0) == 0x80){
        uint8_t y = (op>>3)&7;
        uint8_t r = op&7;
        uint8_t v;
        if(use_index && r==6){
            int8_t d = (int8_t)fetch8();
            v = mem_ixd_rd(INDEX_BASE, d);
        } else {
            v = get_r(r);
        }
        switch(y){
            case 0: alu_add(v,0); break; /* ADD */
            case 1: alu_add(v,1); break; /* ADC */
            case 2: alu_sub(v,0); break; /* SUB */
            case 3: alu_sub(v,1); break; /* SBC */
            case 4: alu_and(v); break;   /* AND */
            case 5: alu_xor(v); break;   /* XOR */
            case 6: alu_or(v); break;    /* OR */
            case 7: alu_cp(v); break;    /* CP */
        }
        return 0;
    }

    /* -------- remaining: 00xxxxxx and 11xxxxxx -------- */
    switch(op){

        /* NOP */
        case 0x00: return 0;

        /* EX AF,AF' */
        case 0x08: { uint8_t t=A; A=A2; A2=t; t=F; F=F2; F2=t; } return 0;

        /* DJNZ e */
        case 0x10: {
            int8_t e = (int8_t)fetch8();
            B = (uint8_t)(B - 1);
            if(B != 0) PC = (uint16_t)(PC + e);
        } return 0;

        /* JR e / JR cc,e */
        case 0x18: { int8_t e=(int8_t)fetch8(); PC = (uint16_t)(PC + e); } return 0;
        case 0x20: { int8_t e=(int8_t)fetch8(); if(!(F&FLAG_Z)) PC=(uint16_t)(PC+e);} return 0; /* NZ */
        case 0x28: { int8_t e=(int8_t)fetch8(); if( (F&FLAG_Z)) PC=(uint16_t)(PC+e);} return 0; /* Z */
        case 0x30: { int8_t e=(int8_t)fetch8(); if(!(F&FLAG_C)) PC=(uint16_t)(PC+e);} return 0; /* NC */
        case 0x38: { int8_t e=(int8_t)fetch8(); if( (F&FLAG_C)) PC=(uint16_t)(PC+e);} return 0; /* C */

        /* 16-bit LD rp,nn */
        case 0x01: SET_BC(fetch16()); return 0;
        case 0x11: SET_DE(fetch16()); return 0;
        case 0x21:
            if(use_index) { *HL_LIKE = fetch16(); }
            else SET_HL(fetch16());
            return 0;
        case 0x31: SP = fetch16(); return 0;

        /* LD (BC),(DE) -> A ; LD A,(BC)/(DE) */
        case 0x0A: A = mem_rd(BC()); return 0;
        case 0x1A: A = mem_rd(DE()); return 0;

        /* LD (BC)/(DE),A */
        case 0x02: mem_wr(BC(), A); return 0;
        case 0x12: mem_wr(DE(), A); return 0;

        /* LD (nn),HL / LD HL,(nn) (and IX/IY via prefix) */
        case 0x22: {
            uint16_t nn = fetch16();
            uint16_t vv = use_index ? *HL_LIKE : HL();
            mem_wr(nn, (uint8_t)vv);
            mem_wr((uint16_t)(nn+1), (uint8_t)(vv>>8));
        } return 0;
        case 0x2A: {
            uint16_t nn = fetch16();
            uint16_t vv = (uint16_t)(mem_rd(nn) | (mem_rd((uint16_t)(nn+1))<<8));
            if(use_index) *HL_LIKE = vv; else SET_HL(vv);
        } return 0;

        /* LD (nn),A / LD A,(nn) */
        case 0x32: { uint16_t nn=fetch16(); mem_wr(nn, A); } return 0;
        case 0x3A: { uint16_t nn=fetch16(); A = mem_rd(nn); } return 0;

        /* INC/DEC rp */
        case 0x03: SET_BC((uint16_t)(BC()+1)); return 0;
        case 0x13: SET_DE((uint16_t)(DE()+1)); return 0;
        case 0x23:
            if(use_index) *HL_LIKE = (uint16_t)(*HL_LIKE + 1);
            else SET_HL((uint16_t)(HL()+1));
            return 0;
        case 0x33: SP = (uint16_t)(SP+1); return 0;

        case 0x0B: SET_BC((uint16_t)(BC()-1)); return 0;
        case 0x1B: SET_DE((uint16_t)(DE()-1)); return 0;
        case 0x2B:
            if(use_index) *HL_LIKE = (uint16_t)(*HL_LIKE - 1);
            else SET_HL((uint16_t)(HL()-1));
            return 0;
        case 0x3B: SP = (uint16_t)(SP-1); return 0;

        /* ADD HL,rr (IX/IY if prefix on 0x29 only; 0x09/19/39 still add to HL-like in prefix context for practical use) */
        case 0x09: { uint32_t r=(uint32_t)( (use_index?*HL_LIKE:HL()) + BC()); if(use_index)*HL_LIKE=(uint16_t)r; else SET_HL((uint16_t)r); if(r&0x10000) F=(F&~FLAG_C)|FLAG_C; else F&=~FLAG_C; F&=~FLAG_N; } return 0;
        case 0x19: { uint32_t r=(uint32_t)( (use_index?*HL_LIKE:HL()) + DE()); if(use_index)*HL_LIKE=(uint16_t)r; else SET_HL((uint16_t)r); if(r&0x10000) F=(F&~FLAG_C)|FLAG_C; else F&=~FLAG_C; F&=~FLAG_N; } return 0;
        case 0x29: { uint16_t rr = use_index ? *HL_LIKE : HL(); uint32_t r=(uint32_t)rr + rr; if(use_index)*HL_LIKE=(uint16_t)r; else SET_HL((uint16_t)r); if(r&0x10000) F=(F&~FLAG_C)|FLAG_C; else F&=~FLAG_C; F&=~FLAG_N; } return 0;
        case 0x39: { uint32_t r=(uint32_t)( (use_index?*HL_LIKE:HL()) + SP); if(use_index)*HL_LIKE=(uint16_t)r; else SET_HL((uint16_t)r); if(r&0x10000) F=(F&~FLAG_C)|FLAG_C; else F&=~FLAG_C; F&=~FLAG_N; } return 0;

        /* INC/DEC r (including (HL) or (IX+d)/(IY+d)) */
        case 0x04: B=inc8(B); return 0;
        case 0x0C: C=inc8(C); return 0;
        case 0x14: D=inc8(D); return 0;
        case 0x1C: E=inc8(E); return 0;
        case 0x24: H=inc8(H); return 0;
        case 0x2C: L=inc8(L); return 0;
        case 0x3C: A=inc8(A); return 0;
        case 0x34:
            if(use_index){
                int8_t d=(int8_t)fetch8();
                uint8_t v=mem_ixd_rd(INDEX_BASE,d);
                v=inc8(v);
                mem_ixd_wr(INDEX_BASE,d,v);
            } else {
                uint16_t a=HL(); uint8_t v=mem_rd(a); v=inc8(v); mem_wr(a,v);
            }
            return 0;

        case 0x05: B=dec8(B); return 0;
        case 0x0D: C=dec8(C); return 0;
        case 0x15: D=dec8(D); return 0;
        case 0x1D: E=dec8(E); return 0;
        case 0x25: H=dec8(H); return 0;
        case 0x2D: L=dec8(L); return 0;
        case 0x3D: A=dec8(A); return 0;
        case 0x35:
            if(use_index){
                int8_t d=(int8_t)fetch8();
                uint8_t v=mem_ixd_rd(INDEX_BASE,d);
                v=dec8(v);
                mem_ixd_wr(INDEX_BASE,d,v);
            } else {
                uint16_t a=HL(); uint8_t v=mem_rd(a); v=dec8(v); mem_wr(a,v);
            }
            return 0;

        /* LD r,n (including (HL)/(IX+d)/(IY+d)) */
        case 0x06: B=fetch8(); return 0;
        case 0x0E: C=fetch8(); return 0;
        case 0x16: D=fetch8(); return 0;
        case 0x1E: E=fetch8(); return 0;
        case 0x26: H=fetch8(); return 0;
        case 0x2E: L=fetch8(); return 0;
        case 0x3E: A=fetch8(); return 0;
        case 0x36:
            if(use_index){
                int8_t d=(int8_t)fetch8();
                uint8_t n=fetch8();
                mem_ixd_wr(INDEX_BASE,d,n);
            } else {
                uint8_t n=fetch8();
                mem_wr(HL(), n);
            }
            return 0;

        /* POP rr */
        case 0xC1: { uint16_t v=pop16(); SET_BC(v); } return 0;
        case 0xD1: { uint16_t v=pop16(); SET_DE(v); } return 0;
        case 0xE1:
            if(use_index){ *HL_LIKE = pop16(); }
            else { uint16_t v=pop16(); SET_HL(v); }
            return 0;
        case 0xF1: { uint16_t v=pop16(); A=(uint8_t)(v>>8); F=(uint8_t)v; } return 0;

        /* PUSH rr */
        case 0xC5: push16(BC()); return 0;
        case 0xD5: push16(DE()); return 0;
        case 0xE5:
            push16(use_index?*HL_LIKE:HL());
            return 0;
        case 0xF5: push16((uint16_t)((A<<8)|F)); return 0;

        /* JP nn / JP (HL)/(IX)/(IY) */
        case 0xC3: PC = fetch16(); return 0;
        case 0xE9:
            if(use_index) PC = *HL_LIKE;
            else PC = HL();
            return 0;

        /* JP cc,nn */
        case 0xC2: { uint16_t nn=fetch16(); if(!(F&FLAG_Z)) PC=nn; } return 0;
        case 0xCA: { uint16_t nn=fetch16(); if( (F&FLAG_Z)) PC=nn; } return 0;
        case 0xD2: { uint16_t nn=fetch16(); if(!(F&FLAG_C)) PC=nn; } return 0;
        case 0xDA: { uint16_t nn=fetch16(); if( (F&FLAG_C)) PC=nn; } return 0;
        case 0xE2: { uint16_t nn=fetch16(); if(!(F&FLAG_PV)) PC=nn; } return 0;
        case 0xEA: { uint16_t nn=fetch16(); if( (F&FLAG_PV)) PC=nn; } return 0;
        case 0xF2: { uint16_t nn=fetch16(); if(!(F&FLAG_S)) PC=nn; } return 0;
        case 0xFA: { uint16_t nn=fetch16(); if( (F&FLAG_S)) PC=nn; } return 0;

        /* CALL nn / CALL cc,nn */
        case 0xCD: { uint16_t nn=fetch16(); push16(PC); PC=nn; } return 0;
        case 0xC4: { uint16_t nn=fetch16(); if(!(F&FLAG_Z)){ push16(PC); PC=nn; } } return 0;
        case 0xCC: { uint16_t nn=fetch16(); if( (F&FLAG_Z)){ push16(PC); PC=nn; } } return 0;
        case 0xD4: { uint16_t nn=fetch16(); if(!(F&FLAG_C)){ push16(PC); PC=nn; } } return 0;
        case 0xDC: { uint16_t nn=fetch16(); if( (F&FLAG_C)){ push16(PC); PC=nn; } } return 0;
        case 0xE4: { uint16_t nn=fetch16(); if(!(F&FLAG_PV)){ push16(PC); PC=nn; } } return 0;
        case 0xEC: { uint16_t nn=fetch16(); if( (F&FLAG_PV)){ push16(PC); PC=nn; } } return 0;
        case 0xF4: { uint16_t nn=fetch16(); if(!(F&FLAG_S)){ push16(PC); PC=nn; } } return 0;
        case 0xFC: { uint16_t nn=fetch16(); if( (F&FLAG_S)){ push16(PC); PC=nn; } } return 0;

        /* RET / RET cc */
        case 0xC9: PC = pop16(); return 0;
        case 0xC0: if(!(F&FLAG_Z)) PC=pop16(); return 0;
        case 0xC8: if( (F&FLAG_Z)) PC=pop16(); return 0;
        case 0xD0: if(!(F&FLAG_C)) PC=pop16(); return 0;
        case 0xD8: if( (F&FLAG_C)) PC=pop16(); return 0;
        case 0xE0: if(!(F&FLAG_PV)) PC=pop16(); return 0;
        case 0xE8: if( (F&FLAG_PV)) PC=pop16(); return 0;
        case 0xF0: if(!(F&FLAG_S)) PC=pop16(); return 0;
        case 0xF8: if( (F&FLAG_S)) PC=pop16(); return 0;

        /* RST p */
        /* あなたの独自トラップ: 0xEF = Aを2桁hexで出力 */
        case 0xEF:
            printf("%02X", A);
            return 0;
        case 0xC7: push16(PC); PC=0x00; return 0;
        case 0xCF: push16(PC); PC=0x08; return 0;
        case 0xD7: push16(PC); PC=0x10; return 0;
        case 0xDF: push16(PC); PC=0x18; return 0;
        case 0xE7: push16(PC); PC=0x20; return 0;
        //case 0xEF: push16(PC); PC=0x28; return 0;
        case 0xF7: push16(PC); PC=0x30; return 0;
        case 0xFF: push16(PC); PC=0x38; return 0;

        /* DI/EI (割り込み未実装なので状態保持だけにするなら変数追加) */
        case 0xF3: return 0;
        case 0xFB: return 0;

        /* EX (SP),HL / EX DE,HL */
        case 0xEB: { uint16_t t=DE(); SET_DE(HL()); SET_HL(t); } return 0;
        case 0xE3: {
            uint16_t m = (uint16_t)(mem_rd(SP) | (mem_rd((uint16_t)(SP+1))<<8));
            uint16_t rr = use_index ? *HL_LIKE : HL();
            mem_wr(SP, (uint8_t)rr);
            mem_wr((uint16_t)(SP+1), (uint8_t)(rr>>8));
            if(use_index) *HL_LIKE = m; else SET_HL(m);
        } return 0;

        /* EXX */
        case 0xD9: {
            uint8_t t;
            t=B; B=B2; B2=t;
            t=C; C=C2; C2=t;
            t=D; D=D2; D2=t;
            t=E; E=E2; E2=t;
            t=H; H=H2; H2=t;
            t=L; L=L2; L2=t;
        } return 0;

        default:
            return -1;
    }
}

int main(int argc, char *argv[]){
    printf("sim80 ver %01g\n",version);

    if(argc < 2){
        printf("usage: sim80 file.bin\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "rb");
    if(!fp){
        printf("cannot open file: %s\n", argv[1]);
        return 1;
    }
    size_t size = fread(ram, 1, 0x10000, fp);
    fclose(fp);
    printf("loaded %zu bytes\n", size);

    /* init */
    PC = 0;
    SP = 0xFFFE;
    A=F=B=C=D=E=H=L=0;
    IX=IY=0;
    halted = 0;

    while(!halted){
        uint16_t pc0 = PC;
        /* prefix aware */
        uint8_t op = fetch8();
        int rc;
        if(op==0xDD || op==0xFD){
            rc = exec_one(op, 1);
        } else {
            /* normal: "op already consumed" なので戻して exec_one へ渡すのが簡単 */
            PC = pc0;
            rc = exec_one(0, 0);
        }
        if(rc < 0){
            uint8_t bad = mem_rd(pc0);
            printf("unknown/unsupported opcode %02X at %04X\n", bad, pc0);
            printf("A=%02X F=%02X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X PC=%04X SP=%04X\n",
                   A,F,(unsigned)BC(),(unsigned)DE(),(unsigned)HL(),
                   (unsigned)IX,(unsigned)IY,(unsigned)PC,(unsigned)SP);
            return 1;
        }
    }

    printf("\nhalt at %04X\n", (unsigned)(PC-1));
    printf("A=%02X F=%02X BC=%04X DE=%04X HL=%04X IX=%04X IY=%04X PC=%04X SP=%04X\n",
           A,F,(unsigned)BC(),(unsigned)DE(),(unsigned)HL(),
           (unsigned)IX,(unsigned)IY,(unsigned)PC,(unsigned)SP);
    return 0;
}


