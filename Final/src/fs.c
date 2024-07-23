#include "disk.h"
#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#define min(a,b) (((a) < (b)) ? (a) : (b))

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release builds */
#endif


// Global Variables  -----------------------------------------------------------

int *free_block_bitmap;


// Debug file system -----------------------------------------------------------

void fs_debug(Disk *disk)
{
    if (disk == 0)
        return;

    Block block;

    // Read Superblock
    disk_read(disk, 0, block.Data);

    uint32_t magic_num = block.Super.MagicNumber;
    uint32_t num_blocks = block.Super.Blocks;
    uint32_t num_inodeBlocks = block.Super.InodeBlocks;
    uint32_t num_inodes = block.Super.Inodes;

    if (magic_num != MAGIC_NUMBER)
    {
        printf("Magic number is valid: %c\n", magic_num);
        return;
    }

    printf("SuperBlock:\n");
    printf("    magic number is valid\n");
    printf("    %u blocks\n", num_blocks);
    printf("    %u inode blocks\n", num_inodeBlocks);
    printf("    %u inodes\n", num_inodes);

    uint32_t expected_num_inodeBlocks = round((float)num_blocks / 10);

    if (expected_num_inodeBlocks != num_inodeBlocks)
    {
        printf("SuperBlock declairs %u InodeBlocks but expect %u InodeBlocks!\n", num_inodeBlocks, expected_num_inodeBlocks);
    }

    uint32_t expect_num_inodes = num_inodeBlocks * INODES_PER_BLOCK;
    if (expect_num_inodes != num_inodes)
    {
        printf("SuperBlock declairs %u Inodes but expect %u Inodes!\n", num_inodes, expect_num_inodes);
    }

    // FIXME: Read Inode blocks
    for (uint32_t k = 0; k < num_inodeBlocks; k++) {
        disk_read(disk, k+1, block.Data);
        for (uint32_t i = 0; i < INODES_PER_BLOCK; i++) {
            if (block.Inodes[i].Valid == 1) {
                printf("Inode %u:\n", k * INODES_PER_BLOCK + i);
                printf("    size: %u bytes\n", block.Inodes[i].Size);
                printf("    direct blocks:");
                for (uint32_t j = 0; j < POINTERS_PER_INODE; j++) {
                    if (block.Inodes[i].Direct[j] != 0) {
                        printf(" %u", block.Inodes[i].Direct[j]);
                    }
                }
                printf("\n");
                // If no indirect blocks, don't print anything
                // Also there is an issue for image_200, incorrect indirect data blocks
                if (block.Inodes[i].Indirect != 0) {
                    printf("    indirect block: %u\n", block.Inodes[i].Indirect);
                    printf("    indirect data blocks:");
                    Block indirectBlock;
                    disk_read(disk, block.Inodes[i].Indirect, indirectBlock.Data);
                    for (uint32_t j = 0; j < POINTERS_PER_BLOCK; j++) {
                        if (indirectBlock.Pointers[j] != 0) {
                            printf(" %u", indirectBlock.Pointers[j]);
                        }
                    }
                    printf("\n");
                }
            }
        }
    }
    

}

// Format file system ----------------------------------------------------------

bool fs_format(Disk *disk)
{
    if (disk == 0) {
      return false;  
    }
    
    if (disk_mounted(disk)) {
        return false;
    }
    
    uint32_t numBlocks = disk->Blocks;
    
    Block block;
    block.Super.MagicNumber = MAGIC_NUMBER;
    block.Super.Blocks = numBlocks;
    const float relativeInodeSize = 0.1;
    block.Super.InodeBlocks = max(1, ceil((float)disk->Blocks * relativeInodeSize));
    block.Super.Inodes = block.Super.InodeBlocks * INODES_PER_BLOCK;
    disk_write(disk, 0, block.Data);

    memset(block.Data, 0, BLOCK_SIZE);

    for (uint32_t i = 1; i < numBlocks; i++) {
        disk_write(disk, i, block.Data);
    }

    return true;
}

