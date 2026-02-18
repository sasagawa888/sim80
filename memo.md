
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

