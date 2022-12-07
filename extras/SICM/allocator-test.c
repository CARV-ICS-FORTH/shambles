#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv){
	size_t size;
	void *ptr;
	if(argc != 2){
		fprintf(stderr, "Usage: %s <allocation size>\n", argv[0]);
		return 1;
	}
	size = atol(argv[1]);
	if(size <= 0){
		fprintf(stderr, "Usage: %s <allocation size>\n", argv[0]);
		return 1;
	}
	
	printf("Before allocation. Hit key to continue...");
	getc(stdin);
	ptr = malloc(size);
	memset(ptr, 0, size);
	printf("After allocation. Hit key to continue...");
	getc(stdin);
	
	return 0;
}
