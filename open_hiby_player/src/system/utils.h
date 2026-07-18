#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

char* read_file_content(const char* filename);
bool file_matches(const char *filename, const char *expected);

// Case-insensitive check of whether `name` ends with `ext` (e.g. ".wav").
bool has_extension(const char *name, const char *ext);

#endif /* UTILS_H */
