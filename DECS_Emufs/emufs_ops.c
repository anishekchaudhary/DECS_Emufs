#include "emufs_disk.h"
#include "emufs.h"

/* ------------------- In-Memory objects ------------------- */

int init=0; //if the file/directory handles arrays are initialized or not

struct file_t
{
	int offset;		                // offset of the file
	int inode_number;	            // inode number of the file in the disk
	int mount_point;    			// reference to mount point
                                    // -1: Free
                                    // >0: In Use
};

struct directory_t
{
    int inode_number;               // inode number of the directory in the disk
    int mount_point;    			// reference to mount point
                                    // -1: Free
                                    // >0: In Use
};


struct directory_t dir[MAX_DIR_HANDLES];    // array of directory handles
struct file_t files[MAX_FILE_HANDLES];      // array of file handles

int closedevice(int mount_point){
    /*
        * Close all the associated handles
        * Unmount the device
        
        * Return value: -1,     error
                         1,     success
    */

    for(int i=0; i<MAX_DIR_HANDLES; i++)
        dir[i].mount_point = (dir[i].mount_point==mount_point ? -1 : dir[i].mount_point);
    for(int i=0; i<MAX_FILE_HANDLES; i++)
        files[i].mount_point = (files[i].mount_point==mount_point ? -1 : files[i].mount_point);
    
    return closedevice_(mount_point);
}

int create_file_system(int mount_point, int fs_number){
    /*
	   	* Read the superblock.
        * Update the mount point with the file system number
	    * Set file system number on superblock
		* Clear the bitmaps.  values on the bitmap will be either '0', or '1'. 
        * Update the used inodes and blocks
		* Create Inode 0 (root) in metadata block in disk
		* Write superblock and metadata block back to disk.

		* Return value: -1,		error
						 1, 	success
	*/
    struct superblock_t superblock;
    read_superblock(mount_point, &superblock);

    update_mount(mount_point, fs_number);

    superblock.fs_number=fs_number;
    for(int i=3; i<MAX_BLOCKS; i++)
        superblock.block_bitmap[i]=0;
    for(int i=0; i<3; i++)
        superblock.block_bitmap[i]=1;
    for(int i=1; i<MAX_INODES; i++)
        superblock.inode_bitmap[i]=0;
    superblock.inode_bitmap[0]=1;
    superblock.used_blocks=3;
    superblock.used_inodes=1;
    write_superblock(mount_point, &superblock);

    struct inode_t inode;
    memset(&inode,0,sizeof(struct inode_t));
    inode.name[0]='/';
    inode.parent=255;
    inode.type=1;
    write_inode(mount_point, 0, &inode);
}

int alloc_dir_handle(){
    /*
        * Initialize the arrays if not already done
        * check and return if there is any free entry
        
		* Return value: -1,		error
						 1, 	success
    */
    if(init==0){
        for(int i=0; i<MAX_DIR_HANDLES; i++)
            dir[i].mount_point = -1;
        for(int i=0; i<MAX_FILE_HANDLES; i++)
            files[i].mount_point = -1;
        init=1;
    }
    for(int i=0; i<MAX_DIR_HANDLES; i++)
        if(dir[i].mount_point==-1)
            return i;
    return -1;
}

int alloc_file_handle(){
    for(int i=0; i<MAX_FILE_HANDLES; i++)
        if(files[i].mount_point==-1)
            return i;
    return -1;
}

int goto_parent(int dir_handle){
    /*
        * Update the dir_handle to point to the parent directory
        
		* Return value: -1,		error   (If the current directory is root)
						 1, 	success
    */

    struct inode_t inode;
    read_inode(dir[dir_handle].mount_point, dir[dir_handle].inode_number, &inode);
    if(inode.parent==255)
        return -1;
    dir[dir_handle].inode_number = inode.parent;
    return 1;
}

