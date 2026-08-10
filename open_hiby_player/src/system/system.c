#include "system.h"

#include <errno.h>
#include <linux/input.h>
#include <linux/netlink.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "src/system/audio.h"
#include "src/system/device_state.h"
#include "src/system/power.h"
#include "src/system/utils.h"
#include "utils.h"

static char battery_cache[8] = "!!";
static const battery_config_t *g_battery_cfg = NULL;

typedef struct {
	storage_config_t *storage_cfg;
	const char *device_name;
	system_notification_cb_t notification_cb;
	void *notification_user_data;
} system_runtime_t;

static void send_notification(const system_runtime_t *runtime, const char *message) {
	if (runtime->notification_cb) {
		runtime->notification_cb(message, runtime->notification_user_data);
	}
}

static const char *storage_device_name(const char *device_path) {
	const char *name = strrchr(device_path, '/');
	return name ? name + 1 : device_path;
}

void sync_battery_from_sysfs(void) {
	if (!g_battery_cfg) {
		strcpy(battery_cache, "!!");
		return;
	}

	const battery_config_t *battery_cfg = g_battery_cfg;
	char *content = read_file_content(battery_cfg->battery_capacity_file);

	// handle failed read
	if (content == NULL) {
		strcpy(battery_cache, "!!");
		return;
	}

	content[strcspn(content, "\r\n")] = '\0'; // remove the trailing newline

	// copy content into cache
	strncpy(battery_cache, content,
			sizeof(battery_cache) - 1);				 // -1 to leave room for null terminator
	battery_cache[sizeof(battery_cache) - 1] = '\0'; // null terminator

	// cleanup
	free(content);
}

char *read_battery_percent() { return battery_cache; }

static int wait_for_sd(const system_runtime_t *runtime) {
	char device_path[128];
	snprintf(device_path, sizeof(device_path), "/dev/%s", runtime->device_name);

	if (access(device_path, F_OK) == 0) {
		return 0;
	}

	int fd = inotify_init1(IN_CLOEXEC);
	if (fd < 0) {
		return -1;
	}

	int wd = inotify_add_watch(fd, "/dev", IN_CREATE | IN_MOVED_TO);
	if (wd < 0) {
		close(fd);
		return -1;
	}

	char buf[4096];

	for (;;) {
		ssize_t len = read(fd, buf, sizeof(buf));

		if (len < 0) {
			if (errno == EINTR)
				continue;

			close(fd);
			return -1;
		}

		for (char *p = buf; p < buf + len;) {
			struct inotify_event *ev = (struct inotify_event *)p;

			if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) && strcmp(ev->name, runtime->device_name) == 0) {
				close(fd);
				return 0;
			}

			p += sizeof(*ev) + ev->len;
		}
	}
}

static int mount_sd(storage_config_t *storage_cfg, const system_runtime_t *runtime) {
	mkdir(storage_cfg->mount_point,
		  0755); // make media dir in case it didnt exist

	if (wait_for_sd(runtime) != 0) {
		fprintf(stderr, "Timed out waiting for SD\n");
		return -1;
	}

	// TODO: detect filesystem type automatically, rather than it being hardcoded
	int ret = mount(storage_cfg->device, storage_cfg->mount_point, "exfat", MS_NOATIME, NULL);

	if (ret != 0) {
		perror("mount_sd failed");
	}

	return ret;
}

static void unmount_sd(storage_config_t *storage_cfg) { umount(storage_cfg->mount_point); }

static void check_and_mount_existing_sd(storage_config_t *storage_cfg, const system_runtime_t *runtime) {
	if (access(storage_cfg->device, F_OK) == 0) {
		printf("SD card device found at boot, mounting...\n");
		mount_sd(storage_cfg, runtime);
	} else {
		printf("No SD card detected at boot.\n");
	}
}

void *sd_hotplug_thread(void *arg) {
	system_runtime_t *runtime = arg;
	storage_config_t *storage_cfg = runtime->storage_cfg;

	int sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
		.nl_pid = getpid(),
		.nl_groups = 1,
	};

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Failed to bind netlink socket\n");
		close(sock);
		return NULL;
	}

	char buf[4096 + 1]; // +1 is for null terminator

	while (1) {
		int len = recv(sock, buf, sizeof(buf), 0);
		if (len <= 0) {
			continue;
		}

		buf[len] = '\0';

		// char *p = buf;

		// while (p < buf + len) {
		//     printf("[%s]\n", p);
		//     p += strlen(p) + 1;
		// }

		// puts("----");

		if (strstr(buf, runtime->device_name)) {
			if (strstr(buf, "add@")) {
				send_notification(runtime, "SD Card Inserted");
				printf("SD Card Inserted\n");
				mount_sd(storage_cfg, runtime);
			}

			if (strstr(buf, "remove@")) {
				send_notification(runtime, "SD Card Removed");
				printf("SD Card Removed\n");
				unmount_sd(storage_cfg);
			}
		}
	}
}

