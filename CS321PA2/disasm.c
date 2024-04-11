#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <endian.h>

typedef union {
	uint32_t i;
	float f;
} intfloat;

typedef struct {
	char mnemonic[10];
	void (*function)();
	uint32_t opcode;
} instruction_t;


// declare functions
void decode_instruction(intfloat inp_inst);

instruction_t find_instruction(intfloat opcode);

void r_format(intfloat inp_inst, instruction_t instr);
void i_format(intfloat inp_inst, instruction_t instr);

void float_bits(intfloat i);

void get_format(intfloat i);

void ADD_inst(intfloat inp_inst, instruction_t instr);
void ADDI_inst(intfloat inp_inst, instruction_t instr);
void ADDIS_inst();

instruction_t instruction[] = {
	{ "ADD",     ADD_inst,    0b10001011000 },
  	{ "ADDI",    ADDI_inst,   0b1001000100  },
  	{ "ADDIS",   ADDIS_inst,  0b1011000100  },
};

int main(int argc, char *argv[]) {
	int fd;
	struct stat buf;
	uint32_t *program;
	
	// check for correct # of arguments
	if (argc != 2) {
		printf("%s <input_file> \n", argv[0]);
		return 1;
	}

	// try to open file
	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("Error reading file");
		return 1;
	}
	
	// file information
	fstat(fd, &buf);

	// allocate memory for file contents
	//program = malloc(buf.st_size);
	// map contents of the file into memory,
	// accessible through 'program' pointer
	program = mmap(
			NULL,
			buf.st_size,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE,
			fd,
			0
		       );
	
	printf("bprogramsize: %d\n", (buf.st_size / 4));

	// convert to 32 bit int
	for (int i = 0; i < (buf.st_size / 4); i++) {
		uint32_t temp = be32toh(program[i]);
		intfloat t;
	        t.i = temp;
		float_bits(t);
		decode_instruction(t);
		printf("\n");
	}

	if (program == NULL) {
		perror("Program is null");
		close(fd);
		return 1;
	}
	
	munmap(program, buf.st_size);
	close(fd);

	return 0;
} // end main()

// break instruction into first 11 bits, retrieve the instance of this instruction
void decode_instruction(intfloat inp_inst) {
	intfloat opcode;
	int j;

       	opcode.i = (inp_inst.i >> 21) & 0x7FF;
	for (j = 10; j >= 0; j--) {
		printf("%d", (opcode.i >> j) & 0x1);
	}
	printf("\n");

	// call method to find the instance
	instruction_t inst = find_instruction(opcode);
	printf("mnemonic is: %s\n", inst.mnemonic);
	inst.function(inp_inst, inst);
}

// search global list for instance of instruction_t that matches 11 bit opcode
instruction_t find_instruction(intfloat opcode) {
	for (int i = 0; i < sizeof(instruction) / sizeof(instruction[0]);
		       	i++) 
	{
		if (instruction[i].opcode == opcode.i) {
			return instruction[i];
		} else if ((instruction[i].opcode << 1) == opcode.i) {
			return instruction[i];
		}
	}
}

void r_format(intfloat inp_inst, instruction_t instr) {
	printf("R-format\n");
	printf("%s\n", instr.mnemonic);
}

void i_format(intfloat inp_inst, instruction_t instr) {
	printf("I-format\n");
	printf("%s\n", instr.mnemonic);

	intfloat immediate;
	immediate.i = (inp_inst.i >> 10) &0xFFF;
	printf("%d\n", immediate.i);

	intfloat Rn;
	Rn.i = (inp_inst.i >> 5) & 0x1F;
	printf("%d\n", Rn.i);

	intfloat Rd;
	Rd.i = inp_inst.i & 0x1F;
	printf("%d\n", Rd.i);
}

// print the entire 32-bit instruction
void float_bits(intfloat i) {
	int j;
	for (j = 31; j >= 0; j--) {
		printf("%d", (i.i >> j) & 0x1);
	}
	printf("\n");
}

// print the first 11 bits opcode
// input: 32-bit instruction
void get_format(intfloat i) {
	int j;
	printf("First 11 bits: ");
	for (j = 31; j >= 21; j--) {
		printf("%d", (i.i >> j) & 0x1);
	}
	printf("\n");
}

void ADD_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void ADDI_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void ADDIS_inst() {
	printf("ADDIS_inst function called \n");
}


