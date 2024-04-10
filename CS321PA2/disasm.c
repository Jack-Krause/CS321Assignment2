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
	int i;
	float f;
} intfloat;

typedef struct {
	char mnemonic[10];
	void (*function)();
	uint32_t opcode;
} instruction_t;


// declare functions
void decode_instruction(intfloat instruction);

instruction_t find_instruction(intfloat opcode);

void float_bits(intfloat i);

void show_ieee754(intfloat i);

void get_format(intfloat i);

void ADD_inst(intfloat instruction);
void ADDI_inst(intfloat instruction);
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
}

//void decode_instruction(uint32_t instruction) {
void decode_instruction(intfloat inp_inst) {
	intfloat opcode;
	int j;

       	opcode.i = (inp_inst.i >> 21) & 0x7FF;
	
	for (j = 10; j >= 0; j--) {
		printf("%d", (opcode.i >> j) & 0x1);
	}
	printf("\n");

	instruction_t inst = find_instruction(opcode);
	printf("%s\n", inst.mnemonic);
}

instruction_t find_instruction(intfloat opcode) {
	for (int i = 0; i < sizeof(instruction) / sizeof(instruction[0]);
		       	++i) 
	{
		if (instruction[i].opcode == opcode.i) {
			return instruction[i];
		}
	}
}

void float_bits(intfloat i) {
	int j;
	for (j = 31; j >= 0; j--) {
		printf("%d", (i.i >> j) & 0x1);
	}
	printf("\n");
}

void show_ieee754(intfloat i) {
	printf("f: %f\ts: %d\tbe: %d\tue: %d\tm: %#x\n", i.f,
			(i.i >> 31) * 0x1, (i.i >> 23) & 0xff,(i.i >> 23) & 0xff - 127, i.i & 0x7fffff);
}

void get_format(intfloat i) {
	int j;
	printf("First 11 bits: ");
	for (j = 31; j >= 21; j--) {
		printf("%d", (i.i >> j) & 0x1);
	}
	printf("\n");
}

void ADD_inst(intfloat instruction) {

}

void ADDI_inst(intfloat instruction) {

}

void ADDIS_inst() {

}


