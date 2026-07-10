#ifndef ROCKBOX_COMPAT_H
#define ROCKBOX_COMPAT_H

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdio.h>

#define logf printf
#define DEBUGF printf
#define panicf printf

static FILE* open_read(const char *file_name)
{
    FILE *f = fopen(file_name, "re");
    if(f == NULL)
    {
        DEBUGF("ERROR %s: Can not open %s for reading.", __func__, file_name);
    }

    return f;
}

bool sysfs_get_int(const char *path, int *value)
{
    *value = -1;

    FILE *f = open_read(path);
    if(f == NULL)
    {
        return false;
    }

    bool success = true;
    if(fscanf(f, "%d", value) == EOF)
    {
        DEBUGF("ERROR %s: Read failed for %s.", __func__, path);
        success = false;
    }

    fclose(f);
    return success;
}

static const char *alsa_ctl_type_name(snd_ctl_elem_type_t type)
{
    switch(type)
    {
        case SND_CTL_ELEM_TYPE_BOOLEAN: return "BOOLEAN";
        case SND_CTL_ELEM_TYPE_INTEGER: return "INTEGER";
        case SND_CTL_ELEM_TYPE_ENUMERATED: return "ENUMERATED";
        default: return "???";
    }
}

#endif
