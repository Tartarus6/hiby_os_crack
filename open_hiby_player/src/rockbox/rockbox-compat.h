#ifndef ROCKBOX_COMPAT_H
#define ROCKBOX_COMPAT_H

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdio.h>

#define logf printf
#define DEBUGF printf
#define panicf printf // TODO: probably smart to make it actually panic, rather than print the log and continue

static inline bool sysfs_get_int(const char *path, int *value)
{
    *value = -1;
    FILE *f = fopen(path, "re");
    if (f == NULL)
        return false;

    bool success = true;
    if (fscanf(f, "%d", value) == EOF) {
        DEBUGF("ERROR %s: Read failed for %s.", __func__, path);
        success = false;
    }
    fclose(f);
    return success;
}

#endif
