# ECS150 Project 4 Report

## Introduction
  + In this project, we implement the FAT-based file system on a virtual disk. We realize the functions to mount/unmount the file system, create/delete files, open/close a file, read from/write to a file, etc. 

## Phase 1
  + In this phase, the main job is to initialize our file system. First of our, we create our own data structures for varient blocks including super blocks, FAT, and root directory. Then we create all the global static variables and initialize them.
  + `int fs_mount(const char *diskname)`: here we start our file system by open it on `diskname`. We open the disk. Then read the super block **g_superBlockInfo** we initialized via `block_read()` . We allocate memory to FAT with the size of the number of blocks **fat_block_num** and use the number in a for loop to read each block with `block_read()`. Next, we read the root directory **g_rootDirInfo**. Finally, we initialize the data blocks with `BLOCK_SIZE` for each block -- create a temp data block, allocate memory to it, and assign it to every data block in our structure **g_all_data**. In every step, we use the functions in a if-codition to check the validity of the step and return -1 for any failure.
  + `int fs_umount(void)`: to unmount the currently mounted file system. We use `block_write()` on each data structure we read in `fs_mount()`, i.e., write super block, FAT, root directory, and all data. Again, we use if-condition for invalidity check.
  + `int fs_info(void)`: where we print the file system status. In our structure, we create a counter or index to store the number of blocks. We using these numbers for printing here.

## Phase 2
 + `int fs_create(const char *filename)`: check the current space status and filename validity. If no error, `strncpy` the `filename` to root directory; otherwise, return -1.
 + `int fs_delete(const char *filename)`: check filename validity. If correct, clear data content in FAT by set data to `FAT_EOC` in a while loop and then delete `filename` using `memset()` in root directory.
 + `int fs_ls(void)`: in a for loop, with `FS_FILE_MAX_COUNT`, print file information of each file in root directory.

## Phase 3
 + `int fs_open(const char *filename)`: check filename validity and availability. Then allocate memory for the file and put it on the opened file list.
 + `int fs_close(int fd)`: check the validity of `fd`. Then find this file on the opened file list. Do `free()` to clear it.
 + `int fs_stat(int fd)`: check the validity. Then find it on the opened file list and use its file index to return the file size information stored in root directory.
 + `int fs_lseek(int fd, size_t offset)`: check the validity of `fd`. Find it on the list and compare offset to file size to check if offset can be set. Then set.

## Phase 4
 + `int fs_write(int fd, void *buf, size_t count)`: check the validity of `fd`, `buf`, and `count`. Find the file to write on the opened file list with `fd` and set the file entry to its content in the root directory via file index. We find empty FAT for file entry by checking `FAT_EOC` with block index. If `BLOCK_SIZE` is smaller than **offset + write_len**, `memcpy()` the buffer into the first block, middle blocks, and the last blocks separately; otherwise, simply write it to one block.
 + `int fs_read(int fd, void *buf, size_t count)`: similar to `fs_write()`, use the `memcpy` on block(s) and buffer reversely.

## Test Case
 + We check four cases in our **my_test.c**.
 + 1. The readability/writability of large file: check our implementation of multiple block usage in `fs_write()` and `fs_read()`.
 + 2. Offset larger than `BLOCK_SIZE`: check under this situation, if `fs_write()` and `fs_read()` work appropriately (another test for multiple block use).
 + 3. Create full number of files: with `FS_FILE_MAX_COUNT`, check if `fs_create()` can create maximum number of files.
 + 4. Open full number of files: check if `fs_open()` can open the maximum number of files. Also, check the situation when opening same file, the file discriptors `fd` differ while the file sizes are the same.