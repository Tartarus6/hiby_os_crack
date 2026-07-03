#ifndef SYSTEM_H
#define SYSTEM_H

void sync_battery_from_sysfs();
char * read_battery_percent();
void system_start_services();

#endif /* SYSTEM_H */
