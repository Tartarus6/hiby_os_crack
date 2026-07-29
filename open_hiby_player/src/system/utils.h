#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

char *read_file_content(const char *filename);
bool file_matches(const char *filename, const char *expected);

// Case-insensitive check of whether `name` ends with `ext` (e.g. ".wav").
bool has_extension(const char *name, const char *ext);

// Returns the size in bytes of the given file, or -1 on failure.
long get_file_size(const char *filepath);

int formatDoubleSeconds(double total_seconds, char *buffer, size_t max_len);
void formatDoubleProgress(double current_secs, double total_secs, char *buffer, size_t max_len);

#endif
