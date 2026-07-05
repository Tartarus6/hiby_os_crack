#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <errno.h>

#include "src/system/audio.h"
#include "src/gui/gui.h"
#include "src/misc/lv_async.h"
#include "src/system/utils.h"
#include "src/events.h"
#include "utils.h"

// TODO: mounting doesn't work. it seems like the binary doesn't have access to the sd card device or something

static char battery_cache[8] = "!!";
static const battery_config_t *g_battery_cfg = NULL;
const char *device_name; // TODO: is it safe to have a global pointer? when/how does it get freed? what if the program crashes?


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
	strncpy(battery_cache, content, sizeof(battery_cache) - 1); // -1 to leave room for null terminator
	battery_cache[sizeof(battery_cache) - 1] = '\0'; // null terminator

	// cleanup
	free(content);
}

char * read_battery_percent() {
	return battery_cache;
}

// TODO: make this function more generic, in case device name differs
static int wait_for_sd(storage_config_t *storage_cfg) {
	if (access("/dev/mmcblk0p1", F_OK) == 0) {
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

        for (char *p = buf; p < buf + len; ) {
            struct inotify_event *ev = (struct inotify_event *)p;

            if ((ev->mask & (IN_CREATE | IN_MOVED_TO)) &&
                strcmp(ev->name, "mmcblk0p1") == 0)
            {
                close(fd);
                return 0;
            }

            p += sizeof(*ev) + ev->len;
        }
    }
}

// TODO: make this function more generic, in case device name differs
static int mount_sd(storage_config_t *storage_cfg) {
	mkdir(storage_cfg->mount_point, 0755); // make media dir in case it didnt exist

	if (wait_for_sd(storage_cfg) != 0) {
        fprintf(stderr, "Timed out waiting for SD\n");
        return -1;
	}

	// TODO: detect filesystem type automatically, rather than it being hardcoded
	int ret = mount(
		storage_cfg->device,
		storage_cfg->mount_point,
		"exfat",
		MS_NOATIME,
		NULL
	);

	if (ret != 0) {
		perror("mount_sd failed");
	}

	return ret;
}

// TODO: make mount directory be a variable somewhere, instead of a repeated hard-coded value
static void unmount_sd(storage_config_t *storage_cfg) {
	umount(storage_cfg->mount_point);
}

// TODO: make this function more generic, in case device name differs
static void check_and_mount_existing_sd(storage_config_t *storage_cfg) {
    if (access(storage_cfg->device, F_OK) == 0) {
        printf("SD card device found at boot, mounting...\n");
        mount_sd(storage_cfg);
    } else {
        printf("No SD card detected at boot.\n");
    }
}

// TODO: make this function more generic, in case device name differs
void *sd_hotplug_thread(void *arg) {
	// get cfg
	storage_config_t *storage_cfg = arg;

	int sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
		.nl_pid = getpid(),
		.nl_groups = 1,
	};

	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
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

		if (strstr(buf, device_name)) {
			if (strstr(buf, "add@")) {
				popup_event_t *ev = malloc(sizeof(*ev));
				strcpy(ev->text, "SD Card Inserted");

				lv_async_call(popup_async_cb, ev);
				printf("SD Card Inserted\n");
				mount_sd(storage_cfg);
			}

			if (strstr(buf, "remove@")) {
				popup_event_t *ev = malloc(sizeof(*ev));
				strcpy(ev->text, "SD Card Removed");

				lv_async_call(popup_async_cb, ev);
				printf("SD Card Removed\n");
				unmount_sd(storage_cfg);
			}
		}
	}
}

/*
 * Starts system services including:
 *     - SD card detection + mounting
 *     - Audio service initializing
 */
void system_start_services(system_config_t *cfg) {
	// --- Battery ---
	g_battery_cfg = cfg->battery_cfg;

	// --- Storage ---
	// getting device name
	// TODO: does this work? what if the device is multiple directories in?
	device_name = strrchr(cfg->storage_cfg->device, '/');
	if (device_name)
	    device_name++;
	else
	    device_name = cfg->storage_cfg->device;

	check_and_mount_existing_sd(cfg->storage_cfg); // mount sd on startup, if it's present

    pthread_t sd_thread;
    if (pthread_create(&sd_thread, NULL, sd_hotplug_thread, cfg->storage_cfg) != 0) {
	    fprintf(stderr, "Failed to start SD hotplug thread :(\n");
	} else {
		printf("Started SD hotplug thread :)\n");
	}

    // --- Audio ---
    audio_init(); // TODO: audio doesnt work. the system restarts after attempting to play sound
}
