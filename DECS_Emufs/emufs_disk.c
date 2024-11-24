
#include "emufs_disk.h"
#include "emufs.h"

struct mount_t mounts[MAX_MOUNT_POINTS];


/*-----------DEVICE------------*/
int writeblock(int dev_fd, int block, char* buf)
{
	/*
		* Writes the memory buffer to a block in the device

		* Return value: -1, error
						 1, success
	*/

	int ret;
	int offset;

	if(dev_fd < 0)
	{
		printf("Devices not found \n");
		return -1;
	}

	offset = block * BLOCKSIZE;
	lseek(dev_fd, offset, SEEK_SET);
	ret = write(dev_fd, buf, BLOCKSIZE);
	if(ret != BLOCKSIZE)
	{
		printf("Error: Disk write error. fd: %d. block: %d. buf: %p. ret: %d \n", dev_fd, block, buf, ret);
		return -1;
	}

	return 1;
}

int readblock(int dev_fd, int block, char * buf)
{
	/*
		* Writes a block in the device to the memory buffer

		* Return value: -1, error
						 1, success
	*/

	int ret;
	int offset;

	if(dev_fd < 0)
	{
		printf("Devices not found\n");
		return -1;
	}
	offset = block * BLOCKSIZE;
	lseek(dev_fd, offset, SEEK_SET);
	ret = read(dev_fd, buf, BLOCKSIZE);
	if(ret != BLOCKSIZE)
	{
		printf("Error: Disk read error. fd: %d. block: %d. buf: %p. ret: %d \n", dev_fd, block, buf, ret);
		return -1;
	}

	return 1;
}


/*-----------ENCRYPTION------------*/
void xor_encrypt(int key, char* buf, int size) {
    for (int i = 0; i < size; i++) {
        buf[i] ^= key; // XOR each byte with the key
    }
}

void xor_decrypt(int key, char* buf, int size) {
    for (int i = 0; i < size; i++) {
        buf[i] ^= key; // XOR each byte with the same key (symmetric)
    }
}


/*----------MOUNT-------*/
int add_new_mount_point(int file_des, char *dev_name, int num_fs)
{
	/*
		* Creates a mount for the device
		* Assigns an entry in the mount devices array

		* Return value: -1,									error
						array entry index (mount point)		success
	*/

	struct mount_t* mount_point = NULL;

	for(int i=0; i<MAX_MOUNT_POINTS; i++)
		if(mounts[i].device_fd <= 0 )
		{
			mount_point = &mounts[i];
			mount_point->device_fd = file_des;
			
			strcpy(mount_point->device_name, dev_name);
			mount_point->fs_number = num_fs;

			return i;
		}

	return -1;
}