int open_root(int mount_point) {
    /*
     * Opens a handle to the root directory of the specified mount point.
     *
     * Parameters:
     *   mount_point - Identifier for the mount point to access.
     *
     * Return value:
     *   -1 if no free directory handles are available or an error occurs.
     *   A valid directory handle index on success.
     */

    // Allocate a directory handle; returns -1 if no handles are available
    int dir_handle = alloc_dir_handle();
    if (dir_handle == -1) {
        return -1; // Return error if handle allocation fails
    }

    // Set the mount point and root inode number for the allocated directory handle
    dir[dir_handle].mount_point = mount_point; // Assign mount point to the handle
    dir[dir_handle].inode_number = 0;         // Root directory always has inode number 0

    // Return the allocated directory handle
    return dir_handle;
}


int return_inode(int mount_point, int inodenum, char* path) {
    /*
        Function to traverse a filesystem path and return the inode number of the specified entity.
        Parameters:
        - mount_point: The mount point identifier.
        - inodenum: The starting inode number.
        - path: The path to traverse.

        Return Values:
        - -1: Error (invalid path or inaccessible directory).
        - Inode number: Success.
    */

    // If the path starts with '/', begin traversal from the root inode (inodenum = 0).
    if (path[0] == '/')
        inodenum = 0;

    struct inode_t inode;
    read_inode(mount_point, inodenum, &inode);

    // Check if the starting inode is not a directory (invalid starting point).
    if (inode.type == 0)
        return -1;

    int ptr1 = 0, ptr2 = 0;   // Pointers for path traversal and entity name construction.
    char buf[MAX_ENTITY_NAME];
    memset(buf, 0, MAX_ENTITY_NAME);   // Initialize buffer to store entity names.

    // Traverse the path character by character.
    while (path[ptr1]) {
        if (path[ptr1] == '/') {
            // Skip redundant slashes.
            ptr1++;
            continue;
        }

        if (path[ptr1] == '.') {
            ptr1++;
            // Handle special cases for '.' and '..'.
            if (path[ptr1] == '/' || path[ptr1] == 0)
                continue;   // Current directory, no action needed.

            if (path[ptr1] == '.') {
                ptr1++;
                // Handle parent directory ('..').
                if (path[ptr1] == '/' || path[ptr1] == 0) {
                    if (inodenum == 0) // Root has no parent.
                        return -1;
                    inodenum = inode.parent;
                    read_inode(mount_point, inodenum, &inode);
                    continue;
                }
            }
            return -1;   // Invalid path syntax.
        }

        // Parse the directory name and look for a match.
        while (1) {
            int found = 0;   // Flag to check if the entity is found in the directory.
            buf[ptr2++] = path[ptr1++];

            // End of entity name detected.
            if (path[ptr1] == 0 || path[ptr1] == '/') {
                for (int i = 0; i < inode.size; i++) {
                    struct inode_t entry;
                    read_inode(mount_point, inode.mappings[i], &entry);

                    // Compare the buffer content with directory entries.
                    if (memcmp(buf, entry.name, MAX_ENTITY_NAME) == 0) {
                        inodenum = inode.mappings[i];
                        inode = entry;

                        // Ensure directories are correctly handled.
                        if (path[ptr1] == '/' && entry.type == 0)
                            return -1;

                        ptr2 = 0;   // Reset buffer for the next entity name.
                        memset(buf, 0, MAX_ENTITY_NAME);
                        found = 1;
                        break;
                    }
                }

                // If no matching entity is found in the directory.
                if (!found)
                    return -1;

                break;   // Proceed to the next segment of the path.
            }

            // If the entity name exceeds the maximum allowed length.
            if (ptr2 == MAX_ENTITY_NAME)
                return -1;
        }
    }
    return inodenum;   // Successfully resolved the path.
}


int change_dir(int dir_handle, char* path) {
    /*
     * Function: change_dir
     * --------------------
     * Updates the directory handle to point to the directory specified by the given path.
     *
     * Parameters:
     *   dir_handle - Handle representing the current directory context.
     *   path - Path to the target directory.
     *
     * Returns:
     *   1  - Success, directory updated.
     *  -1  - Failure, error in finding or accessing the target directory.
     */

    // Get the mount point for the current directory handle.
    int mnt = dir[dir_handle].mount_point;

    // Retrieve the inode number of the target directory based on the given path.
    int inodenum = return_inode(mnt, dir[dir_handle].inode_number, path);
    if (inodenum == -1) {
        // Return error if the inode for the path could not be found.
        return -1;
    }

    // Define an inode structure to store information about the target directory.
    struct inode_t inode;

    // Read the target directory's inode using the retrieved inode number.
    read_inode(mnt, inodenum, &inode);

    // Check if the target inode represents a directory and update the handle if valid.
    if (inode.type) {
        dir[dir_handle].inode_number = inodenum;
    }

    // Return success if it's a valid directory, otherwise return an error.
    return inode.type ? 1 : -1;
}