// FileSystem constructor 
FileSystem *new_fs()
{
    FileSystem *fs = malloc(sizeof(FileSystem));
    return fs;
}

// FileSystem destructor 
void free_fs(FileSystem *fs)
{
    // FIXME: free resources and allocated memory in FileSystem

    free(fs);
}

// Mount file system -----------------------------------------------------------

bool fs_mount(FileSystem *fs, Disk *disk)
{
    if (fs == 0) return false;
    if (disk == 0) return false;
    if (disk_mounted(disk)) return false;

    // Read superblock
    Block block;
    disk_read(disk, 0, block.Data);

    if (block.Super.MagicNumber != MAGIC_NUMBER) return false;
    const float relativeInodeSize = 0.1;
    if (max(1, ceil((float)block.Super.Blocks * relativeInodeSize)) != (float)block.Super.InodeBlocks) return false;
    if (block.Super.Inodes != block.Super.InodeBlocks * INODES_PER_BLOCK) return false;
    if (block.Super.Blocks != disk_size(disk)) return false;
    // Set device and mount
    fs->disk = disk;
    disk_mount(fs->disk);

    // Copy metadata
    fs->superBlock = block.Super;

    // Allocate free block bitmap
    
    free_block_bitmap = malloc(block.Super.Blocks * sizeof(int));
    memset(free_block_bitmap, 0, block.Super.Blocks * sizeof(int));
    free_block_bitmap[0] = 1;

    for (ssize_t i = 0; i < fs->superBlock.InodeBlocks; i++) {
        free_block_bitmap[i + 1] = 1;
    }

    fs->free_block_bitmap = free_block_bitmap;
    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t fs_create(FileSystem *fs)
{
    for (uint32_t k = 0; k < fs->superBlock.InodeBlocks; k++) {
        Block block;
        disk_read(fs->disk, k + 1, block.Data);
        for (uint32_t i = 0; i < INODES_PER_BLOCK; i++) {
            if (!block.Inodes[i].Valid) {
                block.Inodes[i].Valid = true;
                block.Inodes[i].Size = 0;
                disk_write(fs->disk, k + 1, block.Data);
                return i;
            }
        }
    }
    

    return -1;
}

// Optional: the following two helper functions may be useful. 

uint32_t *calculate_inode_loc(size_t inumber) {
    size_t quotient = (inumber / INODES_PER_BLOCK) + 1;
    size_t remainder = inumber % INODES_PER_BLOCK;
    uint32_t *quot_remain = malloc(2 * sizeof(uint32_t));
    quot_remain[0] = quotient;
    quot_remain[1] = remainder;
    return quot_remain;
}

bool find_inode(FileSystem *fs, size_t inumber, Inode *inode)
{
    uint32_t *quot_remain = calculate_inode_loc(inumber);
    uint32_t quotient = quot_remain[0];
    uint32_t remainder = quot_remain[1];
    Block block;
    disk_read(fs->disk, quotient, block.Data);
    if (block.Inodes[remainder].Valid) {
        *inode = block.Inodes[remainder];
        return true;
    }
    return false;
}

bool store_inode(FileSystem *fs, size_t inumber, Inode *inode)
{
    uint32_t *quot_remain = calculate_inode_loc(inumber);
    uint32_t quotient = quot_remain[0];
    uint32_t remainder = quot_remain[1];
    Block block;
    disk_read(fs->disk, quotient, block.Data);
    if (block.Inodes[remainder].Valid) {
        block.Inodes[remainder] = *inode;
        disk_write(fs->disk, quotient, block.Data);
        return true;
    }
    return false;
}

// Remove inode ----------------------------------------------------------------

bool fs_remove(FileSystem *fs, size_t inumber)
{
    // Load inode information
    Inode inode;

    bool found = find_inode(fs, inumber, &inode);

    if (!found) {
        return false;
    }

    
    // Free direct blocks
    for (uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
        uint32_t dataBlock = inode.Direct[i];
        free_block_bitmap[dataBlock] = 0;
        inode.Direct[i] = 0;
    }

    // Free indirect blocks
    if (inode.Indirect != 0) {
        Block block;
        disk_read(fs->disk, inode.Indirect, block.Data);
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
            uint32_t datablock = block.Pointers[i];
            free_block_bitmap[datablock] = 0;
            block.Pointers[i] = 0;
        }
        free_block_bitmap[inode.Indirect] = 0;
        disk_write(fs->disk, inode.Indirect, block.Data);
        inode.Indirect = 0;
    }
    
    // Clear inode in inode table
    inode.Valid = false;
    Block block;
    uint32_t quotient = calculate_inode_loc(inumber)[0];
    uint32_t remainder = (calculate_inode_loc(inumber))[1];
    disk_read(fs->disk, quotient, block.Data);
    block.Inodes[remainder] = inode;
    disk_write(fs->disk, quotient, block.Data);
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t fs_stat(FileSystem *fs, size_t inumber)
{
    // Load inode information
    Inode inode;
    bool found = find_inode(fs, inumber, &inode);
    if (!found) {
        return -1;
    }

    return inode.Size;
}

// Read from inode -------------------------------------------------------------

long calculate_offset(size_t offset) {
    long index = -1;
    long offset_tmp = offset;
    while (offset_tmp >= 0) {
        offset_tmp -= BLOCK_SIZE;
        index++;
    }
    return index;
}

ssize_t fs_read(FileSystem *fs, size_t inumber, char *data, size_t length, size_t offset)
{

    // Load inode information
    Inode inode;
    bool found = find_inode(fs, inumber, &inode);
    if (!found) {
        return -1;
    }
    size_t size = inode.Size;

    // add error cases (when we can't read anymore)
    /*
    if offset >= size we return false since there is nothing to read
    */
    if (offset >= size) {
        return -1;
    }

    // Adjust length
    // |------------------------------|
    // |-------------------------------------------------------------------------|
    //              |----------------------------------
    // length =     |-----------------|


    if (length > size) {
        length = size;
    }

    if (length + offset > size) {
        length = size - offset;
    }

    // Read block and copy to data
    // read all direct blocks (one by one until length), and then go into indirect blocks and read them until done
    long read = 0;
    long loc = -1;
    long indirect_loc = 0;
    long remaining_size = size - offset;
    long increment = 0;

    loc = calculate_offset(offset);
    if (loc >= POINTERS_PER_INODE) {
        indirect_loc = loc - POINTERS_PER_INODE;
    }

    Block indirect_block;
    if (inode.Indirect != 0) {
        disk_read(fs->disk, inode.Indirect, indirect_block.Data);
    }

    while (read < length) {
        Block block;
        if (inode.Direct[loc] == 0) {
            loc++;
        }
        else if (loc < POINTERS_PER_INODE) {
            disk_read(fs->disk, inode.Direct[loc], block.Data);
        }
        else if (inode.Indirect == 0 || indirect_loc > POINTERS_PER_BLOCK) {
            return read;
        }
        else if (indirect_block.Pointers[indirect_loc] == 0) {
            indirect_loc++;
        }
        else {
            // Block tempBlock;
            disk_read(fs->disk, indirect_block.Pointers[indirect_loc], block.Data);
            indirect_loc++;
        }
        
        memcpy(data + read, &block.Data, BLOCK_SIZE);
        increment = min(remaining_size, BLOCK_SIZE);
        if (increment == BLOCK_SIZE) {
            remaining_size -= BLOCK_SIZE;
        }
        else {
            remaining_size = 0;
        }
        read += increment;
        loc++;
    }
    
    return read;
}

// Optional: the following helper function may be useful. 

// return next free block in bitmap and mark it as used
ssize_t fs_allocate_block(FileSystem *fs)
{
    for (int i = 0; i < fs->superBlock.Blocks; i++) {
        if (fs->free_block_bitmap[i] == 0) {
            fs->free_block_bitmap[i] = 1;
            return i;
        }
    }
    return -1;
}

// Write to inode --------------------------------------------------------------

ssize_t fs_write(FileSystem *fs, size_t inumber, char *data, size_t length, size_t offset)
{
    // Load inode
    Inode inode;
    bool found = find_inode(fs, inumber, &inode);
    if (!found) {
        return -1;
    }
    
    // Write block and copy to data
    uint32_t write = 0;
    ssize_t direct_loc = 0;
    ssize_t indirect_loc = 0;

    Block indirect_block;

    if (inode.Indirect != 0) {
        disk_read(fs->disk, inode.Indirect, indirect_block.Data);
    }

    direct_loc = calculate_offset(offset);
    if (direct_loc >= POINTERS_PER_INODE) {
        indirect_loc = direct_loc - POINTERS_PER_INODE;
    }

    while(write != length) {

        Block write_block;
        ssize_t remaining_write = length - write;
        ssize_t intermediate_write = 0;

        if (direct_loc < POINTERS_PER_INODE) {
            intermediate_write = min(BLOCK_SIZE, remaining_write);
            memcpy(&write_block.Data, data + offset, intermediate_write);
            ssize_t loc = fs_allocate_block(fs);
            if (loc == -1) {
                if (write == 0) {
                    return -1;
                }
                inode.Size += write;
                store_inode(fs, inumber, &inode);
                return write;
            }
            inode.Direct[direct_loc] = loc;
            disk_write(fs->disk, loc, write_block.Data);
        }
        else if (inode.Indirect == 0) {
            ssize_t loc = fs_allocate_block(fs);
            if (loc == -1) {
                if (write == 0) {
                    return -1;
                }
                inode.Size += write;
                store_inode(fs, inumber, &inode);
                return write;
            }
            inode.Indirect = loc;
            indirect_loc = 0;
            Block indirect_block;
            disk_read(fs->disk, inode.Indirect, indirect_block.Data);
            memset(indirect_block.Data, 0, BLOCK_SIZE);
            indirect_loc = fs_allocate_block(fs);
            if (indirect_loc == -1) {
                if (write == 0) {
                    return -1;
                }
                inode.Size += write;
                store_inode(fs, inumber, &inode);
                return write;
            }
            indirect_block.Pointers[0] = loc;
            intermediate_write = min(remaining_write, BLOCK_SIZE);
            memcpy(&write_block.Data, data + offset, intermediate_write);
            disk_write(fs->disk, loc, write_block.Data);
            indirect_loc++;
        }
        else if (indirect_loc < POINTERS_PER_BLOCK) {
            Block indirect_block;
            ssize_t indirect_block_loc = inode.Indirect;
            disk_read(fs->disk, indirect_block_loc, indirect_block.Data);
            intermediate_write = min(remaining_write, BLOCK_SIZE);
            memcpy(&write_block.Data, data + offset, intermediate_write);
            ssize_t loc = fs_allocate_block(fs);
            if (loc == -1) {
                if (write == 0) {
                    return -1;
                }
                inode.Size += write;
                store_inode(fs, inumber, &inode);
                return write;
            }
            indirect_block.Pointers[indirect_loc] = loc;
            disk_write(fs->disk, loc, write_block.Data);
            indirect_loc++;
        }
        else {
            if (write == 0) {
                return -1;
            }
            inode.Size += write;
            store_inode(fs, inumber, &inode);
            return write;
        }
        write += intermediate_write;
        direct_loc++;

    }
    inode.Size += write;
    store_inode(fs, inumber, &inode);
    return write;
}