// TODO: can the event0 and event2 threads be combined to save some overhead and reduce duplicate code?
static void *event0_thread_func(void *arg) {
	int fd = open("/dev/input/event0", O_RDONLY);

	if (fd < 0) {
		perror("open input event0");
		return NULL;
	}

	struct input_event ev;

	while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {

		if (ev.type != EV_KEY)
			continue;

		if (ev.value == 1) {
			power_notify_activity(); // any physical button press counts as user activity
		}

		switch (ev.code) {
		case KEY_POWER:
			if (ev.value == 1) {
				printf("Power Button Pressed\n");
				power_notify_power_button(); // short-press toggles the screen / wakes from suspend
			} else if (ev.value == 0) {
				printf("Power Button Released\n");
			}
			break;

		case KEY_PREVIOUSSONG:
			if (ev.value == 1) {
				printf("0: Previous Song Button Pressed\n");
			} else if (ev.value == 0) {
				printf("0: Previous Song Button Released\n");
			}
			break;
		}
	}

	close(fd);
	return NULL;
}

static void *event2_thread_func(void *arg) {
	int fd = open("/dev/input/event2", O_RDONLY);

	if (fd < 0) {
		perror("open input event2");
		return NULL;
	}

	struct input_event ev;

	while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {

		if (ev.type != EV_KEY)
			continue;

		if (ev.value == 1) {
			power_notify_activity(); // any physical button press counts as user activity
		}

		/* ignore key release */
		// if (ev.value != 1)
		// 	continue;

		switch (ev.code) {
		case KEY_VOLUMEUP:
			if (ev.value == 1) {
				printf("Volume Up Button Pressed\n");
				device_state_change_volume(-5);
			} else if (ev.value == 0) {
				printf("Volume Up Button Released\n");
			}
			break;

		case KEY_VOLUMEDOWN:
			if (ev.value == 1) {
				printf("Volume Down Button Pressed\n");
				device_state_change_volume(5);
			} else if (ev.value == 0) {
				printf("Volume Down Button Released\n");
			}
			break;

		case KEY_NEXTSONG:
			if (ev.value == 1) {
				printf("2: Next Song Button Pressed\n");
			} else if (ev.value == 0) {
				printf("2: Next Song Button Released\n");
			}
			break;

		case KEY_PLAYPAUSE:
			if (ev.value == 1) {
				printf("2: Play/Pause Button Pressed\n");
			} else if (ev.value == 0) {
				printf("2: Play/Pause Button Released\n");
			}
			break;
		}
	}

	close(fd);
	return NULL;
}

/*
 * Starts system services including:
 *     - SD card detection + mounting
 *     - Audio service initializing
 */
void system_start_services(system_config_t *cfg, system_notification_cb_t notification_cb, void *user_data) {
	static bool initialized = false;
	if (initialized)
		return;

	static system_runtime_t runtime;

	// --- Battery ---
	g_battery_cfg = cfg->battery_cfg;

	// --- Storage ---
	runtime.storage_cfg = cfg->storage_cfg;
	runtime.device_name = storage_device_name(cfg->storage_cfg->device);
	runtime.notification_cb = notification_cb;
	runtime.notification_user_data = user_data;

	check_and_mount_existing_sd(cfg->storage_cfg, &runtime); // mount sd on startup, if it's present

	pthread_t sd_thread;
	if (pthread_create(&sd_thread, NULL, sd_hotplug_thread, &runtime) != 0) {
		fprintf(stderr, "Failed to start SD hotplug thread :(\n");
	} else {
		printf("Started SD hotplug thread :)\n");
		pthread_detach(sd_thread);
	}

	pthread_t event0_thread;
	pthread_create(&event0_thread, NULL, event0_thread_func, NULL);
	pthread_detach(event0_thread);

	pthread_t event2_thread;
	pthread_create(&event2_thread, NULL, event2_thread_func, NULL);
	pthread_detach(event2_thread);

	// --- Audio ---
	audio_init();

	initialized = true;
}