int opendevice(char* dev_name, int sz)
{
	/*
		* Opens a device if it exists and do some consistency checks
		* Creates a device of given size if not present
		* Assigns a mount point

		* Return value: -1, 			error
						 mount point,	success	
	*/			

	int fd;
	FILE* fp;
	char tempBuf[BLOCKSIZE];
	struct superblock_t* superblock;
	int mount_point;
	int key;

	//checking if a valid device name is passed
	if(!dev_name || strlen(dev_name) == 0)
	{
		printf("Error: Device name INVALID \n");
		return -1;
	}

	//checking if size exceeds MAX BLOCK
	if(sz > MAX_BLOCKS || sz < 3)
	{
		printf("Error: Disk size INVALID \n");
		return -1;
	}

	superblock = (struct superblock_t*)malloc(sizeof(struct superblock_t));
	fp = fopen(dev_name, "r");

	//What is file does not open
	if(!fp)
	{
		//	Creating the device
		printf("[%s] Creating the disk image \n", dev_name);

		superblock->fs_number =  -1; 	//	No fs in the disk
		strcpy(superblock->device_name, dev_name);
		superblock->disk_size = sz;
		superblock->magic_number = MAGIC_NUMBER;	

		fp = fopen(dev_name, "w+");
		if(!fp)
		{
			printf("Error : Device COULD NOT be created \n");
			free(superblock);
			return -1;
		}
		fd = fileno(fp);

		// Disk size = Total size
		fseek(fp, sz * BLOCKSIZE, SEEK_SET);
		fputc('\0', fp);
		fseek(fp, 0, SEEK_SET);

		// Allocating disk to superblock
		memcpy(tempBuf, superblock, sizeof(struct superblock_t));
		writeblock(fd, 0, tempBuf);

		printf("[%s] Disk image SUCCESSFULLY created \n", dev_name);
	}
	
	//YES file did open
	else
	{
		fclose(fp);
		fd = open(dev_name, O_RDWR);

		readblock(fd, 0, tempBuf);
		memcpy(superblock, tempBuf, sizeof(struct superblock_t));
		if(superblock->fs_number==EMUFS_ENCRYPTED){
			printf("Input key: ");
			scanf("%d",&key);
			xor_decrypt(key, (char*)&(superblock->magic_number),4);
		}
		if(superblock->magic_number != MAGIC_NUMBER || superblock->disk_size < 3 || superblock->disk_size > MAX_BLOCKS)
		{
			printf("%d,%d,%d",superblock->magic_number,superblock->disk_size,superblock->disk_size);
			printf("Error: Inconsistent super block on device. \n");
			free(superblock);
			return -1;
		}
		printf("[%s] Disk opened \n", dev_name);

		if(superblock->fs_number == -1)
			printf("[%s] File system found in the disk \n", dev_name);
		else
			printf("[%s] File system found. fs_number: %d \n", dev_name, superblock->fs_number);
		
	}	

	mount_point = add_new_mount_point(fd, dev_name, superblock->fs_number);
	if(superblock->fs_number==1)
		mounts[mount_point].key=key;

	printf("[%s] Disk mount SUCCESS-> To infinity!!! \n", dev_name);
	free(superblock);

	return mount_point;	
}


int closedevice_(int mount_point)
{
	/*
		* Closes a device

		* Return value: -1, error
						 1, success
	*/

	char dev_name[20];

	if(mounts[mount_point].device_fd < 0)
	{
		printf("Error: Devices not found\n");
		return -1;
	}

	strcpy(dev_name, mounts[mount_point].device_name);
	close(mounts[mount_point].device_fd);

	mounts[mount_point].device_fd = -1;
	strcpy(mounts[mount_point].device_name, "\0");
	mounts[mount_point].fs_number = -1;

	printf("[%s] Device closed \n", dev_name);
	return 1;
}

void update_mount(int mount_point, int fs_number){

	int key;

    // Update the filesystem type (fs_number) for the specified mount point
    mounts[mount_point].fs_number = fs_number;

    // If the file system is encrypted (fs_number == 1), ask for the encryption key
    if (fs_number == 1) {
        printf("Input encryption key: ");
        
        // Read the key from the user input (ensure it's a valid integer)
        while (scanf("%d", &key) != 1 || key <= 0) {
            // If invalid input, prompt the user to enter a valid positive integer
            printf("Invalid key! Please enter a valid positive integer for the encryption key: ");
            // Clear the buffer to avoid infinite loop on invalid input
            while(getchar() != '\n');
        }
        
        // Store the encryption key in the mount structure
        mounts[mount_point].key = key;
        
        // Optionally, print a message that the key is set (you can remove this in production code)
        printf("Encryption key set for the mounted file system.\n");
    } else {
        // If not encrypted, we can reset the key (this part is optional)
        mounts[mount_point].key = 0;
    }
	/*
		update the fstype in the mount and prompt the user for key if its an encryted system
	*/
}

void mount_dump(void)
{
	/*
		* Prints summary of mount points
	*/

	struct mount_t* mount_point;

	printf("\n%-12s %-20s %-15s %-10s %-20s \n", "MOUNT-POINT", "DEVICE-NAME", "DEVICE-NUMBER", "FS-NUMBER", "FS-NAME");
	for(int i=0; i< MAX_MOUNT_POINTS; i++)
	{
		mount_point = mounts + i;
		if(mount_point->device_fd <= 0)
			continue;

		if(mount_point->device_fd > 0)
			printf("%-12d %-20s %-15d %-10d %-20s\n", 
					i, mount_point->device_name, mount_point->device_fd, mount_point->fs_number, 
					mount_point->fs_number == EMUFS_NON_ENCRYPTED ? "emufs non-encrypted" : (mount_point->fs_number == EMUFS_ENCRYPTED ? "emufs encrypted" : "Unknown file system"));
	}
}