void emufs_close(int handle, int type){
    /*
        * type = 1 : Indicates Directory handle
        * type = 0 : Indicates File handle
        * This function closes the file or directory by updating the respective mount point to -1.
    */

    // Check if it's a directory handle
    if(type) {
        // Mark the directory handle as closed by setting the mount point to -1
        dir[handle].mount_point = -1;
    }
    else {
        // Mark the file handle as closed by setting the mount point to -1
        files[handle].mount_point = -1;
    }
}


int delete_entity(int mount_point, int inodenum){
    /*
        * Delete the entity denoted by inodenum (inode number)
        * Close all the handles associated
        * If its a file then free all the allocated blocks
        * If its a directory call delete_entity on all the entities present
        * Free the inode
        
        * Return value : inode number of the parent directory
    */

    struct inode_t inode;
    read_inode(mount_point, inodenum, &inode);
    if(inode.type==0){
        for(int i=0; i<MAX_FILE_HANDLES; i++)
            if(files[i].inode_number==inodenum)
                files[i].mount_point=-1;
        int num_blocks = inode.size/BLOCKSIZE;
        if(num_blocks*BLOCKSIZE<inode.size)
            num_blocks++;
        for(int i=0; i<num_blocks; i++)
            free_datablock(mount_point, inode.mappings[i]);
        free_inode(mount_point, inodenum);
        return inode.parent;
    }

    for(int i=0; i<MAX_DIR_HANDLES; i++)
        if(dir[i].inode_number==inodenum)
            dir[i].mount_point=-1;
    
    for(int i=0; i<inode.size; i++)
        delete_entity(mount_point, inode.mappings[i]);
    free_inode(mount_point, inodenum);
    return inode.parent;
}

int emufs_delete(int dir_handle, char* path) {
    /*
        * Function to delete a file or directory at the given path.
        * The function first locates the inode of the entity to be deleted, then removes its entry
        * from the parent directory's inode. After adjusting the directory mappings, the inode is updated
        * back to disk to reflect the deletion.
        * 
        * Steps:
        * 1. Retrieve the mount point of the directory associated with the provided directory handle.
        * 2. Use the return_inode function to find the inode number corresponding to the given path.
        * 3. If the entity exists, use delete_entity to remove it and get the parent inode.
        * 4. Modify the parent directory's mappings by shifting entries to remove the reference to the deleted entity.
        * 5. Update the parent directory's inode and write it back to disk.
        * 
        * Return Value:
        *  -1 if an error occurs (e.g., invalid directory handle, entity not found).
        *   1 if the deletion was successful.
    */

    // Retrieve the mount point from the directory handle.
    int mnt = dir[dir_handle].mount_point;
    if (mnt == -1) {
        printf("Invalid directory handle.\n");
        return -1;  // Return error if mount point is invalid.
    }

    // Get the inode number for the entity located at the given path.
    int inodenum = return_inode(mnt, dir[dir_handle].inode_number, path);
    if (inodenum <= 0) {
        return -1;  // Return error if the entity is not found.
    }

    // Delete the entity and get the parent inode number.
    int par = delete_entity(mnt, inodenum);
    if (par < 0) {
        return -1;  // Return error if entity deletion failed.
    }

    // Read the parent directory's inode to modify its mappings.
    struct inode_t inode;
    read_inode(mnt, par, &inode);

    // Loop through the directory's mappings to find and remove the entry for the deleted entity.
    int del = 0;
    for (int i = 0; i < inode.size; i++) {
        // Shift the remaining entries left after finding the deleted entry.
        if (del) {
            inode.mappings[i - 1] = inode.mappings[i];
        }
        // Mark the entry for deletion when found.
        if (inode.mappings[i] == inodenum) {
            del = 1;
        }
    }

    // Decrease the directory's size and write the updated inode back to disk.
    inode.size--;
    write_inode(mnt, par, &inode);

    return 1;  // Return success after deletion is completed.
}


