#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

// absolute location is the index
// from the instructions[] array where the label is declared
typedef union {
	int absolute_index;
	char label[30];
} branch_label;

// counter for current line
int instruction_counter = 0;
// list array of instructions
// may have to be added to, adjusted for labels and branches
char instruction_list[1000][50];
// for tracking of branch labels and their names
int branch_counter = 0;
branch_label branches[30];

// declare functions
void decode_instruction(intfloat inp_inst, int num_opcodes);

void insert_instruction(const char* instr, int idx);

void insert_label(const char* label, int idx);

int binary_search(intfloat opcode, int left, int right);

// callable methods for output of instruction
void r_format(intfloat inp_inst, instruction_t instr);
void i_format(intfloat inp_inst, instruction_t instr);
void b_format(intfloat inp_inst, instruction_t instr);

int partition(int first, int last);

void quick_sort(int first, int last);

void float_bits(intfloat i);

void get_format(intfloat i);

// methods for specific instances of required instructions
// these call their respective LEGv8 instruction type or format
void ADD_inst(intfloat inp_inst, instruction_t instr);
void ADDI_inst(intfloat inp_inst, instruction_t instr);
void ADDIS_inst(intfloat inp__inst, instruction_t instr);
void ADDS_inst(intfloat inp_inst, instruction_t instr);
void AND_inst(intfloat inp_inst, instruction_t instr);
void ANDI_inst(intfloat inp_inst, instruction_t instr);
void ANDIS_inst(intfloat inp_inst, instruction_t instr);
void ANDS_inst(intfloat inp_inst, instruction_t instr);
void B_inst(intfloat inp_inst, instruction_t instr);
void BL_inst(intfloat inp_inst, instruction_t instr);
void BR_inst(intfloat inp_inst, instruction_t instr);
void CBNZ_inst(intfloat inp_inst, instruction_t instr);
void CBZ_inst(intfloat inp_inst, instruction_t instr);

instruction_t instruction[] = {
	{ "ADD",     ADD_inst,    0b10001011000 },
  	{ "ADDI",    ADDI_inst,   0b1001000100  },
  	{ "ADDIS",   ADDIS_inst,  0b1011000100  },
	{ "ADDS",    ADDS_inst,   0b10101011000 },
  	{ "AND",     AND_inst,    0b10001010000 },
  	{ "ANDI",    ANDI_inst,   0b1001001000  },
  	{ "ANDIS",   ANDIS_inst,  0b1111001000  },
  	{ "ANDS",    ANDS_inst,   0b1110101000  },
	{ "B",       B_inst,      0b000101      },
 	{ "BL",      BL_inst,     0b100101      },
 	{ "BR",      BR_inst,     0b11010110000 },
 	{ "CBNZ",    CBNZ_inst,   0b10110101    },
 	{ "CBZ",     CBZ_inst,    0b10110100    }
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

	int num_opcodes = sizeof(instruction) / sizeof(instruction[0]);
	printf("N is: %d\n", num_opcodes);

	// sort opcodes for lookup time of O(log n) with binary search
	// might implement a hashtable if time
	quick_sort(0, num_opcodes -1);

	for (int i = 0; i < num_opcodes; i++) {
		printf("Sorted: %s ", instruction[i].mnemonic);
		for (int k = 10; k >= 0; k--) {
			printf("%d", (instruction[i].opcode >> k) & 0x1);
		}
		printf(" %d", instruction[i].opcode);
		printf("\n");
	}
	
	// convert to 32 bit int
	for (int i = 0; i < (buf.st_size / 4); i++) {
		uint32_t temp = be32toh(program[i]);
		intfloat t;
	        t.i = temp;
		float_bits(t);
		decode_instruction(t, num_opcodes);
		printf("\n");
	}

	if (program == NULL) {
		perror("Program is null");
		close(fd);
		return 1;
	}
	
	for (int i = 0; i < instruction_counter; i++) {
		printf("%s\n", instruction_list[i]);
	}

	munmap(program, buf.st_size);
	close(fd);

	return 0;
} // end main()

// break instruction into first 11 bits, retrieve the instance of this instruction
void decode_instruction(intfloat inp_inst, int num_opcodes) {
	intfloat opcode;

	for (int j = 31; j >= 21; j--) {
		printf("%d", (inp_inst.i >> j) & 0x1);
	}
	printf(" %d\n", inp_inst.i >> 26);

	// try increasingly longer opcodes
	opcode.i = inp_inst.i >> 26; // first 6 bits to check for B-type
	int idx_found = binary_search(opcode, 0, num_opcodes-1);
	
	// check if 6 bit opcode found, otherwise search for 8 bit opcode
	if (idx_found == -1) {
		opcode.i = inp_inst.i >> 24; // first 8 bits for CB-type
		idx_found = binary_search(opcode, 0, num_opcodes-1);
	}
	// check if 8 bit opcode found, otherwise search for 10 bit opcode
	if (idx_found == -1) {
		opcode.i = inp_inst.i >> 22; // first 10 bit opcodes for I-type
		idx_found = binary_search(opcode, 0, num_opcodes-1);
	}
	// check if 10 bit opcode found, otherwise search for 11 bit opcode
	if (idx_found == -1) {
		opcode.i = inp_inst.i >> 21;
		idx_found = binary_search(opcode, 0, num_opcodes-1);
	}
	
	// if idx >= 0 then success, call output function of LEGv8 instruction
	if (idx_found > -1) {
		instruction_t inst_found = instruction[idx_found];
		printf("mnemonic is: %s\n", inst_found.mnemonic);
		// call instance function
		inst_found.function(inp_inst, inst_found);
	} else { // the instruction was not found
		printf("ERROR instruction not found in opcodes\n");
	}	
}


