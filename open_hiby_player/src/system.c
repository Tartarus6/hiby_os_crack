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

// TODO: make this function more generic, in case device name differs
static int wait_for_sd() {
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
static int mount_sd() {
	mkdir("/media", 0755); // make media dir in case it didnt exist

	if (wait_for_sd() != 0) {
        fprintf(stderr, "Timed out waiting for SD\n");
        return -1;
	}

	int ret = mount(
		"/dev/mmcblk0p1",
		"/media",
		"exfat", // auto identify format
		MS_NOATIME,
		NULL
	);

	if (ret != 0) {
		perror("mount_sd failed");
	}

	return ret;
}

// TODO: make mount directory be a variable somewhere, instead of a repeated hard-coded value
static void unmount_sd() {
	umount("/media");
}

// TODO: make this function more generic, in case device name differs
static void check_and_mount_existing_sd() {
    if (access("/dev/mmcblk0p1", F_OK) == 0) {
        printf("SD card device found at boot, mounting...\n");
        mount_sd();
    } else {
        printf("No SD card detected at boot.\n");
    }
}

// TODO: make this function more generic, in case device name differs
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

		// printf("%s\n", buf);
		char *p = buf;

		while (p < buf + len) {
		    printf("[%s]\n", p);
		    p += strlen(p) + 1;
		}

		puts("----");

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

void system_start_services() {
	check_and_mount_existing_sd(); // mount sd on startup, if it's present

    pthread_t sd_thread;
    if (pthread_create(&sd_thread, NULL, sd_hotplug_thread, NULL) != 0) {
	    fprintf(stderr, "Failed to start SD hotplug thread :(\n");
	} else {
		printf("Started SD hotplug thread :)\n");
	}
}
