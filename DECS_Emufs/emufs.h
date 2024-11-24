#include <stdio.h>      // Standard I/O functions
#include <stdlib.h>     // General utility functions
#include <sys/stat.h>   // Definitions for file status operations
#include <fcntl.h>      // File control options
#include <sys/types.h>  // Data types used in system calls
#include <unistd.h>     // POSIX API for system calls like read, write, etc.
#include <time.h>       // Time-related functions
#include <string.h>     // String manipulation functions

#define MAX_FILE_HANDLES 20  // Maximum number of file handles allowed
#define MAX_DIR_HANDLES 20   // Maximum number of directory handles allowed
#define MAX_MOUNT_POINTS 10  // Maximum number of mount points supported
#define MAX_ENTITY_NAME 8    // Maximum length of a file or directory name

/*-----------DEVICE------------*/

// Function to open a device
// `device_name` specifies the name of the device, `size` is the size of the device.
// Returns an integer representing the mount point of the device.
int opendevice(char *device_name, int size);

// Function to close a device
// `mount_point` specifies the mount point to close.
// Returns 0 on success or -1 on failure.
int closedevice(int mount_point);

// Function to display information about currently mounted devices.
void mount_dump(void);

/*-----------FILE SYSTEM API------------*/

// Function to create a file system on a specified mount point
// `mount_point` specifies the mount point, and `fs_number` identifies the file system type.
// Returns 0 on success or -1 on failure.
int create_file_system(int mount_point, int fs_number);

// Function to dump the file system's structure and metadata for debugging purposes
// `mount_point` specifies the mount point to dump.
void fsdump(int mount_point);

// Function to open the root directory of a specified mount point
// Returns a directory handle on success or -1 on failure.
int open_root(int mount_point);

// Function to change the current working directory
// `dir_handle` specifies the current directory handle, `path` specifies the target path.
// Returns 0 on success or -1 on failure.
int change_dir(int dir_handle, char* path);

// Function to open a file within the current directory
// `dir_handle` specifies the current directory handle, `path` specifies the file path.
// Returns a file handle on success or -1 on failure.
int open_file(int dir_handle, char* path);

// Function to create a new entity (file or directory) in the file system
// `dir_handle` specifies the current directory, `name` is the name of the new entity,
// `type` specifies the type (file or directory).
// Returns 0 on success or -1 on failure.
int emufs_create(int dir_handle, char* name, int type);

// Function to delete an entity (file or directory) in the file system
// `dir_handle` specifies the current directory, `path` specifies the entity to delete.
// Returns 0 on success or -1 on failure.
int emufs_delete(int dir_handle, char* path);

// Function to close a file or directory handle
// `handle` specifies the handle to close, `type` indicates whether it's a file or directory.
void emufs_close(int handle, int type);

// Function to read data from a file
// `file_handle` specifies the file, `buf` is the buffer to store data, `size` is the number of bytes to read.
// Returns the number of bytes read or -1 on failure.
int emufs_read(int file_handle, char* buf, int size);

// Function to write data to a file
// `file_handle` specifies the file, `buf` is the data to write, `size` is the number of bytes to write.
// Returns the number of bytes written or -1 on failure.
int emufs_write(int file_handle, char* buf, int size);

// Function to move the file pointer
// `file_handle` specifies the file, `nseek` is the number of bytes to move the pointer.
// Returns the new pointer position or -1 on failure.
int emufs_seek(int file_handle, int nseek);

// Uncomment these if AES encryption or decryption is needed
// void aes_encrypt_data(char* buf, int size, unsigned char* key); // Encrypt data using AES
// void aes_decrypt_data(char* buf, int size, unsigned char* key); // Decrypt data using AES
