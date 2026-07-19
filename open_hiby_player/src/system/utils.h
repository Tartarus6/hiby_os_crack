#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

char* read_file_content(const char* filename);
bool file_matches(const char *filename, const char *expected);

// Case-insensitive check of whether `name` ends with `ext` (e.g. ".wav").
bool has_extension(const char *name, const char *ext);

int formatDoubleSeconds(double total_seconds, char *buffer, size_t max_len);
void formatDoubleProgress(double current_secs, double total_secs, char *buffer, size_t max_len);

#endif /* UTILS_H */
