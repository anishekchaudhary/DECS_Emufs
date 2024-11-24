#include <sys/types.h>  // Required for defining data types like u_int16_t
#include <stdio.h>      // Standard I/O functions
#include <stdlib.h>     // General utility functions
#include <sys/stat.h>   // Definitions for file status operations
#include <fcntl.h>      // File control options
#include <sys/types.h>  // Data types used in system calls
#include <unistd.h>     // POSIX API for system calls like read, write, etc.
#include <time.h>       // Time-related functions
#include <string.h>     // String manipulation functions

// Definitions for the filesystem's configuration and constraints
#define BLOCKSIZE 256          // Size of a block in bytes
#define MAX_BLOCKS 64          // Maximum number of blocks on the disk
                               // Includes 1 superblock, 1 metadata block, and 40 data blocks
#define MAX_FILE_SIZE 4        // Maximum file size in blocks (4 blocks per file)
#define MAX_INODES 32          // Maximum number of inodes supported

// States for resource allocation
#define UNUSED 0               // Represents an unused resource (inode or block)
#define USED 1                 // Represents an allocated resource

#define MAGIC_NUMBER 6763      // Unique identifier for the filesystem type

// File system types
#define EMUFS_NON_ENCRYPTED 0  // Non-encrypted filesystem
#define EMUFS_ENCRYPTED 1      // Encrypted filesystem

/* ------------------- In-Disk objects ------------------- */

// Structure to represent the superblock of the filesystem
struct superblock_t
{
    int magic_number;                   // Unique identifier for the filesystem
    char device_name[20];	            // Name of the device (e.g., disk image file)
    int disk_size;		                // Size of the device in blocks
    int fs_number;		                // Filesystem type (-1 = no FS, 0 = non-encrypted, 1 = encrypted)
    char used_inodes;					// Number of inodes currently in use
    char used_blocks;					// Number of blocks currently in use
    char inode_bitmap[MAX_INODES];      // Bitmap for inode allocation (0 = free, 1 = allocated)
    char block_bitmap[MAX_BLOCKS];    	// Bitmap for block allocation (0 = free, 1 = allocated)
};

// Structure to represent an inode
struct inode_t		// 16 bytes in size
{
    char name[8];		    	// Name of the file or directory (max 8 characters)
    char type;                  // Type of the entity (0 = file, 1 = directory)
    char parent;                // Parent directory's inode number
    u_int16_t size;				// Size of the file in bytes
    char mappings[4];		    // Block mappings for the file
                                // mappings[i] = -1 : block is not allocated
                                // mappings[i] > 0  : block number
};

// Structure to represent metadata (array of inodes)
// This is stored in the metadata block of the disk
struct metadata_t	// 256 bytes (BLOCKSIZE)
{
    struct inode_t inodes[BLOCKSIZE / 16];	// Array of inodes (16 bytes each)
};

/* ------------------- In-Memory objects ------------------- */

// Structure to represent a mounted device
struct mount_t
{
    int device_fd;		        // File descriptor of the emulated device file
					            // <= 0: Unused mount point
					            //  > 0: Active file descriptor
    char device_name[20]; 	    // Name of the emulated device file
    int fs_number;              // Filesystem type (non-encrypted or encrypted)
    int key;                    // Encryption key (used only for encrypted filesystems)
};

/*--------Device--------------*/

// Function to close a device by its mount point
// `mount_point` is the index of the mounted device
// Returns 0 on success, -1 on failure
int closedevice_(int mount_point);

// Function to update the filesystem type for a given mount point
// `mount_point` is the index, `fs_number` is the new filesystem type
void update_mount(int mount_point, int fs_number);

/*-----------FILE SYSTEM API------------*/

// Function to read the superblock from a mounted device
// `mount_point` specifies the mount index, `superblock` is the structure to populate
void read_superblock(int mount_point, struct superblock_t *superblock);

// Function to write the superblock to a mounted device
// `mount_point` specifies the mount index, `superblock` contains the updated data
void write_superblock(int mount_point, struct superblock_t *superblock);

// Function to allocate a new inode
// `mount_point` specifies the mounted device
// Returns the index of the allocated inode or -1 if no inodes are available
int alloc_inode(int mount_point);

// Function to free a previously allocated inode
// `mount_point` specifies the device, `inodenum` is the inode number to free
void free_inode(int mount_point, int inodenum);

// Function to read an inode's data from the disk
// `mount_point` specifies the device, `inodenum` is the inode index, `inodeptr` is the buffer to store data
void read_inode(int mount_point, int inodenum, struct inode_t *inodeptr);

// Function to write an inode's data to the disk
// `mount_point` specifies the device, `inodenum` is the inode index, `inodeptr` contains the data to write
void write_inode(int mount_point, int inodenum, struct inode_t *inodeptr);

// Function to allocate a new data block
// `mount_point` specifies the mounted device
// Returns the index of the allocated block or -1 if no blocks are available
int alloc_datablock(int mount_point);

// Function to free a previously allocated data block
// `mount_point` specifies the device, `blocknum` is the block number to free
void free_datablock(int mount_point, int blocknum);

// Function to read data from a specific block
// `mount_point` specifies the device, `blocknum` is the block number, `buf` is the buffer to store data
void read_datablock(int mount_point, int blocknum, char *buf);

// Function to write data to a specific block
// `mount_point` specifies the device, `blocknum` is the block number, `buf` contains the data to write
void write_datablock(int mount_point, int blocknum, char *buf);
