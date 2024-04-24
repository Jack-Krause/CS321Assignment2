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
typedef struct {
	uint32_t absolute_index;
	char *label;
} branch_label;

// counter for current line
int instruction_counter;
// list array of instructions
char *instruction_list[1000] = {NULL};
// for tracking of branch labels and their names
int branch_counter;
//branch_label* branches;
branch_label branches[30];

// declare functions
void decode_instruction(intfloat inp_inst, int num_opcodes);
void insert_branches();
void insert_instruction_index(char instr[], int idx);
void insert_instruction(char instr[]);
void insert_label(branch_label branch, uint32_t idx);
int binary_search(intfloat opcode, int left, int right);

// LEGv8 format instructions:
void r_format(intfloat inp_inst, instruction_t instr);
void i_format(intfloat inp_inst, instruction_t instr);
void b_format(intfloat inp_inst, instruction_t instr);
void cb_format(intfloat inp_inst, instruction_t instr);
void d_format(intfloat inp_inst, instruction_t instr);

// other util functions:
void sort_branches();
int partition(int first, int last);
void quick_sort(int first, int last);
void float_bits(intfloat i);
void get_format(intfloat i);

// methods for specific instances of required instructions
// these call their respective LEGv8 instruction type or format
void ADD_inst(intfloat inp_inst, instruction_t instr);
void ADDI_inst(intfloat inp_inst, instruction_t instr);
void AND_inst(intfloat inp_inst, instruction_t instr);
void ANDI_inst(intfloat inp_inst, instruction_t instr);
void B_inst(intfloat inp_inst, instruction_t instr);
void B_cond(intfloat inp_inst, instruction_t instr);
void BL_inst(intfloat inp_inst, instruction_t instr);
void BR_inst(intfloat inp_inst, instruction_t instr);
void CBNZ_inst(intfloat inp_inst, instruction_t instr);
void CBZ_inst(intfloat inp_inst, instruction_t instr);
void DUMP_inst(intfloat inp_inst, instruction_t instr);
void EOR_inst(intfloat inp_inst, instruction_t instr);
void EORI_inst(intfloat inp_inst, instruction_t instr);
void HALT_inst(intfloat inp_inst, instruction_t instr);
void LDUR_inst(intfloat inp_inst, instruction_t instr);
void LSL_inst(intfloat inp_inst, instruction_t instr);
void LSR_inst(intfloat inp_inst, instruction_t instr);
void MUL_inst(intfloat inp_inst, instruction_t instr);
void ORR_inst(intfloat inp_inst, instruction_t instr);
void ORRI_inst(intfloat inp_inst, instruction_t instr);
void PRNL_inst(intfloat inp_inst, instruction_t instr);
void PRNT_inst(intfloat inp_inst, instruction_t instr);
void STUR_inst(intfloat inp_inst, instruction_t instr);
void SUB_inst(intfloat inp_inst, instruction_t instr);
void SUBI_inst(intfloat inp_inst, instruction_t instr);
void SUBIS_inst(intfloat inp_inst, instruction_t instr);
void SUBS_inst(intfloat inp_inst, instruction_t instr);

// LEGv8 "B.cond" instruction suffix array. Maps hexadecimal to strings
const char* b_suffix[14] = {
	"EQ", "NE", "HS", "LO", "MI", "PL", "VS", "VC",
	"HI", "LS", "GE", "LT", "GT", "LE"
};

