/*
 * Z16 Instruction Set Simulator (ISS)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mohamed Shalan
 *
 * This simulator accepts a Z16 binary machine code file (with a .bin extension) and assumes that
 * the first instruction is located at memory address 0x0000. It decodes each 16-bit instruction into a
 * human-readable string and prints it, then executes the instruction by updating registers, memory,
 * or performing I/O via ecall.
 *
 * Supported ecall services:
 *   - ecall 1: Print an integer (value in register a0).
 *   - ecall 5: Print a NULL-terminated string (address in register a0).
 *   - ecall 3: Terminate the simulation.
 *
 * Usage:
 *   z16sim <machine_code_file_name>
 *
 *
 *Things to note:
 *SLL, SRL, SRA are implemented in a way that can shift only up to 15 bits only.
 *regs are declared to be unsigned by default.
 *J instructions are assumed to be all relative.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define MEM_SIZE 65536  // 64KB memory

// Global simulated memory and register file.
unsigned char memory[MEM_SIZE];
int16_t regs[8];       // 8 registers (16-bit each): x0, x1, x2, x3, x4, x5, x6, x7
uint16_t pc = 0;       // Program counter (16-bit)

// Register ABI names for display (x0 = t0, x1 = ra, x2 = sp, x3 = s0, x4 = s1, x5 = t1, x6 = a0, x7 = a1)
const char *regNames[8] = {"t0", "ra", "sp", "s0", "s1", "t1", "a0", "a1"};

// -----------------------
// Disassembly Function
// -----------------------
//
// Decodes a 16-bit instruction 'inst' (fetched at address 'pc') and writes a human‑readable
// string to 'buf' (of size bufSize). This decoder uses the opcode (bits [2:0]) to distinguish
// among R‑, I‑, B‑, L‑, J‑, U‑, and System instructions.

void disassemble(uint16_t inst, uint16_t pc, char *buf, size_t bufSize) {
    uint8_t opcode = inst & 0x7;
    switch(opcode) {
        case 0x0: { // R-type: [15:12] funct4 | [11:9] rs2 | [8:6] rd/rs1 | [5:3] funct3 | [2:0] opcode
            uint8_t funct4 = (inst >> 12) & 0xF;
            uint8_t rs2    = (inst >> 9) & 0x7;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if(funct4 == 0x0 && funct3 == 0x0)
                snprintf(buf, bufSize, "add %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x1 && funct3 == 0x0)
                snprintf(buf, bufSize, "sub %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x0 && funct3 == 0x1)
                snprintf(buf, bufSize, "slt %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x0 && funct3 == 0x2)
                snprintf(buf, bufSize, "sltu %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x2 && funct3 == 0x3)
                snprintf(buf, bufSize, "sll %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x4 && funct3 == 0x3)
                snprintf(buf, bufSize, "srl %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x8 && funct3 == 0x3)
                snprintf(buf, bufSize, "sra %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x1 && funct3 == 0x4)
                snprintf(buf, bufSize, "or %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x0 && funct3 == 0x5)
                snprintf(buf, bufSize, "and %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x0 && funct3 == 0x6)
                snprintf(buf, bufSize, "xor %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x0 && funct3 == 0x7)
                snprintf(buf, bufSize, "mv %s, %s", regNames[rd_rs1], regNames[rs2]);
            else if(funct4 == 0x4 && funct3 == 0x0)
                snprintf(buf, bufSize, "jr 0x%04X", regs[rs2]);
            else if(funct4 == 0x8 && funct3 == 0x0)
                snprintf(buf, bufSize, "jalr %s, %s", regNames[rd_rs1], regNames[rs2]);
            else
                snprintf(buf, bufSize, "Unknown R-type instruction");
            break;
        }
        case 0x1: { // I-type: [15:9] imm[6:0] | [8:6] rd/rs1 | [5:3] funct3 | [2:0] opcode
            uint8_t imm7 = (inst >> 9) & 0x7F;
            uint8_t rd_rs1= (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;
            int16_t simm = (imm7 & 0x40) ? (imm7 | 0xFF80) : imm7;

            if(funct3 == 0x0)
                snprintf(buf, bufSize, "addi %s, %d", regNames[rd_rs1], simm);
            else if(funct3 == 0x1)
                snprintf(buf, bufSize, "slti %s, %d", regNames[rd_rs1], simm);
            else if(funct3 == 0x2)
                snprintf(buf, bufSize, "sltui %s, %u", regNames[rd_rs1], imm7);
            else if(funct3 == 0x3) {
                uint8_t differentiator = (imm7 >> 4) & 0x7;
                uint8_t shamt = imm7 & 0xF;

                if(differentiator == 0x1)
                    snprintf(buf, bufSize, "slli %s, %u", regNames[rd_rs1], shamt);
                else if(differentiator == 0x2)
                    snprintf(buf, bufSize, "srli %s, %u", regNames[rd_rs1], shamt);
                else if(differentiator == 0x4)
                    snprintf(buf, bufSize, "srai %s, %u", regNames[rd_rs1], shamt);
                else
                    snprintf(buf, bufSize, "Unknown shift instruction");
            }
            else if(funct3 == 0x4)
                snprintf(buf, bufSize, "ori %s, %d", regNames[rd_rs1], simm);
            else if(funct3 == 0x5)
                snprintf(buf, bufSize, "andi %s, %d", regNames[rd_rs1], simm);
            else if(funct3 == 0x6)
                snprintf(buf, bufSize, "xori %s, %d", regNames[rd_rs1], simm);
            else if(funct3 == 0x7)
                snprintf(buf, bufSize, "li %s, %d", regNames[rd_rs1], simm);
            else
                snprintf(buf, bufSize, "Unknown I-type instruction");
            break;
        }
        case 0x2: { // B-type (branch): [15:12] offset[4:1] | [11:9] rs2 | [8:6] rs1 | [5:3] funct3 | [2:0] opcode
            int8_t imm = (inst >> 12) & 0xF;
            imm = imm << 1; // The immediate is from bit 1 to bit 4
            uint8_t rs2   = (inst >> 9) & 0x7;
            uint8_t rs1   = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if (imm & 0x10) {  // Check if the 4th bit (MSB in 4-bit) is 1 (negative number)
                imm |= 0xF0;   // Sign extend by setting the higher bits (8 bits in this case) to 1
            }
            int16_t branchTarget = pc + imm;

            if(funct3 == 0x0)
                snprintf(buf, bufSize, "beq %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else if(funct3 == 0x1)
                snprintf(buf, bufSize, "bne %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else if(funct3 == 0x2)
                snprintf(buf, bufSize, "bz %s, 0x%04X", regNames[rs1], branchTarget);
            else if(funct3 == 0x3)
                snprintf(buf, bufSize, "bnz %s, 0x%04X", regNames[rs1], branchTarget);
            else if(funct3 == 0x4)
                snprintf(buf, bufSize, "blt %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else if(funct3 == 0x5)
                snprintf(buf, bufSize, "bge %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else if(funct3 == 0x6)
                snprintf(buf, bufSize, "bltu %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else if(funct3 == 0x7)
                snprintf(buf, bufSize, "bgeu %s, %s, 0x%04X", regNames[rs1], regNames[rs2], branchTarget);
            else
                snprintf(buf, bufSize, "Unknown B-type instruction");
            break;

        }
        case 0x3: { // B-type (store): [15:12] offset[3:0] | [11:9] rs2 | [8:6] rs1 | [5:3] funct3 | [2:0] opcode
            int8_t imm = (inst >> 12) & 0xF;
            uint8_t rs2   = (inst >> 9) & 0x7;
            uint8_t rs1   = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if(funct3 == 0x0)
                snprintf(buf, bufSize, "sb %s, %d(%s)", regNames[rs2], imm, regNames[rs1]);
            else if(funct3 == 0x1)
                snprintf(buf, bufSize, "sw %s, %d(%s)", regNames[rs2], imm, regNames[rs1]);
            else
                snprintf(buf, bufSize, "Unknown store instruction");
            break;
        }
        case 0x4: { // L-type (load): [15:12] offset[3:0] | [11:9] rs2 | [8:6] rd | [5:3] funct3 | [2:0] opcode
            int8_t imm = (inst >> 12) & 0xF;
            uint8_t rs2 = (inst >> 9) & 0x7;
            uint8_t rd  = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;

            if(funct3 == 0x0)
                snprintf(buf, bufSize, "lb %s, %d(%s)", regNames[rd], imm, regNames[rs2]);
            else if(funct3 == 0x1)
                snprintf(buf, bufSize, "lw %s, %d(%s)", regNames[rd], imm, regNames[rs2]);
            else if(funct3 == 0x4)
                snprintf(buf, bufSize, "lbu %s, %d(%s)", regNames[rd], imm, regNames[rs2]);
            else
                snprintf(buf, bufSize, "Unknown load instruction");
            break;
        }
        case 0x5: { // J-type (jump): [15] f | [14:9] offset[9:4] | [8:6] rd | [5:3] offset[3:1] | [2:0] opcode
            uint8_t imm4_9 = (inst >> 9) & 0x3F;
            uint8_t rd     = (inst >> 6) & 0x7;  // rd is not printed
            uint8_t f      = (inst >> 15) & 0x1;
            uint8_t imm1_3 = (inst >> 3) & 0x7;
            int16_t imm = (imm4_9 << 4) | (imm1_3 << 1);
            imm = (imm4_9 & 0x20) ? (imm | 0xFC00) : imm; // Sign extend the immediate
            int16_t branchTarget = pc + imm; // Get the jump target

            if(f == 0) {
                snprintf(buf, bufSize, "j 0x%04X", branchTarget);
                // printf("The branch in here is: ");
                // printf("%d\n",branchTarget);
            }
            else if(f == 1)
                snprintf(buf, bufSize, "jal 0x%04X", branchTarget);
            else
                snprintf(buf, bufSize, "Unknown jump instruction");
            break;
        }
        case 0x6: { // U-type (upper immediate): [15] f | [14:9] Imm[15:10] | [8:6] rd | [5:3] Imm[9:7] | [2:0] opcode
            uint8_t rd     = (inst >> 6) & 0x7;
            uint8_t f      = (inst >> 15) & 0x1;

            uint16_t imm15_10 = (inst >> 9) & 0x3F;
            uint16_t imm9_7   = (inst >> 3) & 0x7;

            uint16_t imm = ((imm15_10 << 3) | imm9_7) << 7;

            if(f == 0)
                snprintf(buf, bufSize, "lui %s, 0x%X", regNames[rd], imm);
            else if(f == 1)
                snprintf(buf, bufSize, "AUIPC %s, 0x%X", regNames[rd], imm);
            else
                snprintf(buf, bufSize, "Unknown U instruction");
            break;
        }
        case 0x7: {
            // U-type (ecall): [15:6] Service | [5:3] funct3 | [2:0] opcode
            uint8_t funct3 = (inst >> 3) & 0x7;
            uint16_t service = (inst >> 6) & 0x3FF;
            snprintf(buf, bufSize, "ecall");

            if(funct3 == 0x0) {
                switch(service) {
                    case 1: {
                        snprintf(buf, bufSize, "ecall 1");
                        break;
                    }
                    case 5: {
                        snprintf(buf, bufSize, "ecall 5");
                        break;
                    }
                    case 3: {
                        snprintf(buf, bufSize, "ecall 3");
                        break;
                    }
                    default: {
                        snprintf(buf, bufSize, "Unknown ecall instruction");
                        break;
                    }
                }
            }
            else {
                snprintf(buf, bufSize, "Unknown ecall instruction");
                break;
            }
            break;
        }
        default: {
            snprintf(buf, bufSize, "unknown opcode");
            break;
        }
    }
}

// -----------------------
// Instruction Execution
// -----------------------
//
// Executes the instruction 'inst' (a 16-bit word) by updating registers, memory, and PC.
// Returns 1 to continue simulation or 0 to terminate (if ecall 3 is executed).
int executeInstruction(uint16_t inst) {
    //anding with 7 to get the last three bits -> opcode
    uint8_t opcode = inst & 0x7;
    // if(memory[pc] == 0) {
    //     printf("memory == 0");
    //     printf("%d/n", pc);
    //     return 0;
    // }
    int pcUpdated = 0; // flag: if instruction updated PC directly
    switch(opcode) {
        case 0x0: { // R-type
            uint8_t funct4 = (inst >> 12) & 0xF;
            uint8_t rs2     = (inst >> 9) & 0x7;
            uint8_t rd_rs1  = (inst >> 6) & 0x7;
            uint8_t funct3  = (inst >> 3) & 0x7;
            if(funct4 == 0x0 && funct3 == 0x0) // add
                regs[rd_rs1] = regs[rd_rs1] + regs[rs2];
            else if(funct4 == 0x1 && funct3 == 0x0) // sub
                regs[rd_rs1] = regs[rd_rs1] - regs[rs2];
            else if(funct4 == 0x0 && funct3 == 0x1) // slt
                regs[rd_rs1] = (regs[rd_rs1] < regs[rs2]) ? 1 : 0;
            else if(funct4 == 0x0 && funct3 == 0x2) // sltu
                regs[rd_rs1] = ((uint16_t)regs[rd_rs1] < (uint16_t)regs[rs2]) ? 1 : 0;
            else if(funct4 == 0x2 && funct3 == 0x3) // sll
                regs[rd_rs1] = regs[rd_rs1] << (regs[rs2]& 0xF); //cause the register can have a very large number whereas shifting can be at most with 16 bits
            else if(funct4 == 0x4 && funct3 == 0x3) // srl
                regs[rd_rs1] = (uint16_t)regs[rd_rs1] >> (regs[rs2] & 0xF);
            else if(funct4 == 0x8 && funct3 == 0x3) // sra
                regs[rd_rs1] = regs[rd_rs1] >> (regs[rs2] & 0xF);
            else if(funct4== 0x1 && funct3 == 0x4) //OR
                regs[rd_rs1] = regs[rd_rs1] | regs[rs2];
            else if(funct4 == 0x0 && funct3 == 0x5) // and
                regs[rd_rs1] =regs[rd_rs1] & regs[rs2];
            else if(funct4 == 0x0 && funct3 == 0x6) // xor
                regs[rd_rs1] = regs[rd_rs1] ^ regs[rs2];
            else if(funct4 == 0x0 && funct3 == 0x7) // mv
                regs[rd_rs1] = regs[rs2];
            else if(funct4 == 0x4 && funct3 == 0x0) // jr
            {
                pc = regs[rs2];
                pcUpdated=1;
            }
            else if(funct4 == 0x8 && funct3 == 0x0) // jalr
            {
                regs[rd_rs1] = pc + 2;  //this saves the return address
                pc = regs[rs2]; //moves to the target address
                pcUpdated=1;
            }
            break;
        }
        case 0x1: { // I-type
            uint8_t imm7   = (inst >> 9) & 0x7F;
            uint8_t rd_rs1 = (inst >> 6) & 0x7;
            uint8_t funct3 = (inst >> 3) & 0x7;
            int16_t simm = (imm7 & 0x40) ? (imm7 | 0xFF80) : imm7;
            // your code goes here
            if(funct3==0x0) //addi
                regs[rd_rs1] += simm;
            else if(funct3 == 0x1)//slti signed imm
                regs[rd_rs1] = ((int16_t)regs[rd_rs1] < simm) ? 1 : 0;
            else if(funct3 == 0x2) //sltui unsigned imm
                regs[rd_rs1] = ((int16_t)regs[rd_rs1] < imm7) ? 1 : 0;
            else if(funct3 == 0x3) //slli/srli/srai
            {
                uint8_t differentiator = (imm7>>4)& 0x7;
                uint8_t shamt = (imm7)& 0xF;
                if(differentiator==0x1) {
                    regs[rd_rs1] = regs[rd_rs1] << shamt;
                }
                else if(differentiator==0x2) {
                    regs[rd_rs1] = regs[rd_rs1] >> shamt;
                }
                else if(differentiator==0x4) {
                    regs[rd_rs1] = (int16_t)regs[rd_rs1] >> shamt;
                }
            }
            else if(funct3 == 0x4) //ori
                regs[rd_rs1] = regs[rd_rs1] | simm;
            else if(funct3 == 0x5) //andi
                regs[rd_rs1] = regs[rd_rs1] & simm;
            else if(funct3 == 0x6) //xori
                regs[rd_rs1] = regs[rd_rs1] ^ simm;
            else if(funct3 == 0x7) //li
                regs[rd_rs1] = simm;

            break;
        }
        case 0x2: { // B-type (branch)
            int8_t imm = (inst >> 12) & 0xF; //the imm is signed
            imm = imm<<1; //the imm is from bit 1 to bit 4
            if (imm & 0x10) {  // Check if the 4th bit (MSB in 4-bit) is 1 (negative number)
                imm |= 0xF0;   // Sign extend by setting the higher bits (8 bits in this case) to 1
            }
            uint8_t rs2     = (inst >> 9) & 0x7;
            uint8_t rs1  = (inst >> 6) & 0x7;
            uint8_t funct3  = (inst >> 3) & 0x7;
            if(funct3==0x0 && regs[rs1] == regs[rs2])//beq
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x1 && regs[rs1] != regs[rs2])//bne
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x2 && regs[rs1] == 0)//bz
            {
                pcUpdated=1;
                printf("The immediate is: ");
                printf("%d\n", imm);
                pc+=imm;
            }
            else if(funct3==0x3 && regs[rs1] != 0)//bnz
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x4 && (int16_t)regs[rs1] < (int16_t)regs[rs2])//blt
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x5 && (int16_t)regs[rs1] >= (int16_t)regs[rs2])//bge
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x6 && (uint16_t)regs[rs1] < (uint16_t)regs[rs2])//bltu
            {
                pcUpdated=1;
                pc+=imm;
            }
            else if(funct3==0x7 && (uint16_t)regs[rs1] >= (uint16_t)regs[rs2])//bgeu
            {
                pcUpdated=1;
                pc+=imm;
            }
            break;
        }
        case 0x3: { // store
            // your code goes here
            int8_t imm = (inst >> 12) & 0xF; //the imm is signed
            uint8_t rs2     = (inst >> 9) & 0x7;
            uint8_t rs1  = (inst >> 6) & 0x7;
            uint8_t funct3  = (inst >> 3) & 0x7;
            if(funct3==0x0)
                memory[regs[rs1] + imm] = (uint8_t)(regs[rs2]); //stores only the first 8 bits
            else if(funct3 == 0x1)
                memory[regs[rs1] + imm] = (regs[rs2]);
            break;
        }
        case 0x4: { // L-type (load)
            // your code goes here
            int8_t imm = (inst >> 12) & 0xF;
            uint8_t rs2     = (inst >> 9) & 0x7;
            uint8_t rd  = (inst >> 6) & 0x7;
            uint8_t funct3  = (inst >> 3) & 0x7;

            if(funct3==0x0)
                regs[rd] = (int8_t)(memory[regs[rs2] + imm]);
            else if(funct3 == 0x1)
                regs[rd] = (int8_t)(memory[regs[rs2] + imm]);
            else if(funct3 == 0x4)
                regs[rd] = (uint8_t)memory[(regs[rs2] + imm)]; //the memory is unsigned by default
            break;
        }
        //note that there is a typo in the instructions table on github, I[8:4] should be 6 bits instead
        case 0x5: { // J-type (jump)

            uint8_t imm4_9 = (inst >> 9) & 0x3F;
            uint8_t rd     = (inst >> 6) & 0x7;  // rd is not printed
            uint8_t f      = (inst >> 15) & 0x1;
            uint8_t imm1_3 = (inst >> 3) & 0x7;
            int16_t imm = (imm4_9 << 4) | (imm1_3 << 1);
            imm = (imm4_9 & 0x20) ? (imm | 0xFC00) : imm; // Sign extend the immediate
            int16_t branchTarget = pc + imm; // Get the jump target


            // your code goes here
            // uint8_t imm4_9  = (inst >> 9) & 0x3F;
            //
            // uint8_t rd  = (inst >> 6) & 0x7;
            // int f = inst >> 15;
            // uint8_t imm1_3  = (inst >> 3) & 0x7;
            // int16_t imm=imm4_9<<4;
            // imm=imm+(imm1_3<<1);
            if(f==1) { //jal
                pcUpdated=1;
                regs[1]=pc+2; //saves the return address in x1
                pc=branchTarget;
            }
            else if(f==0) { //j
                pcUpdated=1;
                // printf("J executes, with target address: ");
                // printf("%d\n",branchTarget);
                // printf( "j 0x%04X", branchTarget);
                // printf("The branch in here 2 is: ");
                // printf("%d\n",branchTarget);
                pc=branchTarget;
            }
            break;
        }
        case 0x6: { // U-type
            uint8_t imm15_10 = (inst >> 9) & 0x3F;
            uint8_t rd     = (inst >> 6) & 0x7;
            uint8_t f      = (inst >> 15) & 0x1;
            uint8_t imm9_7 = (inst >> 3) & 0x7;
            int16_t imm = (imm15_10 << 3) | (imm9_7);

            if (f == 0) { // LUI (Load Upper Immediate)
                    int16_t extended_imm = imm << 7; // Shift by 7

                    // If the sign bit is set (negative number), sign-extend
                    // if (imm & 0x40) { // Check if the 7th bit (sign bit in 7-bit immediate) is set
                    //     extended_imm |= 0xFFFFF000;  // Sign-extend by setting the upper 20 bits
                    // }

                    // Store the result in the register
                    regs[rd] = extended_imm;

            }
            else if (f == 1) { // AUIPC
                regs[rd] = pc + (imm << 7);
            }
            break;
        }
        case 0x7: { // System instruction (ecall)
            uint8_t funct3 = (inst >> 3) & 0x7;
            uint16_t service = (inst >> 6) & 0x3FF;

            if (funct3 == 0) { // ecall

                if (service == 0x1) { // Print integer
                    printf("%d\n", regs[6]);
                }
                else if (service == 0x5) { // Print string
                    // // Check that regs[6] is a valid address in memory
                    // if (regs[6] < 0 || regs[6] >= MEM_SIZE) {
                    //     printf("Invalid memory address.\n");
                    //     return 0; // Or handle error appropriately
                    // }
                    //
                    // // Manually set a string in memory
                    // // memcpy(&memory[regs[6]], "abcdefghi", 9);  // "abcdefghi" + null terminator
                    //
                    // // Fetch and print the string as usual
                    // char *str = (char*)&memory[regs[6]];  // Fetch the string address
                    //
                    // // // Check if the string is null-terminated and print each byte
                    // // for (int i = 0; ; i++) {
                    // //     printf("Byte %d: 0x%02x, Char: '%c'\n", i, str[i], str[i]);
                    // //     if (str[i] == '\0') break;  // Break if null terminator is found
                    // // }
                    //
                    // printf("\n");
                    //
                    // // Print the string using %s if it's correctly null-terminated
                    // printf("String: %s\n", str);

                    // Check that regs[6] (a0) is a valid address in memory
                    if (regs[6] < 0 || regs[6] >= MEM_SIZE) {
                        printf("Invalid memory address.\n");
                        return 0;
                    }

                    // print the string character by character, but skip the first character "
                    uint16_t addr = regs[6];

                    if (memory[addr] == '"') {
                        addr++;
                    }

                    // Print the rest of the string
                    while (addr < MEM_SIZE && memory[addr] != '\0') {
                        printf("%c", memory[addr]);
                        addr++;
                    }
                    printf("\n");

                }

                else if (service == 3) { // Terminate simulation
                    printf("Simulation terminated.\n");
                    exit(0);
                }
                else {
                    printf("Unknown ecall: %d\n", service);
                }
            }
            break;
        }
        default: {
            printf("Unknown instruction opcode 0x%X\n", opcode);
            return 0;
        }
    }
    if(!pcUpdated)
        pc += 2; // default: move to next instruction
    return 1;
}

// -----------------------
// Memory Loading
// -----------------------
//
// Loads the binary machine code image from the specified file into simulated memory.
void loadMemoryFromFile(const char *filename) {
    //rb -> read binary mode
    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        perror("Error opening binary file");
        exit(1);
    }
    //fread -> reads from the file byte by byte and stores the read data into the memory
    //It also ensures that the read data doesn't exceed the actual memory size. i.e. it reads up to 65536 bytes only.
    // Finally, it stores the no of read bytes in 'n'
    size_t n = fread(memory, 1, MEM_SIZE, fp);
    fclose(fp);
    printf("Loaded %zu bytes into memory\n", n);
}

// -----------------------
// Main Simulation Loop
// -----------------------
int main(int argc, char **argv) {
    printf("main called");
    //This if condition checks whether the machine code file is actually passed as an argument or not
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <machine_code_file>\n", argv[0]);
        exit(1);
    }
    loadMemoryFromFile(argv[1]);
    //memset is a functino that sets a block of memory to a specific value
    memset(regs, 0, sizeof(regs)); // initialize registers to 0
    pc = 0;  // starting at address 0
    char disasmBuf[128];
    while(pc < MEM_SIZE) {
        // Fetch a 16-bit instruction from memory (little-endian)
        uint16_t inst = memory[pc] | (memory[pc+1] << 8);
        disassemble(inst, pc, disasmBuf, sizeof(disasmBuf));
        printf("0x%04X: %04X    %s\n", pc, inst, disasmBuf);
        if(!executeInstruction(inst)) {
            break;
        }
        // Terminate if PC goes out of bounds
        if(pc >= MEM_SIZE) break;
    }
    return 0;
}