int emufs_create(int dir_handle, char* name, int type) {
    /*
        * This function creates either a directory (type = 1) or a file (type = 0) within the directory specified by dir_handle.
        * It ensures that no directory or file with the same name already exists within the specified directory.
        * It is possible to have both a file and a directory with the same name in the same directory, as they are of different types.
        
        * Parameters:
        * dir_handle - Handle to the directory where the file or directory should be created.
        * name - The name of the new file or directory.
        * type - Type of the new entity (0 for file, 1 for directory).
        
        * Returns:
        * -1 on error (e.g., invalid name or duplicate entity).
        * 1 on success (creation of the new entity).
    */

    // Check if the name exceeds the maximum allowed length
    if (strlen(name) > MAX_ENTITY_NAME) {
        return -1;
    }

    // Ensure the name is valid: not empty, not starting with '/', and not starting with '.'
    if (name[0] == 0 || name[0] == '/' || name[0] == '.') {
        return -1;
    }

    // Initialize a structure to hold the inode information
    struct inode_t inode;

    // Retrieve the mount point for the directory and the corresponding inode
    int mount_point = dir[dir_handle].mount_point;
    read_inode(mount_point, dir[dir_handle].inode_number, &inode);

    // Create a temporary buffer to hold the name of the new entity
    char ename[MAX_ENTITY_NAME];
    memset(ename, 0, MAX_ENTITY_NAME);
    for (int i = 0; i < strlen(name); i++) {
        ename[i] = name[i];
    }

    // If the directory is a special reserved one (inode size = 4), return error
    if (inode.size == 4) {
        return -1;
    }

    // Check if an entity with the same name and type already exists in the directory
    for (int i = 0; i < inode.size; i++) {
        struct inode_t entry;
        read_inode(mount_point, inode.mappings[i], &entry);

        // If an entry with the same name and type exists, return error
        if (entry.type == type) {
            if (memcmp(ename, entry.name, MAX_ENTITY_NAME) == 0) {
                return -1;
            }
        }
    }

    // Allocate a new inode for the new entity
    int new_inodenum = alloc_inode(mount_point);
    inode.mappings[inode.size] = new_inodenum;
    inode.size++;

    // Write the updated inode back to the filesystem
    write_inode(mount_point, dir[dir_handle].inode_number, &inode);

    // Initialize the new inode and assign the appropriate attributes
    inode.size = 0;
    inode.parent = dir[dir_handle].inode_number;
    inode.type = type;
    memcpy(inode.name, ename, MAX_ENTITY_NAME);

    // Write the new inode to the filesystem
    write_inode(mount_point, new_inodenum, &inode);

    return 1;  // Indicate successful creation
}


int open_file(int dir_handle, char* path) {
    /*
        * This function opens a file denoted by the given path within the directory specified by dir_handle.
        * It retrieves the inode for the file using the return_inode function.
        * A file handle is then allocated using alloc_file_handle.
        * The file handle is initialized with the relevant mount point, inode number, and an offset of 0.
        
        * Return value:
        *   -1: In case of an error (e.g., invalid inode or failure to allocate file handle)
        *   File handle number (>= 0): On success, returning a valid file handle
    */

    // Retrieve the mount point associated with the directory handle
    int mnt = dir[dir_handle].mount_point;

    // Retrieve the inode number for the file at the specified path
    int inodenum = return_inode(mnt, dir[dir_handle].inode_number, path);
    if (inodenum == -1) {
        // If inode retrieval fails, return error
        return -1;
    }

    // Create an inode structure to store the inode details
    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // Check if the inode type is valid (e.g., not a directory or special type)
    if (inode.type) {
        // If the inode type is not suitable (like a directory), return error
        return -1;
    }

    // Allocate a new file handle
    int file_handle = alloc_file_handle();
    if (file_handle == -1) {
        // If file handle allocation fails, return error
        return -1;
    }

    // Initialize the file handle with the mount point, inode number, and offset
    files[file_handle].mount_point = mnt;
    files[file_handle].inode_number = inodenum;
    files[file_handle].offset = 0;

    // Return the allocated file handle
    return file_handle;
}


