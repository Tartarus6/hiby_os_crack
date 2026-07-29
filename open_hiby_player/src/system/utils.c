#include "utils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file_content(const char *filename) {
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

bool file_matches(const char *filename, const char *expected) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return false; // Couldn't open file
	}

	char buffer[64];

	size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
	fclose(file);

	buffer[bytes_read] = '\0';

	// Remove trailing newline(s), if present.
	buffer[strcspn(buffer, "\r\n")] = '\0';

	return strcmp(buffer, expected) == 0;
}

bool has_extension(const char *name, const char *ext) {
	size_t name_len = strlen(name);
	size_t ext_len = strlen(ext);

	if (name_len < ext_len)
		return false;
	return strcasecmp(name + (name_len - ext_len), ext) == 0;
}

long get_file_size(const char *filepath) {
	FILE *file = fopen(filepath, "rb");
	if (file == NULL) {
		return -1;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fclose(file);

	return size;
}

// formats a double of seconds as an HH:MM:SS string
// hides hours if no hours, only shows necessary minute digits, always has 2 seconds digits
// returns the number of characters written
int formatDoubleSeconds(double total_seconds, char *buffer, size_t max_len) {
	uint32_t total_secs = (uint32_t)total_seconds;

	uint32_t hours = total_secs / 3600;
	uint32_t rem_secs = total_secs % 3600;
	uint32_t minutes = rem_secs / 60;
	uint32_t seconds = rem_secs % 60;

	if (hours > 0) {
		return snprintf(buffer, max_len, "%u:%02u:%02u", hours, minutes, seconds);
	} else {
		return snprintf(buffer, max_len, "%u:%02u", minutes, seconds);
	}
}

// formats a pair of doubles of seconds as an "HH:MM:SS/HH:MM:SS" string
// for each double: hides hours if no hours, only shows necessary minute digits, always has 2 seconds digits
void formatDoubleProgress(double current_secs, double total_secs, char *buffer, size_t max_len) {
	// Guard against a completely useless/zero-size destination buffer
	if (max_len == 0 || buffer == NULL)
		return;

	// Format current time safely
	int chars_written = formatDoubleSeconds(current_secs, buffer, max_len);

	// snprintf returns what it *wanted* to write. We must clamp it to the actual capacity.
	if ((size_t)chars_written >= max_len) {
		return; // Buffer was too small; current time consumed or hit the bounds
	}

	buffer += chars_written;
	max_len -= chars_written; // Reduce the remaining capacity

	// Append the divider safely (ensure space for '/' and a '\0')
	if (max_len < 2) {
		return;
	}
	*buffer++ = '/';
	max_len--; // Reduce capacity by 1 for the '/' character

	// Format total time into the remaining memory space
	formatDoubleSeconds(total_secs, buffer, max_len);
}
