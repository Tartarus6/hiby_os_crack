#include "utils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_file_content(const char* filename) {
	// attempt to open file
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return NULL;
	}

	// determine file size
	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	fseek(file, 0, SEEK_SET); // reset file pointer to beginning

	// allocate memory
	char *content = (char *)malloc(filesize + 1); // +1 byte for the null terminator
	if (content == NULL) {
		fclose(file);
		return NULL;
	}

	// read file and write into content buffer
	size_t bytes_read = fread(content, 1, filesize, file);
	content[bytes_read] = '\0'; // explicitly null-terminate the string

	// cleanup
	fclose(file);

	return content;
}

// bool file_matches(const char *filename, const char *expected) {
//     FILE *file = fopen(filename, "rb");
//     if (file == NULL) {
//         return false; // Couldn't open file
//     }

//     char buffer[64];

//     size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
//     fclose(file);

//     buffer[bytes_read] = '\0';

//     // Remove trailing newline(s), if present.
//     buffer[strcspn(buffer, "\r\n")] = '\0';

//     return strcmp(buffer, expected) == 0;
// }