void read_superblock(int mount_point, struct superblock_t *superblock){
	/*	
		* Reads the superblock of the device
		* If its an encrypted system, decrypts the magic number after reading
	*/

	char tempBuf[BLOCKSIZE];
	readblock(mounts[mount_point].device_fd, 0, tempBuf);
	if(mounts[mount_point].fs_number==1)
		xor_decrypt(mounts[mount_point].key, tempBuf, 4);
	memcpy(superblock, tempBuf, sizeof(struct superblock_t));
}

void write_superblock(int mount_point, struct superblock_t *superblock){
	/*
		* Updates the superblock of the device
		* If its an encrypted system, encrypts the magic number before writing
	*/

	char tempBuf[BLOCKSIZE];
	memcpy(tempBuf, superblock, sizeof(struct superblock_t));
	if(mounts[mount_point].fs_number==1)
		xor_encrypt(mounts[mount_point].key, tempBuf, 4);
	writeblock(mounts[mount_point].device_fd, 0, tempBuf);
}

int alloc_inode(int mount_point) {
    /*
        Function: alloc_inode
        Purpose: This function allocates a free inode if available, by checking the inode bitmap.
        Updates the inode bitmap and the number of used inodes accordingly.

        Parameters:
            mount_point: The mount point to identify the filesystem.

        Return value:
            -1: if no free inodes are available (error).
            inode number: if a free inode is allocated successfully (success).
    */

    // Structure to hold the superblock data
    struct superblock_t superblock;

    // Read the superblock data from the disk
    read_superblock(mount_point, &superblock);

    // If all inodes are already used, return error (-1)
    if(superblock.used_inodes == MAX_INODES)
        return -1;

    // Loop through the inode bitmap to find a free inode
    for(int i = 0; i < MAX_INODES; i++) {
        if(superblock.inode_bitmap[i] == 0) { // Check for an unused inode
            // Mark this inode as used and increment the used inode count
            superblock.inode_bitmap[i] = 1;
            superblock.used_inodes++;

            // Write the updated superblock data back to the disk
            write_superblock(mount_point, &superblock);

            // Return the index of the allocated inode
            return i;
        }
    }

    // Return error if no free inodes found (though this should not happen)
    return -1;
}


void free_inode(int mount_point, int inodenum){
	/*
	 * Marks the specified inode as free in the inode bitmap
	 * and updates the count of used inodes in the superblock.
	 */
	
	// Define a variable to hold the superblock structure
	struct superblock_t superblock;
	
	// Read the current superblock information from the disk
	read_superblock(mount_point, &superblock);
	
	// Set the corresponding inode bit to 0 (free) in the inode bitmap
	superblock.inode_bitmap[inodenum] = 0;
	
	// Decrease the count of used inodes in the superblock
	superblock.used_inodes--;
	
	// Write the updated superblock back to disk
	write_superblock(mount_point, &superblock);
}


void read_inode(int mount_point, int inodenum, struct inode_t *inodeptr){
    /*
        * This function retrieves the inode metadata from the storage.
        * It reads the appropriate block (block 2 or 3) based on the inode number.
        * If the file system is encrypted, it decrypts the metadata block using a XOR operation.
        * The specific inode entry is extracted from the metadata block and placed into the provided inode pointer.
    */

    struct metadata_t metadata;  // Structure to hold the metadata block read from the disk.
    int block_offset = inodenum / 16;  // Determine the block offset based on inode number.
    int entry_index = inodenum % 16;  // Calculate the index of the inode within the block.

    // Read the metadata block from the disk. Offset depends on inode number.
    readblock(mounts[mount_point].device_fd, 1 + block_offset, (char*)&metadata);

    // If the filesystem is encrypted (fs_number == 1), decrypt the metadata block.
    if (mounts[mount_point].fs_number == 1) {
        xor_decrypt(mounts[mount_point].key, (char*)&metadata, BLOCKSIZE);
    }

    // Copy the inode entry corresponding to the given index into the provided inode pointer.
    *inodeptr = metadata.inodes[entry_index];
}


