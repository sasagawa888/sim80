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

test.asm

ORG 0
LD A,1
HALT


Run:

./asm80 test.asm test.bin
./sim80 test.bin

Current Features
SIM80

Loads raw .bin into ram[0x10000]

Execution starts at PC = 0

Implemented instructions (minimal set):

LD A,n (3E nn)

HALT (76)

ASM80

Simple one-line instruction format

No labels (yet)

Implemented directives/instructions:

ORG addr

DB n[,n...]

LD A,n

HALT

Purpose

Revisit the Z80 at an instruction-level understanding

Build up the instruction set gradually

Enjoy the simplicity of classic microcomputer architecture

Have fun with Z80 🙂