int emufs_read(int file_handle, char* buf, int size){
    /*
        * Function to read a specified number of bytes from a file into a buffer.
        * The read starts from the current seek offset and reads up to the given size.
        * The seek offset in the file is updated by the size of the data read.
        * 
        * Parameters:
        *   file_handle: An integer that identifies the file to be read.
        *   buf: A pointer to a buffer where the read data will be stored.
        *   size: The number of bytes to read from the file.
        * 
        * Returns:
        *   -1 if an error occurs (e.g., invalid read range).
        *   1 if the read operation is successful.
        */

    // Retrieve the mount point, current seek position, and inode number for the file.
    int mnt = files[file_handle].mount_point;
    int seek = files[file_handle].offset;
    int inodenum = files[file_handle].inode_number;

    // Read the inode to access file metadata such as size and block mappings.
    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // If the file's size is smaller than the requested read size, return an error.
    if(inode.size < seek + size)
        return -1;
    
    // Temporary buffer to hold data read from each block.
    char temp_buf[BLOCKSIZE];

    // Loop through the file's data blocks to read the requested size.
    for(int i = seek / BLOCKSIZE; i * BLOCKSIZE < (seek + size); i++){
        // Calculate the start (a) and end (b) positions for the current block's data.
        int a = i * BLOCKSIZE > seek ? i * BLOCKSIZE : seek;  // Start of the current block to read.
        int b = (i + 1) * BLOCKSIZE < (seek + size) ? (i + 1) * BLOCKSIZE : (seek + size);  // End of the current block to read.

        // Read the data block into the temporary buffer.
        read_datablock(mnt, inode.mappings[i], temp_buf);

        // Copy the relevant portion of the block into the provided buffer.
        memcpy(buf + a - seek, temp_buf + a - i * BLOCKSIZE, b - a);
    }

    // Update the file's seek offset to reflect the number of bytes read.
    files[file_handle].offset += size;

    // Return success status.
    return 1;
}


int emufs_write(int file_handle, char* buf, int size){
    /*
        * This function writes a chunk of data from the provided buffer to the file starting from the current seek offset.
        * It handles writing to file blocks that may not align with the block size, ensuring that partial blocks are correctly written.
        * The inode is updated if the file's size or mapping changes.
        * The file handle’s offset is updated to reflect the new position after the write.
        
        * Return value:
            -1: error occurred (e.g., invalid size or insufficient space)
            1: success (data written successfully)
    */

    // Get the mount point, seek position, and inode number of the file being written to
    int mnt = files[file_handle].mount_point;
    int seek = files[file_handle].offset;
    int inodenum = files[file_handle].inode_number;

    // Check if the requested write goes beyond the maximum allowed file size
    if(seek + size > BLOCKSIZE * MAX_FILE_SIZE)
        return -1;

    // Read the superblock to gather information about available space
    struct superblock_t superblock;
    read_superblock(mnt, &superblock);

    // Read the inode to get file metadata (size, mappings, etc.)
    struct inode_t inode;
    read_inode(mnt, inodenum, &inode);

    // If the new write size extends the current file size, check if there is enough space
    if(seek + size > inode.size){
        int num_req;
        int k = (seek + size) / BLOCKSIZE;
        num_req = k;
        
        // If size isn't a multiple of BLOCKSIZE, increase the required block count
        if(k * BLOCKSIZE < (seek + size))
            num_req++;
        
        // Determine how many additional blocks are needed to extend the file
        k = inode.size / BLOCKSIZE;
        num_req -= k;
        if(k * BLOCKSIZE < inode.size)
            num_req--;
        
        // If there aren't enough free blocks in the disk, return an error
        if(superblock.disk_size - superblock.used_blocks < num_req)
            return -1;
    }

    // Temporary buffer for reading and writing blocks
    char temp_buf[BLOCKSIZE];
    int num_blocks = inode.size / BLOCKSIZE;
    
    // Adjust number of blocks if file size isn't an exact multiple of BLOCKSIZE
    if(num_blocks * BLOCKSIZE < inode.size)
        num_blocks++;

    // Loop through the blocks affected by the write operation
    for(int i = seek / BLOCKSIZE; i * BLOCKSIZE < (seek + size); i++){
        int a, b;
        // Determine the start and end positions of the data to write within the block
        a = i * BLOCKSIZE > seek ? i * BLOCKSIZE : seek;
        b = (i + 1) * BLOCKSIZE < (seek + size) ? (i + 1) * BLOCKSIZE : (seek + size);
        
        // If this is a new block, allocate it and write the data from the buffer
        if(i == num_blocks){
            inode.mappings[i] = alloc_datablock(mnt);
            memcpy(temp_buf, buf + a - seek, b - a);
            write_datablock(mnt, inode.mappings[i], temp_buf);
            num_blocks++;
        }
        // If the block is already allocated, read it, update it, and write it back
        else{
            read_datablock(mnt, inode.mappings[i], temp_buf);
            memcpy(temp_buf + a - i * BLOCKSIZE, buf + a - seek, b - a);
            write_datablock(mnt, inode.mappings[i], temp_buf);
        }
    }

    // Update the inode size to the new size if necessary
    inode.size = inode.size > (seek + size) ? inode.size : (seek + size);
    write_inode(mnt, inodenum, &inode);

    // Update the file handle’s offset to reflect the new position
    files[file_handle].offset += size;

    // Return success
    return 1;
}


