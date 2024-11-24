#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "emufs.h"
#include "emufs_disk.h"

#define MAX_PATH_LENGTH 256

FILE *log_file;

// Function to get the current timestamp
void log_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
}

// Function to log a message
void log_transaction(const char *message) {
    char timestamp[64];
    log_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file);  // Ensure the message is written to the file immediately
}

void display_menu() {
    printf("\nFile System Manager\n");
    printf("1. Create New Device\n");
    printf("2. Mount Device\n");
    printf("3. Unmount Device\n");
    printf("4. View File System Metadata\n");
    printf("5. Change Directory\n");
    printf("6. Create Directory\n");
    printf("7. Create File\n");
    printf("8. Delete File\n");
    printf("9. Read File\n");
    printf("10. Write File\n");
    printf("11. Exit\n");
    printf("Enter your choice: ");
}

int main() {
    int choice, mount_point = -1, handle = -1, file_handle = -1, fileop = -1, fs_number = 0;
    char device_name[MAX_PATH_LENGTH], path[MAX_PATH_LENGTH];
    char buffer[BLOCKSIZE];
    int size = 1;

    // Open log file
    log_file = fopen("file_system_log.txt", "a");
    if (!log_file) {
        perror("Failed to open log file");
        return EXIT_FAILURE;
    }

    log_transaction("File System Manager started.");

    while (1) {
        display_menu();
        scanf("%d", &choice);
        getchar();  // Clear newline from the input buffer

        switch (choice) {
            case 1:
                printf("Enter device name: ");
                fgets(device_name, MAX_PATH_LENGTH, stdin);
                device_name[strcspn(device_name, "\n")] = 0;  // Remove newline
                mount_point = opendevice(device_name, MAX_BLOCKS);
                printf("Enter file system number (0 = non-encrypted, 1 = encrypted): ");
                scanf("%d", &choice);
                if (mount_point == -1) {
                    printf("Failed to mount device.\n");
                    log_transaction("Failed to mount device.");
                } else {
                    int handle = open_root(mount_point);
                    printf("Device mounted at mount point %d.\n", mount_point);
                    log_transaction("Device mounted successfully.");
                    if (create_file_system(mount_point, choice) != -1) {
                        printf("File system created successfully.\n");
                        log_transaction("File system created successfully.");
                    } else {
                        printf("Failed to create file system.\n");
                        log_transaction("Failed to create file system.");
                    }
                }
                break;

            case 2:
                printf("Enter device name: ");
                fgets(device_name, MAX_PATH_LENGTH, stdin);
                device_name[strcspn(device_name, "\n")] = 0;  // Remove newline
                mount_point = opendevice(device_name, MAX_BLOCKS);
                if (mount_point == -1) {
                    printf("Failed to mount device.\n");
                    log_transaction("Failed to mount device.");
                } else {
                    int handle = open_root(mount_point);
                    printf("Device mounted at mount point %d.\n", mount_point);
                    log_transaction("Device mounted successfully.");
                }
                break;

            case 3:
                if (closedevice(mount_point) == -1) {
                    printf("Failed to unmount device.\n");
                    log_transaction("Failed to unmount device.");
                } else {
                    printf("Device unmounted successfully.\n");
                    log_transaction("Device unmounted successfully.");
                    mount_point = -1;
                }
                break;

            case 4:
                if (mount_point != -1) {
                    fsdump(mount_point);
                    log_transaction("File system metadata viewed.");
                } else {
                    printf("No disk mounted\n");
                    log_transaction("Failed to view metadata: No disk mounted.");
                }
                break;

            case 5:
                printf("Enter path: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                if (change_dir(handle, path) == 1) {
                    printf("Directory changed successfully.\n");
                    log_transaction("Directory changed successfully.");
                } else {
                    printf("Failed to change directory.\n");
                    log_transaction("Failed to change directory.");
                }
                break;

            case 6:
                printf("Enter Directory name: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                if (emufs_create(handle, path, 1) == 1) {
                    printf("Directory created successfully.\n");
                    log_transaction("Directory created successfully.");
                } else {
                    printf("Failed to create directory.\n");
                    log_transaction("Failed to create directory.");
                }
                break;

            case 7:
                printf("Enter file name: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                if (emufs_create(handle, path, 0) == 1) {
                    printf("File created successfully.\n");
                    log_transaction("File created successfully.");
                } else {
                    printf("Failed to create file.\n");
                    log_transaction("Failed to create file.");
                }
                break;

            case 8:
                printf("Enter file name: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                if (emufs_delete(handle, path) == 1) {
                    printf("File deleted successfully.\n");
                    log_transaction("File deleted successfully.");
                } else {
                    printf("Failed to delete file.\n");
                    log_transaction("Failed to delete file.");
                }
                break;

            case 9:
                printf("Enter file name: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                fileop = open_file(handle, path);
                printf("Enter number of bytes to read: ");
                scanf("%d", &size);
                if (emufs_read(fileop, buffer, size) != -1) {
                    printf("Data read: %s\n", buffer);
                    log_transaction("File read successfully.");
                } else {
                    printf("Failed to read file.\n");
                    log_transaction("Failed to read file.");
                }
                break;

            case 10:
                printf("Enter file name: ");
                fgets(path, MAX_PATH_LENGTH, stdin);
                path[strcspn(path, "\n")] = 0;  // Remove newline
                fileop = open_file(handle, path);
                printf("Enter data to write: ");
                fgets(buffer, BLOCKSIZE, stdin);
                buffer[strcspn(buffer, "\n")] = 0;  // Remove newline
                if (emufs_write(fileop, buffer, strlen(buffer)) != -1) {
                    printf("Data written successfully.\n");
                    log_transaction("File written successfully.");
                } else {
                    printf("Failed to write to file.\n");
                    log_transaction("Failed to write to file.");
                }
                break;

            case 11:
                log_transaction("File System Manager exited.");
                fclose(log_file);
                printf("Exiting File System Manager.\n");
                return 0;

            default:
                printf("Invalid choice. Please try again.\n");
                log_transaction("Invalid choice entered.");
        }
    }
}
