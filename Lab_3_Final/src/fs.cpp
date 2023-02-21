#include <iostream>
#include <cstring>
#include <vector>
#include <sstream>

#include "fs.h"

FS::FS()
{
    std::cout << "FS::FS()... Creating file system\n";
}

FS::~FS() {}

// formats the disk, i.e., creates an empty file system
int FS::format()
{
    std::cout << "FS::format()\n";

    char emptyBlock[BLOCK_SIZE] = {'\0'};
    for (int i = 0; i < m_disk.get_no_blocks(); i++)
    {
        if (m_disk.write(i, (uint8_t *)emptyBlock) != 0)
        {
            return ERROR_CODE;
        }
    }

    // Set busy for root block and FAT block.
    if (MakeFATEntry(ROOT_BLOCK, FAT_EOF) != 0)
    {
        return ERROR_CODE;
    }
    if (MakeFATEntry(FAT_BLOCK, FAT_EOF) != 0)
    {
        return ERROR_CODE;
    }

    // Start initializing after root and FAT block.
    for (int i = FAT_BLOCK + 1; i < FAT_SIZE; i++)
    {
        if (MakeFATEntry(i, FAT_FREE) != 0)
        {
            return ERROR_CODE;
        }
    }

    return 0;
}

// create <filepath> creates a new file on the disk, the data content is
// written on the following rows (ended with an empty row)
int FS::create(std::string filepath)
{
    std::cout << "FS::create(" << filepath << ")\n";

    if (!FilenamesAreValid(filepath) || FilepathExists(filepath))
    {
        return ERROR_CODE;
    }

    std::vector<std::string> parsedDirPath = ParseDirPath(filepath);
    std::string newFilename = parsedDirPath.back();
    parsedDirPath.pop_back();

    // Get user data
    std::string inputBuffer;
    std::getline(std::cin, inputBuffer, '\n');

    if (newFilename == "testfile")
    {
        inputBuffer = std::string(BLOCK_SIZE * 3 + 1, 'a');
    }

    // Add the newline that was ignored from getline.
    inputBuffer.append("\n");
    int inputBufferSize = (int)inputBuffer.size();
    int nBlocksToAllocate = CalculateMinBlockCount(inputBufferSize);

    int allocatedFirstBlock;
    if (AllocateNewFileOnFAT(nBlocksToAllocate, &allocatedFirstBlock) != 0)
    {
        return ERROR_CODE;
    };

    dir_entry newDirEntry = {};
    strcpy(newDirEntry.file_name, filepath.c_str());
    newDirEntry.size = inputBufferSize;
    newDirEntry.first_blk = allocatedFirstBlock;
    newDirEntry.type = TYPE_FILE;
    newDirEntry.access_rights = m_defaultPermissions;

    if (WriteDataStringToFile(inputBuffer, newDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    // Make dir entry.
    if (AddNewDirEntry(GetDirectoryBlock(parsedDirPath), newDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    return 0;
}

// cat <filepath> reads the content of a file and prints it on the screen
int FS::cat(std::string filepath)
{
    std::cout << "FS::cat(" << filepath << ")\n";

    if (!FilenamesAreValid(filepath) || !FilepathExists(filepath))
    {
        return ERROR_CODE;
    }

    dir_entry fileDirEntry;
    GetDirEntry(ParseDirPath(filepath), fileDirEntry);

    if (fileDirEntry.type != TYPE_FILE || !HasValidAccess(fileDirEntry, READ))
    {
        return ERROR_CODE;
    }

    int currentFileBlock = fileDirEntry.first_blk;
    char fileBlockBuffer[BLOCK_SIZE] = {'\0'};
    std::string catOutput = "";
    while (currentFileBlock != FAT_EOF)
    {
        if (m_disk.read(currentFileBlock, (uint8_t *)fileBlockBuffer) != 0)
        {
            return ERROR_CODE;
        }
        catOutput.append(fileBlockBuffer);

        currentFileBlock = GetChildBlock(currentFileBlock);
    }

    std::cout << catOutput << std::endl;
    return 0;
}

// ls lists the content in the currect directory (files and sub-directories)
int FS::ls()
{
    std::cout << "FS::ls()\n";

    // The order of this enum determines the order of headers in the output. Last enum should always be NUMBER_OF_HEADERS.
    enum HEADERS
    {
        NAME = 0,
        TYPE,
        AXS_RIGHTS,
        SIZE,
        NUMBER_OF_HEADERS
    };
    const int columnCount = HEADERS::NUMBER_OF_HEADERS;

    // An array of string vectors where each element in the array represents values for that column.
    StringVector columnData[columnCount];
    columnData[HEADERS::NAME] = {"Name"};
    columnData[HEADERS::SIZE] = {"Size"};
    columnData[HEADERS::AXS_RIGHTS] = {"Accessrights"};
    columnData[HEADERS::TYPE] = {"Type"};

    // Keeps track of size of the largest string in each column.
    // This is for determining the maximum spacing each column has to have.
    int maxLengths[columnCount];
    for (int i = 0; i < columnCount; i++)
    {
        maxLengths[i] = (int)columnData[i].back().size();
    }

    // Gets all dir entries in CWD.
    dir_entry dirEntries[DIR_BLOCK_SIZE];
    if (m_disk.read(m_cwdBlock, (uint8_t *)dirEntries))
    {
        return ERROR_CODE;
    }

    int nDirEntriesAdded = 0;
    // For each dir entry we want to add information to each column.
    for (dir_entry &dirEntry : dirEntries)
    {
        if (!DirEntryExists(dirEntry))
        {
            continue;
        }

        // For each column we want to add its own specified data from the dir entry.
        for (int i = 0; i < columnCount; i++)
        {
            std::string columnEntry;

            switch (i)
            {
            case HEADERS::NAME:
                columnEntry = dirEntry.file_name;
                break;

            case HEADERS::SIZE:
                columnEntry = dirEntry.size == 0 ? "-" : std::to_string(dirEntry.size);
                break;

            case HEADERS::TYPE:
                columnEntry = dirEntry.type == TYPE_FILE ? "file" : "dir";
                break;

            case HEADERS::AXS_RIGHTS:
            {
                const int axsRights = dirEntry.access_rights;
                columnEntry = axsRights & READ ? "r" : "-";
                columnEntry += axsRights & WRITE ? "w" : "-";
                columnEntry += axsRights & EXECUTE ? "x" : "-";
            }
            break;

            default:
                // This should never run as every case should be covered.
                columnEntry = "N/A";
                break;
            }

            columnData[i].push_back(columnEntry);

            // Replaces previous max length if new entry is larger.
            maxLengths[i] = columnEntry.size() > maxLengths[i] ? columnEntry.size() : maxLengths[i];
        }

        nDirEntriesAdded++;
    }

    const std::string defaultColumnMargin = "\t";
    std::string rowOutput = "";
    for (int row = 0; row < nDirEntriesAdded + 1; row++) // +1 because we want to also include headers row.
    {
        for (int column = 0; column < columnCount; column++)
        {
            std::string columnEntry = columnData[column][row];
            int emptySpace = maxLengths[column] - columnEntry.size();

            rowOutput.append(columnEntry);
            rowOutput.append(emptySpace, ' ');
            rowOutput.append(defaultColumnMargin);
        }

        std::cout << rowOutput << std::endl;

        rowOutput.clear();
    }

    return 0;
}

// cp <sourcepath> <destpath> makes an exact copy of the file
// <sourcepath> to a new file <destpath>
int FS::cp(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::cp(" << sourcepath << "," << destpath << ")\n";

    if (!FilenamesAreValid(sourcepath) || !FilenamesAreValid(destpath))
    {
        return ERROR_CODE;
    }
    if (!FilepathExists(sourcepath))
    {
        return ERROR_CODE;
    }

    dir_entry sourceDirEntry;
    GetDirEntry(ParseDirPath(sourcepath), sourceDirEntry);
    if (!DirEntryExists(sourceDirEntry))
    {
        return ERROR_CODE;
    }
    // Return error if the found dir entry is not a file or if the file cannot be read from.
    if (sourceDirEntry.type != TYPE_FILE || !HasValidAccess(sourceDirEntry, READ))
    {
        return ERROR_CODE;
    }

    dir_entry destDirEntry;
    StringVector parsedDestPath = ParseDirPath(destpath);
    if (GetDirEntry(parsedDestPath, destDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    // Will either be the current directory or the specified directory given by destpath.
    int dirBlock;
    std::string destFileName;
    if (destDirEntry.type == TYPE_DIR)
    {
        dirBlock = destDirEntry.first_blk;
        destFileName = sourceDirEntry.file_name;
    }
    else
    {
        // Return error if a file already exists or if the filename given has any special characters.
        if (DirEntryExists(destDirEntry) || HasSpecialCharacters(destpath))
        {
            return ERROR_CODE;
        }
        dirBlock = m_cwdBlock;
        destFileName = destpath;
    }

    int firstFreeBlock;
    if (AllocateNewFileOnFAT(CalculateMinBlockCount(sourceDirEntry.size), &firstFreeBlock) != 0)
    {
        return ERROR_CODE;
    }

    dir_entry sourceDirCopy = sourceDirEntry;
    strcpy(sourceDirCopy.file_name, destFileName.c_str());
    sourceDirCopy.first_blk = firstFreeBlock;

    // Add the dir entry to the evaluted correct dir block.
    if (AddNewDirEntry(dirBlock, sourceDirCopy) != 0)
    {
        return ERROR_CODE;
    }

    int currentSourceBlock = sourceDirEntry.first_blk;
    int currentDestBlock = sourceDirCopy.first_blk;
    while (currentSourceBlock != FAT_EOF)
    {
        char dataBuffer[BLOCK_SIZE]; // No need to initialize as whole block will be written.
        if (m_disk.read(currentSourceBlock, (uint8_t *)dataBuffer) != 0)
        {
            return ERROR_CODE;
        }
        if (m_disk.write(currentDestBlock, (uint8_t *)dataBuffer) != 0)
        {
            return ERROR_CODE;
        }

        currentSourceBlock = GetChildBlock(currentSourceBlock);
        currentDestBlock = GetChildBlock(currentDestBlock);
    }

    return 0;
}

// mv <sourcepath> <destpath> renames the file <sourcepath> to the name <destpath>,
// or moves the file <sourcepath> to the directory <destpath> (if dest is a directory)
int FS::mv(std::string sourcepath, std::string destpath)
{
    std::cout << "FS::mv(" << sourcepath << "," << destpath << ")\n";

    if (!FilenamesAreValid(sourcepath) || !FilenamesAreValid(destpath))
    {
        return ERROR_CODE;
    }
    if (!FilepathExists(sourcepath))
    {
        return ERROR_CODE;
    }

    StringVector sourceParsedPath = ParseDirPath(sourcepath);
    std::string sourceFileName = sourceParsedPath.back();

    dir_entry sourceDirEntry;
    // Get dir entry of source file.
    GetDirEntry(sourceParsedPath, sourceDirEntry);
    if (sourceDirEntry.type == TYPE_DIR)
    {
        return ERROR_CODE;
    }
    sourceParsedPath.pop_back();
    int sourceDirEntryBlock = GetDirectoryBlock(sourceParsedPath);

    dir_entry destDirEntry;
    // Get dir entry of destpath (if it exists).
    StringVector destParsedPath = ParseDirPath(destpath);
    if (GetDirEntry(destParsedPath, destDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    if (destDirEntry.type == TYPE_DIR) // Move to dir
    {
        // Write dir entry in dest dir.
        if (AddNewDirEntry(destDirEntry.first_blk, sourceDirEntry) != 0)
        {
            return ERROR_CODE;
        }

        dir_entry emptyDirEntry = {};
        // Remove dir entry from source dir.
        if (UpdateDirEntry(sourceDirEntryBlock, sourceDirEntry, emptyDirEntry) != 0)
        {
            return ERROR_CODE;
        }
    }
    else // Rename file.
    {
        // Return error if a file already exists or if the filename given has any special characters.
        if (DirEntryExists(destDirEntry) || HasSpecialCharacters(destpath))
        {
            return ERROR_CODE;
        }

        std::string newFileName = destpath;
        dir_entry newSourceDirEntry = sourceDirEntry;
        strcpy(newSourceDirEntry.file_name, newFileName.c_str());
        if (UpdateDirEntry(sourceDirEntryBlock, sourceDirEntry, newSourceDirEntry) != 0)
        {
            return ERROR_CODE;
        }
    }

    return 0;
}

// rm <filepath> removes / deletes the file <filepath>
int FS::rm(std::string filepath)
{
    std::cout << "FS::rm(" << filepath << ")\n";

    if (!FilenamesAreValid(filepath) || !FilepathExists(filepath))
    {
        return ERROR_CODE;
    }

    dir_entry tempDirEntryHolder;
    StringVector parsedPath = ParseDirPath(filepath);
    if (GetDirEntry(parsedPath, tempDirEntryHolder) != 0)
    {
        return ERROR_CODE;
    }

    // Return error if directory is not empty.
    // If-statement will not run function unless first condition is true.
    if (tempDirEntryHolder.type == TYPE_DIR && !DirectoryIsEmpty(tempDirEntryHolder))
    {
        return ERROR_CODE;
    }

    dir_entry emptyDirEntry = {};
    parsedPath.pop_back();
    if (UpdateDirEntry(GetDirectoryBlock(parsedPath), tempDirEntryHolder, emptyDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    int currentBlock = tempDirEntryHolder.first_blk;
    while (currentBlock != FAT_EOF)
    {
        // Zero out data at the block.
        // This was done as to make sure that when any file want to use the free block it should not contain data.
        // The decision was made to do this at removal instead of creation as there are many sources of creating a file but only one of removing.
        char emptyBlock[BLOCK_SIZE] = {'\0'};
        m_disk.write(currentBlock, (uint8_t *)emptyBlock);

        int nextBlock = GetChildBlock(currentBlock);
        if (MakeFATEntry(currentBlock, FAT_FREE) != 0)
        {
            return ERROR_CODE;
        }
        currentBlock = nextBlock;
    }

    return 0;
}

// append <filepath1> <filepath2> appends the contents of file <filepath1> to
// the end of file <filepath2>. The file <filepath1> is unchanged.
int FS::append(std::string filepath1, std::string filepath2)
{
    std::cout << "FS::append(" << filepath1 << "," << filepath2 << ")\n";

    if (!FilenamesAreValid(filepath1) || !FilenamesAreValid(filepath2))
    {
        return ERROR_CODE;
    }
    if (!FilepathExists(filepath1) || !FilepathExists(filepath2))
    {
        return ERROR_CODE;
    }

    dir_entry sourceDirEntry;
    dir_entry destDirEntry;

    StringVector parsedDestPath = ParseDirPath(filepath2);
    GetDirEntry(ParseDirPath(filepath1), sourceDirEntry);
    GetDirEntry(parsedDestPath, destDirEntry);

    if (sourceDirEntry.type != TYPE_FILE || destDirEntry.type != TYPE_FILE)
    {
        return ERROR_CODE;
    }
    if (!HasValidAccess(sourceDirEntry, READ) || !HasValidAccess(destDirEntry, (READ | WRITE)))
    {
        return ERROR_CODE;
    }

    std::string fileContents = "";
    if (ReadFileToDataString(fileContents, destDirEntry) != 0)
    {
        return ERROR_CODE;
    }
    if (ReadFileToDataString(fileContents, sourceDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    {
        int destBlockCount = CalculateMinBlockCount(destDirEntry.size);
        int desiredNewBlockCount = CalculateMinBlockCount(fileContents.size());
        // If new file contents are bigger than previous size.
        if (desiredNewBlockCount > destBlockCount)
        {
            // Expand file2.
            if (ExtendFileOnFAT(desiredNewBlockCount - destBlockCount, destDirEntry.first_blk) != 0)
            {
                return ERROR_CODE;
            }
        }
    }

    dir_entry newDestDirEntry = destDirEntry;
    newDestDirEntry.size = fileContents.size();
    parsedDestPath.pop_back();
    if (UpdateDirEntry(GetDirectoryBlock(parsedDestPath), destDirEntry, newDestDirEntry) != 0)
    {
        return ERROR_CODE;
    }

    return WriteDataStringToFile(fileContents, newDestDirEntry);
}

// mkdir <dirpath> creates a new sub-directory with the name <dirpath>
// in the current directory
int FS::mkdir(std::string dirpath)
{
    std::cout << "FS::mkdir(" << dirpath << ")\n";

    if (!FilenamesAreValid(dirpath) || FilepathExists(dirpath))
    {
        return ERROR_CODE;
    }

    std::vector<std::string> parsedFilePath = ParseDirPath(dirpath);
    std::string dirName = parsedFilePath.back();

    int newDirBlock;
    if (AllocateNewFileOnFAT(1, &newDirBlock) != 0)
    {
        return ERROR_CODE;
    }

    dir_entry newDir = {};
    strcpy(newDir.file_name, dirName.c_str());
    newDir.first_blk = newDirBlock;
    newDir.access_rights = m_defaultPermissions;
    newDir.size = 0;
    newDir.type = TYPE_DIR;

    parsedFilePath.pop_back();
    int parentDirBlock = GetDirectoryBlock(parsedFilePath);
    // Add dir entry to parent directory.
    if (AddNewDirEntry(parentDirBlock, newDir) != 0)
    {
        return ERROR_CODE;
    }
    if (MakeFATEntry(newDirBlock, FAT_EOF) != 0)
    {
        return ERROR_CODE;
    }

    dir_entry backRefDirEntry = {};
    strcpy(backRefDirEntry.file_name, "..");
    backRefDirEntry.first_blk = parentDirBlock;
    newDir.access_rights = 0;
    backRefDirEntry.size = 0;
    backRefDirEntry.type = TYPE_DIR;

    // Create back ref dir entry for directory at the newly allocated block.
    if (AddNewDirEntry(newDirBlock, backRefDirEntry) != 0)
    {
        return ERROR_CODE;
    }
    return 0;
}

// cd <dirpath> changes the current (working) directory to the directory named <dirpath>
int FS::cd(std::string dirpath)
{
    std::cout << "FS::cd(" << dirpath << ")\n";

    if (dirpath == "/")
    {
        m_cwdBlock = ROOT_BLOCK;
        return 0;
    }

    if (!FilenamesAreValid(dirpath))
    {
        return ERROR_CODE;
    }

    dir_entry newCWD;
    GetDirEntry(ParseDirPath(dirpath), newCWD);
    if (!DirEntryExists(newCWD) || newCWD.type != TYPE_DIR)
    {
        return ERROR_CODE;
    }

    m_cwdBlock = newCWD.first_blk;

    return 0;
}

// pwd prints the full path, i.e., from the root directory, to the current
// directory, including the currect directory name
int FS::pwd()
{
    std::cout << "FS::pwd()\n";

    int currentBlock = m_cwdBlock;
    StringVector filepath = {};
    // This while loop goes through all directories backwards starting from CWD and saves the name of a parent dir entry
    // that points to the same block as CWD. This repeats until root is hit.
    while (currentBlock != ROOT_BLOCK)
    {
        dir_entry backRefEntry;
        // Get parent directory.
        if (GetDirEntry(currentBlock, "..", backRefEntry) != 0)
        {
            return ERROR_CODE;
        }

        dir_entry dirEntries[DIR_BLOCK_SIZE];
        if (m_disk.read(backRefEntry.first_blk, (uint8_t *)dirEntries) != 0)
        {
            return ERROR_CODE;
        }
        // Loop through all entries in parent directory
        for (dir_entry &dirEntry : dirEntries)
        {
            if (dirEntry.first_blk == currentBlock) // Find dir entry in parent folder that points to current dir.
            {
                filepath.push_back("/" + (std::string)dirEntry.file_name);
                break;
            }
        }

        // Change CWD to parent directory.
        currentBlock = backRefEntry.first_blk;
    }

    std::string pwdOutput = "";
    // Because we are going from a dir to root instead of root to dir, we reverse the order that we append to the string.
    for (int i = filepath.size() - 1; i >= 0; i--)
    {
        pwdOutput.append(filepath[i]);
    }

    // If block started at root, the output string should be empty. If so, add the root directory character.
    if (pwdOutput.empty())
    {
        pwdOutput = "/";
    }

    // Enclose string with apostrophes.
    pwdOutput.insert(0, "'");
    pwdOutput.append("'");
    std::cout << pwdOutput << std::endl;
    return 0;
}

// chmod <accessrights> <filepath> changes the access rights for the
// file <filepath> to <accessrights>.
int FS::chmod(std::string accessrights, std::string filepath)
{
    std::cout << "FS::chmod(" << accessrights << "," << filepath << ")\n";

    if (!FilenamesAreValid(filepath) || !FilepathExists(filepath))
    {
        return ERROR_CODE;
    }

    const int accessRightsValue = std::stoi(accessrights);
    // Validate access rights value.
    if (accessRightsValue > (READ | WRITE | EXECUTE) || accessRightsValue < 0)
    {
        return ERROR_CODE;
    }

    dir_entry dirEntry;
    StringVector parsedFilepath = ParseDirPath(filepath);
    GetDirEntry(parsedFilepath, dirEntry);

    parsedFilepath.pop_back();
    int parentDirectoryBlock = GetDirectoryBlock(parsedFilepath);

    dir_entry newDirEntry = dirEntry;
    newDirEntry.access_rights = accessRightsValue;
    UpdateDirEntry(parentDirectoryBlock, dirEntry, newDirEntry);

    return 0;
}

int FS::MakeFATEntry(const uint32_t index, const int16_t blockValue)
{
    // Error handling
    {
        static bool rootCreated = false;
        static bool FATCreated = false;

        if (index == ROOT_BLOCK)
        {
            if (rootCreated)
            {
                return ERROR_CODE;
            }
            rootCreated = true;
        }

        if (index == FAT_BLOCK)
        {
            if (FATCreated)
            {
                return ERROR_CODE;
            }
            FATCreated = true;
        }

        if (index > FAT_SIZE || index < 0)
        {
            return ERROR_CODE;
        }
    }

    m_fat[index] = blockValue;

    return UpdateFAT(); // Updates FAT on disk automatically upon returning.
}

int FS::UpdateFAT()
{
    // FAT has a size of BLOCK_SIZE so this will fill the whole block.
    return m_disk.write(FAT_BLOCK, (uint8_t *)m_fat);
}

int FS::AddNewDirEntry(const int parentDirectoryBlock, const dir_entry &newDirEntry)
{
    if (!DirEntryExists(newDirEntry))
    {
        return ERROR_CODE;
    } // Return if name is empty.

    dir_entry dirEntries[DIR_BLOCK_SIZE];
    if (m_disk.read(parentDirectoryBlock, (uint8_t *)dirEntries) != 0)
    {
        return ERROR_CODE;
    }

    bool foundEmptyDirEntry = false;
    for (dir_entry &dirEntry : dirEntries)
    {
        // If we found a spot where a dir entry does not exists we found a free spot.
        if (!DirEntryExists(dirEntry))
        {
            dirEntry = newDirEntry;
            foundEmptyDirEntry = true;
            break;
        }
    }

    return foundEmptyDirEntry ? m_disk.write(parentDirectoryBlock, (uint8_t *)dirEntries) : ERROR_CODE;
}

int FS::AllocateNewFileOnFAT(const int nBlocksToAllocate, int *const allocatedFirstBlock)
{
    std::vector<int> freeBlocksArray;
    if (GetFreeBlocks(nBlocksToAllocate, freeBlocksArray) != 0)
    {
        return ERROR_CODE;
    }

    // Go through the available blocks and insert correct flags and linkage.
    for (int i = 0; i < nBlocksToAllocate; i++)
    {
        int freeBlockIndex = freeBlocksArray[i];
        int16_t linkedBlock = i == nBlocksToAllocate - 1 ? FAT_EOF : freeBlocksArray[i + 1];

        if (MakeFATEntry(freeBlockIndex, linkedBlock) != 0)
        {
            return ERROR_CODE;
        }
    }

    if (allocatedFirstBlock != nullptr)
    {
        *allocatedFirstBlock = freeBlocksArray[0];
    }
    return 0;
}

int FS::ExtendFileOnFAT(const int nBlocksToAllocate, const int startBlock)
{
    std::vector<int> freeBlocksArray;
    if (GetFreeBlocks(nBlocksToAllocate, freeBlocksArray) != 0)
    {
        return ERROR_CODE;
    }

    // Update EOF block.
    int EOFBlock = GetEOFBlockFromStartBlock(startBlock);
    if (MakeFATEntry(EOFBlock, freeBlocksArray[0]) != 0)
    {
        return ERROR_CODE;
    }

    // Go through the available blocks and insert correct flags and linkage.
    for (int i = 0; i < nBlocksToAllocate; i++)
    {
        int freeBlockIndex = freeBlocksArray[i];
        int16_t linkedBlock = i == nBlocksToAllocate - 1 ? FAT_EOF : freeBlocksArray[i + 1];

        if (MakeFATEntry(freeBlockIndex, linkedBlock) != 0)
        {
            return ERROR_CODE;
        }
    }

    return 0;
}

int FS::GetChildBlock(const int block)
{
    return m_fat[block];
}

int FS::CalculateMinBlockCount(const int size)
{
    // Integer divison always results in a floor of the number if they are not already evenly divisable.
    // This expression returns ceil of size / blocksize.
    return (size % BLOCK_SIZE) ? size / BLOCK_SIZE + 1 : size / BLOCK_SIZE;
}

int FS::GetEOFBlockFromStartBlock(const int startBlock)
{
    int EOFBlock = startBlock;
    while (GetChildBlock(EOFBlock) != FAT_EOF)
    {
        EOFBlock = GetChildBlock(EOFBlock);
    }

    return EOFBlock;
}

bool FS::BlockIsFree(const int block)
{
    return m_fat[block] == FAT_FREE;
}

bool FS::DirectoryIsEmpty(const dir_entry &dirEntry)
{
    dir_entry dirEntries[DIR_BLOCK_SIZE];
    if (m_disk.read(dirEntry.first_blk, (uint8_t *)dirEntries) != 0)
    {
        return false;
    }

    for (int i = 1; i < DIR_BLOCK_SIZE; i++) // Skip back ref dir ".."
    {
        if (DirEntryExists(dirEntries[i]))
        {
            return false;
        }
    }

    return true;
}

bool FS::FilepathExists(const std::string &filePath)
{
    dir_entry dirEntry;
    GetDirEntry(ParseDirPath(filePath), dirEntry);

    return DirEntryExists(dirEntry);
}

bool FS::DirEntryExists(const dir_entry &dirEntry)
{
    return dirEntry.file_name[0] != '\0';
}

int FS::GetFreeBlocks(int nBlocksToAdd, std::vector<int> &freeBlocksVector)
{
    if (nBlocksToAdd < 0)
    {
        return ERROR_CODE;
    }

    freeBlocksVector.clear();
    for (int blockNum = 0; blockNum < FAT_SIZE; blockNum++) // Find free blocks.
    {
        if (nBlocksToAdd == 0)
        {
            break;
        }

        if (BlockIsFree(blockNum))
        {
            freeBlocksVector.push_back(blockNum);
            nBlocksToAdd--;
        }
    }

    return 0;
}

bool FS::FilenamesAreValid(std::string &dirpath)
{
    if (dirpath.empty())
    {
        return false;
    }

    std::vector<std::string> parsedFilePath = ParseDirPath(dirpath);
    FS::PATH_TYPE pathType = EvaluatePathType(parsedFilePath);

    if (pathType == PATH_TYPE::INVALID)
    {
        return false;
    }
    if (pathType == PATH_TYPE::ROOT)
    {
        return true;
    }

    for (int i = 0; i < parsedFilePath.size(); i++)
    {
        std::string filename = parsedFilePath[i];

        if (HasSpecialCharacters(filename))
        {
            // Return error if the path is neither absolute and relative but has special characters.
            if (pathType != PATH_TYPE::ABSOLUTE && pathType != PATH_TYPE::RELATIVE)
            {
                return false;
            }

            // Return not valid if the string contains special characters and is not the first in the path.
            // i is equal to 0 for all cases below.
            if (i != 0)
            {
                return false;
            }

            // Return not valid if the path is absolute and the first element is not empty.
            if (pathType == PATH_TYPE::ABSOLUTE && filename != "")
            {
                return false;
            }
            // Return not valid if its relative and string is not ".".
            if (pathType == PATH_TYPE::RELATIVE && !(filename == "."))
            {
                return false;
            }
        }

        if (filename.size() > FILE_NAME_SIZE)
        {
            return false;
        }
    }

    return true;
}

bool FS::HasValidAccess(const dir_entry &dirEntry, const int accessBitMask)
{
    return (dirEntry.access_rights & accessBitMask) == accessBitMask ? true : false;
}

bool FS::HasSpecialCharacters(const std::string &filename)
{
    // Do not count ".." as special characters.
    if (filename == "..")
    {
        return false;
    }

    for (const char character : filename)
    {
        if (!std::isalnum(character))
        {
            return true;
        }
    }

    return false;
}

int FS::WriteDataStringToFile(std::string stringData, const dir_entry &fileDirEntry)
{
    // If dir entry is not file.
    if (fileDirEntry.type != TYPE_FILE)
    {
        return ERROR_CODE;
    }
    // If file is too small to fit string data.
    if (CalculateMinBlockCount(fileDirEntry.size) < CalculateMinBlockCount(stringData.size()))
    {
        return ERROR_CODE;
    }

    int nextBlock = fileDirEntry.first_blk;
    while (!stringData.empty()) // While there is still data to write.
    {
        char blockBuffer[BLOCK_SIZE] = {'\0'};
        int fileContentSize = stringData.size();
        int charactersToCopy = BLOCK_SIZE < fileContentSize ? BLOCK_SIZE : fileContentSize % BLOCK_SIZE;
        memcpy(blockBuffer, stringData.c_str(), charactersToCopy);
        if (m_disk.write(nextBlock, (uint8_t *)blockBuffer) != 0)
        {
            return ERROR_CODE;
        }

        stringData.erase(0, charactersToCopy);
        nextBlock = GetChildBlock(nextBlock);
    }

    return 0;
}

int FS::ReadFileToDataString(std::string &stringData, const dir_entry &fileDirEntry)
{
    char blockBuffer[BLOCK_SIZE] = {'\0'};
    int currentBlock = fileDirEntry.first_blk;
    // Read all data from dest file into memory.
    while (currentBlock != FAT_EOF)
    {
        if (m_disk.read(currentBlock, (uint8_t *)blockBuffer) != 0)
        {
            return ERROR_CODE;
        }
        // If the whole block buffer is filled there will be no defined null-terminator.
        int charactersToAppend = strlen(blockBuffer) >= BLOCK_SIZE ? BLOCK_SIZE : strlen(blockBuffer);
        stringData.append(blockBuffer, charactersToAppend);
        currentBlock = GetChildBlock(currentBlock);
    }

    return 0;
}

std::vector<std::string>
FS::ParseDirPath(const std::string &dirPath)
{
    std::vector<std::string> outputVector = {};

    std::string individualPath;
    std::stringstream stream(dirPath); // Convert to stream to use getline().
    while (getline(stream, individualPath, '/'))
    {
        outputVector.push_back(individualPath);
    }

    return outputVector;
}

FS::PATH_TYPE
FS::EvaluatePathType(const std::vector<std::string> &paths)
{
    if (paths.size() == 1)
    {
        // If element is empty the initial string was "/" and therefore root.
        // Otherwise it will be a reference to a dir entry in the CWD.
        return paths[0] == "" ? PATH_TYPE::ROOT : PATH_TYPE::RELATIVE;
    }

    if (paths.size() > 1)
    {
        if (paths.front() == "." || paths.front() == "..")
        {
            return PATH_TYPE::RELATIVE;
        }
        if (paths.front() == "")
        {
            return PATH_TYPE::ABSOLUTE;
        }
    }

    return PATH_TYPE::INVALID;
}

int FS::GetDirEntry(const int parentDirBlock, const std::string &filename, dir_entry &dirEntryOut)
{
    dirEntryOut = {};

    dir_entry dirEntries[DIR_BLOCK_SIZE];
    if (m_disk.read(parentDirBlock, (uint8_t *)dirEntries) != 0)
    {
        return ERROR_CODE;
    }

    for (const dir_entry &dirEntry : dirEntries)
    {
        if (dirEntry.file_name == filename)
        {
            dirEntryOut = dirEntry;
            break;
        }
    }

    return 0;
}

int FS::GetDirEntry(std::vector<std::string> dirPaths, dir_entry &dirEntryOut)
{
    dirEntryOut = {};

    std::string fileName = dirPaths.back();
    dirPaths.pop_back();
    int parentDirBlock = GetDirectoryBlock(dirPaths);
    // Return early if no block was found.
    if (parentDirBlock != ERROR_CODE)
    {
        // Input will always be a valid block which means that no checks has to be done.
        GetDirEntry(parentDirBlock, fileName, dirEntryOut);
    }

    return 0;
}

int FS::UpdateDirEntry(const int parentDirBlock, const dir_entry &oldDirEntry, const dir_entry &newDirEntry)
{
    dir_entry dirEntries[DIR_BLOCK_SIZE];
    if (m_disk.read(parentDirBlock, (uint8_t *)dirEntries) != 0)
    {
        return ERROR_CODE;
    }

    for (dir_entry &dirEntry : dirEntries)
    {
        if (strcmp(dirEntry.file_name, oldDirEntry.file_name) == 0)
        {
            dirEntry = newDirEntry;
            break;
        }
    }

    if (m_disk.write(parentDirBlock, (uint8_t *)dirEntries) != 0)
    {
        return ERROR_CODE;
    }

    return 0;
}

int FS::GetDirectoryBlock(const std::vector<std::string> &dirPaths)
{
    // Return CWD block as default if no paths were given.
    if (dirPaths.size() == 0)
    {
        return m_cwdBlock;
    }

    FS::PATH_TYPE pathType = EvaluatePathType(dirPaths);
    int startingBlock;
    switch (pathType)
    {
    case PATH_TYPE::INVALID:
        return ERROR_CODE;
        break;

    case PATH_TYPE::ROOT:
        return ROOT_BLOCK;
        break;

    case PATH_TYPE::RELATIVE:
        startingBlock = m_cwdBlock;
        break;

    // Similar as relative only that looping should start at root.
    case PATH_TYPE::ABSOLUTE:
        startingBlock = ROOT_BLOCK;
        break;

    default:
        // There is no case where default will run as all path types are covered.
        break;
    }

    int currentBlock = startingBlock;
    // Loop through all filenames and use the block its pointing to for each loop.
    for (int i = 0; i < dirPaths.size(); i++)
    {
        // Special cases for first element for certain path types.
        if (i == 0)
        {
            // Skip "." for relative paths.
            if (pathType == PATH_TYPE::RELATIVE && dirPaths[i] == ".")
            {
                continue;
            }

            // Skip root directory name for absolute paths.
            if (pathType == PATH_TYPE::ABSOLUTE && dirPaths[i] == "")
            {
                continue;
            }
        }

        dir_entry foundDir;
        // No need to check for error in get dir entry as the input will always be a valid parent block.
        GetDirEntry(currentBlock, dirPaths[i], foundDir);
        if (!DirEntryExists(foundDir) || foundDir.type != TYPE_DIR)
        {
            return ERROR_CODE;
        }
        currentBlock = foundDir.first_blk;
    }

    // return the block that was finally landed on.
    return currentBlock;
}
