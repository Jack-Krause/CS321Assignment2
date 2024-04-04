#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <endian.h>

int main(int argc, char *argv[]) {

	int fd;
	struct stat buf;
	char *program;
	uint32_t *bprogram;
	//int *bprogram;
	
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

	// allocate memory for array of elements from file
	// buf.st / 4 = # of 32-bit integers
	bprogram = calloc(buf.st_size / 4, sizeof(*bprogram));
	
	printf("bprogramsize: %d\n", (buf.st_size / 4));

	// convert to 32 bit int
	for (int i = 0; i < (buf.st_size / 4); i++) {
		bprogram[i] = be32toh(*(uint32_t *)(
					program + i * sizeof(uint32_t)));
		//bprogram[i] = be32toh(program[i]);
		printf("%d\n", bprogram[i]);
	}

	if (program == NULL) {
		perror("Program is null");
		close(fd);
		return 1;
	}

	// read contents of file
	ssize_t bytes_read = read(fd, program, buf.st_size);
	if (bytes_read == -1) {
		perror("Error reading file");
		free(program);
		close(fd);
		return 1;
	}

	printf("Contents of file: \n%s\n", program);

	free(bprogram);
	munmap(program, buf.st_size);
	close(fd);

	

	return 0;
}