int emufs_seek(int file_handle, int nseek) {
    /*
     * Adjust the file offset for the specified file handle.
     * The function ensures that the new offset is within valid bounds:
     * - The offset cannot be negative.
     * - The offset must not exceed the size of the file.
     *
     * Returns:
     * - -1: If the new offset is invalid (e.g., exceeds file size or is negative).
     * -  1: If the offset update is successful.
     */

    // Retrieve the mount point, current offset, and inode number of the file
    int mount_point = files[file_handle].mount_point;
    int current_offset = files[file_handle].offset;
    int inode_num = files[file_handle].inode_number;

    // If the requested seek is positive, check if the new offset is valid
    if (nseek > 0) {
        struct inode_t inode;

        // Fetch the inode to check the file's size and other properties
        read_inode(mount_point, inode_num, &inode);

        // Validate that the new offset does not exceed the file's size
        if (inode.size < (current_offset + nseek)) {
            return -1; // Error: New offset exceeds file size
        }
    }

    // Update the file's offset by adding the seek amount
    files[file_handle].offset += nseek;

    // Return 1 to indicate the operation was successful
    return 1;
}



void flush_dir(int mount_point, int inodenum, int depth) {
    // Structure to store inode information
    struct inode_t inode;

    // Read the inode information from the specified file system and inode number
    read_inode(mount_point, inodenum, &inode);

    // Print indentation based on depth for the directory tree view
    for (int i = 0; i < depth - 1; i++) {
        printf("|  "); // Indentation level marker
    }

    // Add a branch symbol at the current depth level
    if (depth) {
        printf("|--");
    }

    // Print the name of the file or directory
    for (int i = 0; i < MAX_ENTITY_NAME && inode.name[i] > 0; i++) {
        printf("%c", inode.name[i]);
    }

    // If the inode represents a file, display its size
    if (inode.type == 0) {
        printf(" (%d bytes)\n", inode.size);
    } else {
        // If it's a directory, add a newline and recursively process its contents
        printf("\n");
        for (int i = 0; i < inode.size; i++) {
            flush_dir(mount_point, inode.mappings[i], depth + 1);
        }
    }
}

void fsdump(int mount_point)
{
    /*
        * Outputs the metadata of the file system linked to the provided mount point.
        * This includes details about inodes and blocks currently in use.
    */
    
    // Define a superblock structure to hold the metadata
    struct superblock_t superblock;

    // Fetch the superblock data for the given mount point
    read_superblock(mount_point, &superblock);

    // Display the name of the storage device
    printf("\n[%s] File System Dump (fsdump)\n", superblock.device_name);

    // Force a flush of the directory data, starting from the root
    flush_dir(mount_point, 0, 0);

    // Print the count of in-use inodes and blocks
    printf("Inodes in use: %d, Blocks in use: %d\n", superblock.used_inodes, superblock.used_blocks);
}