// LEGv8 opcodes for needed instructions:
instruction_t instruction[] = {
  { "ADD",     ADD_inst,    0b10001011000 },
  { "ADDI",    ADDI_inst,   0b1001000100  },
  { "AND",     AND_inst,    0b10001010000 },
  { "ANDI",    ANDI_inst,   0b1001001000  },
  { "B",       B_inst,      0b000101      },
  { "BL",      BL_inst,     0b100101      },
  { "B.",      B_cond,      0b01010100    },
  { "BR",      BR_inst,     0b11010110000 },
  { "CBNZ",    CBNZ_inst,   0b10110101    },
  { "CBZ",     CBZ_inst,    0b10110100    },
  { "DUMP",    DUMP_inst,   0b11111111110 },
  { "EOR",     EOR_inst,    0b11001010000 },
  { "EORI",    EORI_inst,   0b1101001000  },
  { "HALT",    HALT_inst,   0b11111111111 },
  { "LDUR",    LDUR_inst,   0b11111000010 },
  { "LSL",     LSL_inst,    0b11010011011 },
  { "LSR",     LSR_inst,    0b11010011010 },
  { "MUL",     MUL_inst,    0b10011011000 },
  { "ORR",     ORR_inst,    0b10101010000 },
  { "ORRI",    ORRI_inst,   0b1011001000  },
  { "PRNL",    PRNL_inst,   0b11111111100 },
  { "PRNT",    PRNT_inst,   0b11111111101 },
  { "STUR",    STUR_inst,   0b11111000000 },
  { "SUB",     SUB_inst,    0b11001011000 },
  { "SUBI",    SUBI_inst,   0b1101000100  },
  { "SUBIS",   SUBIS_inst,  0b1111000100  },
  { "SUBS",    SUBS_inst,   0b11101011000 }
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
	
	// sort opcodes for lookup time of O(log n) with binary search
	// might implement a hashtable if time
	quick_sort(0, num_opcodes -1);

	// convert to 32 bit int
	for (int i = 0; i < (buf.st_size / 4); i++) {
		uint32_t temp = be32toh(program[i]);
		intfloat t;
	        t.i = temp;
		//float_bits(t);
		decode_instruction(t, num_opcodes);
	}

	if (program == NULL) {
		perror("Program is null");
		close(fd);
		return 1;
	}
	
	// NOTE: UNCOMMENT THESE TO SHOW LABELS IN OUTPUT
	sort_branches();
	// insert the branch labels as final editing of output
	insert_branches();
	
	for (int i = 0; i < instruction_counter; i++) {
		if (instruction_list[i] != NULL) {
			printf("%s\n", instruction_list[i]);
		}
	}

	munmap(program, buf.st_size);
	close(fd);

	return 0;
} // end main()

// break instruction into first 11 bits, retrieve the instance of this instruction
void decode_instruction(intfloat inp_inst, int num_opcodes) {
	intfloat opcode;

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
		// call instance function
		inst_found.function(inp_inst, inst_found);
	} else { // the instruction was not found
		printf("ERROR instruction not found in opcodes\n");
	}	
}

// insert branch declarations into correct spots
void insert_branches() {
	for (int i = 0; i < branch_counter; i++) {
		branch_label temp_branch = branches[i];
		
		// adjust absolute index of labels to account for shifting
		temp_branch.absolute_index += i;
		
		// call insert_instruction with absolute_index
		insert_instruction_index(temp_branch.label, temp_branch.absolute_index);
	}
}

void insert_instruction_index(char instr[], int idx) {
	//instruction_counter++;
	instruction_list[instruction_counter+1] = malloc(strlen(instr) + 20);
	
	for (int i = instruction_counter; i >= idx; i--) {
		instruction_list[i] = instruction_list[i-1];	
	}
	instruction_list[idx-1] = instr;
	
	instruction_counter++;
}

void insert_instruction(char instr[]) {
	//instruction_counter++;
	instruction_list[instruction_counter] = malloc(strlen(instr) + 1);
	
	if (instruction_list[instruction_counter] != NULL) {
		strcpy(instruction_list[instruction_counter], instr);
	} else {
		printf("Failed to insert instruction");
	}
	instruction_counter++;
}

