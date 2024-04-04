#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

	int fd;
	struct stat buf;
	char *program;
	
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
	program = malloc(buf.st_size);
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

	free(program);
	close(fd);

	

	return 0;
}

