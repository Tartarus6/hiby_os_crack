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

#include "src/gui.h"
#include "src/misc/lv_async.h"
#include "src/utils.h"
#include "src/events.h"

// TODO: mounting doesn't work. it seems like the binary doesn't have access to the sd card device or something

static char battery_cache[8] = "!!";


void sync_battery_from_sysfs() {
	char *content = read_file_content("/sys/class/power_supply/battery/capacity");

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

static int mount_sd() {
	mkdir("/media", 0755); // make media dir in case it didnt exist

	int ret = mount(
		"/dev/mmcblk0p1",
		"/media",
		"auto", // auto identify format
		MS_NOATIME,
		NULL
	);

	return ret;
}

static void unmount_sd() {
	umount("/media");
}

void *sd_hotplug_thread(void *arg) {
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

		if (strstr(buf, "mmcblk0p1")) {
			if (strstr(buf, "add@")) {
				popup_event_t *ev = malloc(sizeof(*ev));
				strcpy(ev->text, "SD Card Inserted");

				lv_async_call(popup_async_cb, ev);
				printf("SD Card Inserted\n");
				mount_sd();
			}

			if (strstr(buf, "remove@")) {
				popup_event_t *ev = malloc(sizeof(*ev));
				strcpy(ev->text, "SD Card Removed");

				lv_async_call(popup_async_cb, ev);
				printf("SD Card Removed\n");
				unmount_sd();
			}
		}
	}
}

static void check_and_mount_existing_sd(void) {
    if (access("/dev/mmcblk0p1", F_OK) == 0) {
        printf("SD card device found at boot, mounting...\n");
        if (mount_sd() == 0) {
            printf("SD card mounted successfully.\n");
        } else {
            perror("mount_sd");
        }
    } else {
        printf("No SD card detected at boot.\n");
    }
}

void system_start_services() {
	check_and_mount_existing_sd(); // mount sd on startup, if it's present

    pthread_t sd_thread;
    if (pthread_create(&sd_thread, NULL, sd_hotplug_thread, NULL) != 0) {
	    fprintf(stderr, "Failed to start SD hotplug thread :(\n");
	} else {
		printf("Started SD hotplug thread :)\n");
	}
}