// add label to list
void insert_label(branch_label branch, uint32_t idx) {
	// check branch is already declared (this is fine, no error)
	for (int b = 0; b < branch_counter; b++) {
	//	if (strcmp(branches[b].label, branch.label) == 1) {
		if (branches[b].absolute_index == branch.absolute_index) {
			return;
		}
	}
	
	branches[branch_counter] = branch;
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
	char str[30];

	// opcode: first 11 bits [31-21]
	
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
	
	if (strcmp(instr.mnemonic, "PRNT") == 0) {
		sprintf(str, "%s X%d", instr.mnemonic, Rd.i);
		insert_instruction(str);
		return;
	}

	if (strcmp(instr.mnemonic, "BR") == 0) {
		sprintf(str, "%s X%d", instr.mnemonic, Rn.i);
		insert_instruction(str);
		return;
	}

	if (strcmp(instr.mnemonic, "LSL") == 0 || strcmp(instr.mnemonic, "LSR") == 0) {
		sprintf(str, "%s X%d, X%d, #%d", instr.mnemonic, Rd.i, Rn.i, shamt.i);
	} else {
		sprintf(str, "%s X%d, X%d, X%d", instr.mnemonic, Rd.i, Rn.i, Rm.i);
	}

	//printf("X%d, X%d, X%d\n", Rd.i, Rn.i, Rm.i);

	//printf("argument: %s\n", str); 
	//printf("%d\n", instruction_counter);
	insert_instruction(str);
}

void i_format(intfloat inp_inst, instruction_t instr) {
	// opcode: first 10 bits [31-22]
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
	
	//printf("X%d, X%d, #%d\n", Rd.i, Rn.i, immediate.i);

	char str[30];
	sprintf(str, "%s X%d, X%d, #%d", instr.mnemonic, Rd.i, Rn.i, immediate.i);
	insert_instruction(str);
}

// this method does two things:
// 1) checks if the label is declared at: line number - offset, inserts label otherwise. In LEGv8: ```branch2:```
// 2) inserts the actual instruction. in LEGv8: ```B branch2```
void b_format(intfloat inp_inst, instruction_t instr) {
	//printf("B-format\n");
	branch_label *temp_branch = malloc(sizeof(branch_label));
	
	// create instance of label and add it to the output array
	char str_count[30];
	char actual_instr[35];
	int bc = branch_counter + 1;
	sprintf(str_count, "label%d", bc);
	sprintf(actual_instr, "%s %s", instr.mnemonic, str_count);
	strcat(str_count, ":");
	
	temp_branch->label = malloc(strlen(str_count) + 1);
	strcpy(temp_branch->label, str_count);
	
	// insert actual instruction (like: ```B loop2```)
	// increment instruction counter (at the beginning?)
	insert_instruction(actual_instr);

	// calculate absolute_index (line number of "labeln:")
	int relative = (inp_inst.i & 0x03FFFFFF);
	// handling for signed address (negatives)
	if (relative & 0x02000000) {
		relative |= ~0x03FFFFFF;
	}

	temp_branch->absolute_index = instruction_counter + relative;

	// call with instruction_counter - relative_address
	// absolute index is the line number of "label n:"
	insert_label(*temp_branch, temp_branch->absolute_index);
}

void cb_format(intfloat inp_inst, instruction_t instr) {
	//printf("CB-format\n");
	intfloat COND_BR_address;
 	COND_BR_address.i = (inp_inst.i >> 5) & 0x7FFFF;
	
	// handle negative address
	if (COND_BR_address.i & 0x40000) {
		COND_BR_address.i |= ~0x7FFFF;
	}
	
	// add new branch if needed, similar to B-format
	branch_label *temp_branch = malloc(sizeof(branch_label));

	char str_count[30];
	char actual_instr[35];
	int bc = branch_counter + 1;
	sprintf(str_count, "label%d", bc);
	sprintf(actual_instr, "%s", str_count);
	strcat(str_count, ":");
	
	temp_branch->label = malloc(strlen(str_count) + 1);
	strcpy(temp_branch->label, str_count);
	
	intfloat Rt;
	char str[50];
	
	// check for B.cond instruction. diverge if so
	if (strcmp(instr.mnemonic, "B.") == 0) {
		Rt.i = inp_inst.i & 0x1F;
		
		sprintf(str, "%s%s %s", instr.mnemonic, b_suffix[Rt.i], actual_instr);
		insert_instruction(str);
		temp_branch->absolute_index = instruction_counter + COND_BR_address.i;
		insert_label(*temp_branch, temp_branch->absolute_index);
		return;
	}

	// else: continue as normal
	Rt.i = inp_inst.i & 0x1F;
	sprintf(str, "%s X%d, %s", instr.mnemonic, Rt.i, actual_instr);

	insert_instruction(str);

	temp_branch->absolute_index = instruction_counter + COND_BR_address.i;
	
	insert_label(*temp_branch, temp_branch->absolute_index);
}