void write_inode(int mount_point, int inodenum, struct inode_t *inodeptr) {
    /*
        This function is responsible for updating the inode entry in the 
        filesystem's metadata block. The steps are as follows:
        1. Identify the appropriate metadata block based on the inode number.
        2. If the filesystem is encrypted, decrypt the block before modification.
        3. Update the inode entry at the correct location within the block.
        4. If the filesystem is encrypted, re-encrypt the block after modification.
        5. Write the updated metadata block back to the disk.
    */

    struct metadata_t metadata;
    int block_idx = inodenum / 16;  // Determine the block index based on inode number
    int entry_idx = inodenum % 16;  // Determine the position within the block

    // Read the metadata block from the storage device
    readblock(mounts[mount_point].device_fd, 1 + block_idx, (char*)&metadata);

    // Decrypt the block if the filesystem is encrypted
    if (mounts[mount_point].fs_number == 1) {
        xor_decrypt(mounts[mount_point].key, (char*)&metadata, BLOCKSIZE);
    }

    // Update the inode entry in the metadata block
    metadata.inodes[entry_idx] = *inodeptr;

    // Encrypt the block again if the filesystem is encrypted
    if (mounts[mount_point].fs_number == 1) {
        xor_encrypt(mounts[mount_point].key, (char*)&metadata, BLOCKSIZE);
    }

    // Write the modified metadata block back to the device
    writeblock(mounts[mount_point].device_fd, 1 + block_idx, (char*)&metadata);
}


// Function to allocate a new data block
// Returns the index of the allocated block or -1 if no blocks are available
int alloc_datablock(int mount_point) {
    // Structure representing the superblock
    struct superblock_t superblock;
    
    // Read the superblock to get current disk information
    read_superblock(mount_point, &superblock);

    // Check if all blocks are used; return -1 if disk is full
    if(superblock.used_blocks == superblock.disk_size)
        return -1;

    // Iterate through the block bitmap to find a free block
    for(int i = 0; i < MAX_BLOCKS; i++) {
        // If the block is free (marked as 0), allocate it
        if(superblock.block_bitmap[i] == 0) {
            superblock.block_bitmap[i] = 1;   // Mark block as used
            superblock.used_blocks++;         // Increment the count of used blocks
            // Write the updated superblock back to disk
            write_superblock(mount_point, &superblock);
            return i;  // Return the index of the allocated block
        }
    }

    // In case no free block was found, return -1 (shouldn't reach here if the check above passes)
    return -1;
}


void free_datablock(int mount_point, int blocknum){
    /*
        * This function marks the specified data block as free.
        * It updates the block bitmap and the number of used blocks in the superblock.
    */

    struct superblock_t superblock;  // Declare a structure to hold the superblock data.
    
    // Read the current superblock data from the specified mount point.
    read_superblock(mount_point, &superblock);
    
    // Mark the block as free in the block bitmap by setting the corresponding entry to 0.
    superblock.block_bitmap[blocknum] = 0;
    
    // Decrement the count of used blocks in the superblock.
    superblock.used_blocks--;
    
    // Write the updated superblock data back to the storage.
    write_superblock(mount_point, &superblock);
}

void read_datablock(int mount_point, int blocknum, char *buf){
	/*
		* Read the specified block of data into the provided buffer.
		* If the system uses encryption, decrypt the block before storing it in memory.
	*/

	// Read the block of data from the device into the buffer
	readblock(mounts[mount_point].device_fd, blocknum, buf);

	// Check if the filesystem uses encryption (fs_number == 1) and decrypt if necessary
	if(mounts[mount_point].fs_number == 1)
		xor_decrypt(mounts[mount_point].key, buf, BLOCKSIZE);
}

void write_datablock(int mount_point, int blocknum, char *buf){
	/*
		* If the system is encrypted, encrypt the buffer before writing.
		* Write the processed buffer to the corresponding block on the disk.
	*/

	// Encrypt the buffer if the filesystem uses encryption
	if(mounts[mount_point].fs_number == 1)
		xor_encrypt(mounts[mount_point].key, buf, BLOCKSIZE);

	// Write the buffer data to the specified block on the device
	writeblock(mounts[mount_point].device_fd, blocknum, buf);
}