void insert_instruction(const char* instr, int idx) {
	strcpy(instruction_list[idx], instr);
}

// shift instructions, insert "label:", add label to list
void insert_label(branch_label branch, int idx) {
	for (int i = idx; i < instruction_counter; i++) {
		char[] temp = instruction_list[i+1];
		instruction_list[i+1] = i;
	}
	instruction_list[idx] = branch.label;
	
	int b = 0;
	for (b = 0; b < branch_counter; b++) {
		if (branches[b].absolute_index == idx) {
			return;
		}
	}

	branches[b+1] = branch;
	branch_counter++;
}	

// binary search to find index of instruction matching the opcode
// returns: index in instruction or -1 else
int binary_search(intfloat opcode, int left, int right) {
	while (left <= right) {
		int mid = left + (right - left) / 2;
		
		if (instruction[mid].opcode == opcode.i) {
			return mid;
		}

		if (instruction[mid].opcode < opcode.i) {
			left = mid + 1;
		} else {
			right = mid - 1;
		}
	}

	return -1;
}

void r_format(intfloat inp_inst, instruction_t instr) {
	printf("R-format\n");

	// opcode: first 11 bits [31-21]
	printf("%s ", instr.mnemonic);

	// Rm: second register source operand: 5 bits [20-16]
	intfloat Rm;
	Rm.i = (inp_inst.i >> 16) & 0x1F;
	//printf("%d ", Rm.i);

	// shamt: shift amount: 6 bits [15-10]
	intfloat shamt;
	shamt.i = (inp_inst.i >> 10) & 0xF;
	//printf("%d ", shamt.i);

	// Rn: first register source operand: 5 bits [9-5]
	intfloat Rn;
	Rn.i = (inp_inst.i >> 5) & 0x1F;
	//printf("%d ", Rn.i);

	// Rd: register destination: 5 bits [4-0]
	intfloat Rd;
	Rd.i = inp_inst.i & 0x1F;
	//printf("%d\n", Rd.i);

	printf("X%d, X%d, X%d\n", Rd.i, Rn.i, Rm.i);

	char str[50];
	sprintf(str, "%s X%d, X%d, X%d", instr.mnemonic, Rd.i, Rn.i, Rm.i);
	insert_instruction(str, instruction_counter);
	instruction_counter++;	
}

void i_format(intfloat inp_inst, instruction_t instr) {
	printf("I-format\n");

	// opcode: first 10 bits [31-22]
	printf("%s ", instr.mnemonic);
	
	// immediate value: 12 bits [21-10]
	intfloat immediate;
	immediate.i = (inp_inst.i >> 10) &0xFFF;
	//printf("%d ", immediate.i);

	// Rn: source register: 5 bits [9-5]
	intfloat Rn;
	Rn.i = (inp_inst.i >> 5) & 0x1F;
	//printf("%d ", Rn.i);
	
	// Rd: destination register: 5 bits [4-0]
	intfloat Rd;
	Rd.i = inp_inst.i & 0x1F;
	//printf("%d\n", Rd.i);
	
	printf("X%d, X%d, #%d\n", Rd.i, Rn.i, immediate.i);

	char str[50];
	sprintf(str, "%s X%d, X%d, #%d\n", instr.mnemonic, Rd.i, Rn.i, immediate.i);
	insert_instruction(str, instruction_counter);
	instruction_counter++;
}

// 
void b_format(intfloat inp_inst, instruction_t instr) {
	printf("B-format\n");
	printf("%s ", instr.mnemonic);
	
	branch_label temp;
	// set location (line number) of branch label
	uint32_t loc = inp_inst.i & 0x03FFFFFF;	
	temp.absolute_index = instruction_counter - loc;
	// create instance of label and add it to the output array
	strcpy(temp.label, "label");
	char str_count[20];
	sprintf(str_count, "%d", branch_counter);
	strncat(temp.label, str_count, 30 - strlen(temp.label) -1);
	strcpy(temp.label, ":");
	// insertion/operation 
	// call with instruction_counter - relative_address
	insert_branch!!!1
	branch_counter++;

	
}

int partition(int first, int last) {
	instruction_t p = instruction[last];
	uint32_t pivot = p.opcode;
	int i = first - 1;

	for (int j = first; j < last; j++) {
		if (instruction[j].opcode < pivot) {
			i++;
			instruction_t temp = instruction[i];
			instruction[i] = instruction[j];
			instruction[j] = temp;
		}
	}
	instruction_t temp = instruction[i + 1];
	instruction[i + 1] = p; // insert pivot into sorted location
	instruction[last] = temp;
	return i+1; // location of sorted location of p
}

void quick_sort(int first, int last) {
	if (first < last) {
		int p = partition(first, last);

		quick_sort(first, p - 1);
		quick_sort(p + 1, last);
	}	
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

void ADDIS_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void ADDS_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void AND_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void ANDI_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void ANDIS_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void ANDS_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

// 
void B_inst(intfloat inp_inst, instruction_t instr) {
	printf("%d\n", (inp_inst.i & 0x03FFFFFF));
	branch_label temp;
	strcpy(temp.label, "label");
	char strCount[5];
	sprintf(strCount, "%d", branch_counter);
	strncat(temp.label, strCount, 30 - strlen(temp.label) -1);
	//sprintf(temp.label, "%d", branch_counter);
	printf("%s\n", temp.label);
	branch_counter++;
	b_format(inp_inst, instr);
}	

void BL_inst(intfloat inp_inst, instruction_t instr) {
	b_format(inp_inst, instr);
}

void BR_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void CBNZ_inst(intfloat inp_inst, instruction_t instr) {
	printf("CBNZ_inst\n");
	// CB-type
}

void CBZ_inst(intfloat inp_inst, instruction_t instr) {
	printf("CBZ_inst\n");
	// CB-type
}




