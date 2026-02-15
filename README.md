# SIM80 / ASM80

A small project to enjoy the nostalgic **Z80** once again.  
This repository contains:

- **SIM80** – a minimal Z80 simulator  
- **ASM80** – a minimal Z80 assembler  

The simulator loads a raw `.bin` file into memory and executes instructions step by step.  
The assembler converts simple `.asm` source files into raw binary.

---

## Project Structure

- `sim80.c` – Z80 instruction-level simulator  
- `asm80.c` – Simple Z80 assembler (`.asm` → raw `.bin`)  
- `Makefile` – Build configuration  

---

## Build

```sh
make


This will generate:

sim80

asm80

Clean build artifacts:

make clean

Usage
Assemble (.asm → .bin)
./asm80 test.asm test.bin

Run (.bin → execute in SIM80)
./sim80 test.bin

Example

sim.asm
```
;test sim80
        LD   A,0xFF
LOOP:
        DEC  A
        RST 0x28
        JP   NZ,LOOP
        HALT
;end
```

Run:

asm80 sim.asm
sim80 sim.bin

sim80 sim.bin
loaded 8 bytes
FEFDFCFBFAF9F8F7F6F5F4F3F2F1F0EFEEEDECEBEAE9E8E7E6E5E4E3E2E1E0DFDEDDDCDBDAD9D8D7D6D5D4D3D2D1D0CFCECDCCCBCAC9C8C7C6C5C4C3C2C1C0BFBEBDBCBBBAB9B8B7B6B5B4B3B2B1B0AFAEADACABAAA9A8A7A6A5A4A3A2A1A09F9E9D9C9B9A999897969594939291908F8E8D8C8B8A898887868584838281807F7E7D7C7B7A797877767574737271706F6E6D6C6B6A696867666564636261605F5E5D5C5B5A595857565554535251504F4E4D4C4B4A494847464544434241403F3E3D3C3B3A393837363534333231302F2E2D2C2B2A292827262524232221201F1E1D1C1B1A191817161514131211100F0E0D0C0B0A09080706050403020100halt 76 at 0007
A=0 BC=0 DE=0 HL=0 PC=8 SP=0

Purpose:

Revisit the Z80 at an instruction-level understanding

Build up the instruction set gradually

Enjoy the simplicity of classic microcomputer architecture

Have fun with Z80 🙂