#include <iostream>
#include <cstdint>
#include <vector>

#include "disk.h"

#ifndef __FS_H__
#define __FS_H__

#define ERROR_CODE -1

#define FAT_SIZE BLOCK_SIZE / 2
#define DIR_BLOCK_SIZE BLOCK_SIZE / (uint32_t)sizeof(dir_entry)

#define ROOT_BLOCK 0
#define FAT_BLOCK 1

#define FAT_FREE 0
#define FAT_EOF -1

#define TYPE_FILE 0
#define TYPE_DIR 1
#define READ 0x04
#define WRITE 0x02
#define EXECUTE 0x01
#define FILE_NAME_SIZE 56


struct dir_entry {
    char file_name[FILE_NAME_SIZE]; // name of the file / sub-directory
    uint32_t size; // size of the file in bytes
    uint16_t first_blk; // index in the FAT for the first block of the file
    uint8_t type; // directory (1) or file (0)
    uint8_t access_rights; // read (0x04), write (0x02), execute (0x01)
};

class FS {

private:
    /*
        PATH_TYPE definitions where xN is any string:

            ROOT: "/"

            RELATIVE: "./x/x2/.../xN" OR "../x/x2/.../xN" OR "x"

            ABSOLUTE: "/x/x2/.../xN"

            INVALID: Not pertaining to any of the above.

    */
    enum class PATH_TYPE { ROOT = 0, RELATIVE, ABSOLUTE, INVALID };

    typedef std::vector<std::string> StringVector;

private:
    Disk m_disk;
    // size of a FAT entry is 2 bytes.
    int16_t m_fat[FAT_SIZE];
    // Permissions: rw-
    const uint8_t m_defaultPermissions = READ | WRITE;

    // Holds the block of CWD.
    uint16_t m_cwdBlock = ROOT_BLOCK;

private:
    // Correctly inserts a FAT entry given its index and the value for that block.
    int MakeFATEntry(const uint32_t index, const int16_t blockValue);
    
    // Writes FAT array to designated block on disk.
    int UpdateFAT();

    // Calculates a free space to write a dir entry and writes it to disk.
    int AddNewDirEntry(const int parentDirectoryBlock, const dir_entry& newDirEntry);

    // Allocates a file of certain block count. Optional input to save where first block was allocated.  
    int AllocateNewFileOnFAT(const int nBlocksToAllocate, int* const allocatedFirstBlock);

    // Extends a file by n blocks given any block beloning to the file.
    int ExtendFileOnFAT(const int nBlocksToAllocate, const int startBlock);

    // Returns the child of a certain block.
    int GetChildBlock(const int block);

    // Calculates how many blocks should minimum be allocated given a certain size in bytes.
    int CalculateMinBlockCount(const int size);

    // Gets a copy of a dir entry given its parent directory block and its name.
    // Outputs an empty dir entry if none were found.
    int GetDirEntry(const int parentDirBlock, const std::string& filename, dir_entry& dirEntryOut);

    // Gets a copy of a dir entry given a path to the file/directory.
    // Outputs an empty dir entry if none were found.
    int GetDirEntry(std::vector<std::string> dirPaths, dir_entry& dirEntryOut);

    // Updates an existing dir entry with a new dir entry given in a certain parent directory.
    int UpdateDirEntry(const int parentDirBlock, const dir_entry& oldDirEntry, const dir_entry& newDirEntry);

    // Goes through all linked blocks untill EOF is reached and returns that block.
    int GetEOFBlockFromStartBlock(const int startBlock);

    // Returns if a block is free or not.
    bool BlockIsFree(const int block);

    // Returns if all elements of a given dirpath are valid or not.
    bool FilenamesAreValid(std::string& dirpath);

    // Returns true if the access rights of a given dir entry matches the bitmask given.
    bool HasValidAccess(const dir_entry& dirEntry, const int accessBitMask);

    // Returns true if the filename contains any special characters.
    bool HasSpecialCharacters(const std::string& fileName);

    // Checks if a certain directory contains any dir entries (except "..") by checking if it has a name or not.
    // Assumes that input is of type DIR.
    bool DirectoryIsEmpty(const dir_entry& dirEntry);

    // Checks if a given filepath ends in an existing dir entry.
    bool FilepathExists(const std::string& filePath);

    // Checks if given dir entry exists by checking if the name is null or not.
    bool DirEntryExists(const dir_entry& dirEntry);

    // Clears input vector and loads all blocks from FAT that are free to use.
    int GetFreeBlocks(int nBlocksToAdd, std::vector<int>& freeBlocksVector);

    // Writes data from string into file starting from its first block.
    int WriteDataStringToFile(std::string stringData, const dir_entry& fileDirEntry);

    // Reads data from a file and appends it to the given string.
    int ReadFileToDataString(std::string& stringData, const dir_entry& fileDirEntry);

    // Gets the block that representes the directory (not file) at the end of a given dir path.
    // Returns error code if no block was found or if a file was hit in the middle of the path.
    int GetDirectoryBlock(const std::vector<std::string>& dirPaths);

    // Returns a vector of strings containing each filename that was separated by '/' from input string.
    // If the given path only consists "/" the vector will contain one empty string.
    std::vector<std::string> ParseDirPath(const std::string& dirPath);

    // Returns an evaluated path type given the structure of a certain filepath.
    PATH_TYPE EvaluatePathType(const std::vector<std::string>& paths);

public:
    FS();
    ~FS();
    // formats the disk, i.e., creates an empty file system
    int format();
    // create <filepath> creates a new file on the disk, the data content is
    // written on the following rows (ended with an empty row)
    int create(std::string filepath);
    // cat <filepath> reads the content of a file and prints it on the screen
    int cat(std::string filepath);
    // ls lists the content in the currect directory (files and sub-directories)
    int ls();

    // cp <sourcepath> <destpath> makes an exact copy of the file
    // <sourcepath> to a new file <destpath>
    int cp(std::string sourcepath, std::string destpath);
    // mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
    // or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
    int mv(std::string sourcepath, std::string destpath);
    // rm <filepath> removes / deletes the file <filepath>
    int rm(std::string filepath);
    // append <filepath1> <filepath2> appends the contents of file <filepath1> to
    // the end of file <filepath2>. The file <filepath1> is unchanged.
    int append(std::string filepath1, std::string filepath2);

    // mkdir <dirpath> creates a new sub-directory with the name <dirpath>
    // in the current directory
    int mkdir(std::string dirpath);
    // cd <dirpath> changes the current (working) directory to the directory named <dirpath>
    int cd(std::string dirpath);
    // pwd prints the full path, i.e., from the root directory, to the current
    // directory, including the currect directory name
    int pwd();

    // chmod <accessrights> <filepath> changes the access rights for the
    // file <filepath> to <accessrights>.
    int chmod(std::string accessrights, std::string filepath);
};

#endif // __FS_H__
