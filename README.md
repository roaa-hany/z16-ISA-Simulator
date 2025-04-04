# z16 ISA Simulator 
## Project Overview
This project is a simulator for the Z16 instruction set architecture (ISA). The simulator emulates the execution of Z16 assembly instructions, allowing users to test and analyze Z16 programs without running them on actual hardware.

## Build Instructions
To build the simulator, follow these steps:
1. Clone the repository:
``` bash
git clone https://github.com/CSCE-2303/csce-2303-s25-project-1-triple-byte.git

```

2. Head to the location where the repo was cloned. 

3. Compile the source code:

```bash
gcc -o z16_simulator z16sim.c
```

## Usage Guidelines

After building the simulator, run it with:

```bash
./z16_simulator <input_file.bin>
```

<input_file.bin> should be a file containing Z16 assembly binary instructions.

The simulator will then process the instructions.

## Design Overview

Our Z16 simulator is designed to emulate the execution of a simple 16-bit instruction set architecture (ISA). The simulator consists of multiple components that interact to decode, execute, and update the program counter (PC) and registers based on a series of instructions.

### Key Components:

#### Instruction Fetching and Decoding:

- The simulator fetches a 16-bit instruction from memory at the address pointed to by the program counter (pc).

- The instruction is then decoded to extract the opcode, and other instruction-specific fields (like funct3, funct4, register operands, and immediate values).

#### Program Counter (PC) Management:

- After each instruction is executed, the program counter (pc) is typically incremented by 2 (to point to the next instruction).

- Special instructions such as jal, j, jalr, jr, and branches modify the pc to jump to a different memory location based on the instruction's behavior.

#### Memory Management:

- The simulator utilizes a simple memory model where instructions and data are stored.

- The memory array is used for both storing program data (for load and store instructions) and string data for printing operations during system calls.

- String printing in system calls is performed by fetching the string from memory and printing it byte by byte, checking for the null terminator.

#### Error Handling and Debugging:

- Memory Access Errors: The simulator ensures that memory accesses are valid by checking the bounds of memory accesses (especially for load/store operations).

- Instruction Decoding Errors: If an unknown or unsupported opcode is encountered, the simulator prints an error message and terminates the simulation.

- System Call Errors: For unsupported or incorrect system calls, the simulator handles errors appropriately (e.g., invalid memory address for string printing).


#### Execution Flow:

- The instruction is fetched from memory at the current pc address.

- The instruction is decoded to extract the opcode and other necessary components (registers, immediate values).

- Based on the opcode, the corresponding execution logic is triggered.

    - If the instruction modifies the pc (e.g., jumps or branches), the pc is updated accordingly.

    - If the instruction involves reading/writing to memory, the memory array is updated or accessed.

- After execution, the pc is updated by default to point to the next instruction, unless a jump/branch modifies it.

- If an ecall is encountered, the simulator performs the requested system operation, such as printing data or terminating the simulation.

- The simulator continues to execute instructions until a system call to terminate (ecall 3) is invoked.





