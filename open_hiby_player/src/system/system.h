#ifndef SYSTEM_H
#define SYSTEM_H

typedef struct {
	const char *device;
	const char *mount_point;
} storage_config_t;

typedef struct {
	const char *battery_capacity_file;
	// could also add file that says whether battery is currently charging or not
} battery_config_t;

// TODO: is this dumb to have a struct of structs? will there be issues due to the pointers to pointer
typedef struct {
	storage_config_t *storage_cfg;
	battery_config_t *battery_cfg;
} system_config_t;

typedef void (*system_notification_cb_t)(const char *message, void *user_data);

void sync_battery_from_sysfs(void);
char *read_battery_percent();
void system_start_services(system_config_t *cfg, system_notification_cb_t notification_cb, void *user_data);

#endif /* SYSTEM_H */