void d_format(intfloat inp_inst, instruction_t instr) {
	intfloat DT_address;
	intfloat op;
	intfloat Rn;
	intfloat Rt;
	
	// LDUR X9, [X10, #240]
	// DT_address: 9 bits [20-12]
	DT_address.i = (inp_inst.i >> 12) & 0x1FF;

	// op2/op: 2 bits [11-10]
	op.i = (inp_inst.i >> 10) & 0x3;

	// Rn: base register: 5 bits [9-5]
	Rn.i = (inp_inst.i >> 5) & 0x1F;

	// Rt destination/source register: 5 bits [4-0]
	Rt.i = inp_inst.i & 0x1F;

	char str[50];
	sprintf(str, "%s X%d, [X%d, #%d]", instr.mnemonic, Rt.i, Rn.i, DT_address.i);
	insert_instruction(str);
}

void sort_branches() {
	for (int b = 1; b < branch_counter; b++) {
		branch_label temp = branches[b];
		int key = branches[b].absolute_index;
		int j = b - 1;
		while (j >= 0 && branches[j].absolute_index > key) {
			branches[j+1] = branches[j];
			j = j-1;
		}
		branches[j + 1] = temp;
	}
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

void B_inst(intfloat inp_inst, instruction_t instr) {
	b_format(inp_inst, instr);
}	

void B_cond(intfloat inp_inst, instruction_t instr) {
	// special case of CB-format
	cb_format(inp_inst, instr);
}

void BL_inst(intfloat inp_inst, instruction_t instr) {
	b_format(inp_inst, instr);
}

void BR_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void CBNZ_inst(intfloat inp_inst, instruction_t instr) {
	cb_format(inp_inst, instr);
}

void CBZ_inst(intfloat inp_inst, instruction_t instr) {
	cb_format(inp_inst, instr);
}

void DUMP_inst(intfloat inp_inst, instruction_t instr) {
	//r_format(inp_inst, instr);
	// DUMP doesn't follow normal R-format
	insert_instruction(instr.mnemonic);
}

void EOR_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void EORI_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void HALT_inst(intfloat inp_inst, instruction_t instr) {
	// HALT doesn't follow typical R-format
	insert_instruction(instr.mnemonic);
}

void LDUR_inst(intfloat inp_inst, instruction_t instr) {
	d_format(inp_inst, instr);
}

void LSL_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void LSR_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void MUL_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void ORR_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void ORRI_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void PRNL_inst(intfloat inp_inst, instruction_t instr) {
	// PRNL doesn't follow typical R-format
	insert_instruction(instr.mnemonic);
}

void PRNT_inst(intfloat inp_inst, instruction_t instr) {
	// special case of R-format
	r_format(inp_inst, instr);
}

void STUR_inst(intfloat inp_inst, instruction_t instr) {
	d_format(inp_inst, instr);
}

void SUB_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}

void SUBI_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void SUBIS_inst(intfloat inp_inst, instruction_t instr) {
	i_format(inp_inst, instr);
}

void SUBS_inst(intfloat inp_inst, instruction_t instr) {
	r_format(inp_inst, instr);
}


