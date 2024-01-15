// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define NumDirEntries 64
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// processedData
//  This class will represent the data after processed by ProcessPath.
//  Assume the input path looks like `/test/a/b/c`, then the corresponding 
//  parts of this class look like:
//
//          test
//          \ a
//             \ b      <= directory and directoryFile
//                \ c   <= name
//----------------------------------------------------------------------
class processedData {
public:
    Directory* directory;
    OpenFile* directoryFile;
    char name[10];
};

processedData ProcessPath(char* path) {
    // Initialize current directory
    int currentSector = DirectorySector;
    Directory* currentDir = new Directory(NumDirEntries);
    OpenFile* temp = new OpenFile(DirectorySector);
    currentDir->FetchFrom(temp);
    delete temp;
    // Calculate length of path
    int len = 0;
    while (path[len] != '\0') len++;
    // Initalize variables
    int div = (path[0]=='/'), last = div;
    char name[10];
    // Parse path
    while (path[div] != '\0' && div < len) {
        if (path[div] == '/') {
            memcpy(name, path + last, (div-last)*sizeof(char));
            name[(div-last)] = '\0';
            int sector = currentDir->Find(name);
            ASSERT(sector != -1);
            OpenFile* temp = new OpenFile(sector);
            currentDir->FetchFrom(temp);
            currentSector = sector;
            delete temp;
            last = div+1;
        }
        div++;
    }
    // Store result
    processedData result;
    memcpy(result.name, path + last, (div-last)*sizeof(char));
    result.name[(div-last)] = '\0';
    result.directory = currentDir;
    result.directoryFile = new OpenFile(currentSector);
    // return
    return result;
}

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    DEBUG(dbgFile, "Initializing the file system.");
    if (format)
    {
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        currentFile = NULL;

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

        if (debug->IsEnabled('f'))
        {
            freeMap->Print();
            directory->Print(0, 0);
        }
        delete freeMap;
        delete directory;
        delete mapHdr;
        delete dirHdr;
    }
    else
    {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        // Initialize currentFile
        currentFile = NULL;
    }
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize)
{
    // Default size
    bool isDirectory = (initialSize == -1);
    
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG(dbgFile, "Creating file " << name << " size " << (isDirectory ? DirectoryFileSize : initialSize));

    // MP4 III: Process Path
    processedData result = ProcessPath(name);

    if (result.directory->Find(result.name) != -1)
        success = FALSE; // file is already in directory
    else
    {
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet(); // find a sector to hold the file header
        if (sector == -1)
            success = FALSE; // no free block for file header
        else if (!result.directory->Add(result.name, sector, isDirectory))
            success = FALSE; // no space in directory
        else
        {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, isDirectory ? DirectoryFileSize : initialSize))
                success = FALSE; // no space on disk for data
            else
            {
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);
                if (isDirectory) {
                    // Initialize Directory
                    Directory* newDirectory = new Directory(NumDirEntries);
                    OpenFile* newDirectoryFile = new OpenFile(sector);
                    newDirectory->WriteBack(newDirectoryFile);
                    delete newDirectory;
                    delete newDirectoryFile;
                }
                result.directory->WriteBack(result.directoryFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
        }
        delete freeMap;
    }
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile * FileSystem::Open(char *name)
{
    OpenFile *openFile = NULL;

    DEBUG(dbgFile, "Opening file" << name);

    // MP4 III: Process Path
    processedData result = ProcessPath(name);

    int sector = result.directory->Find(result.name);
    if (sector >= 0)
        openFile = new OpenFile(sector); // file was found in directory
    return openFile; // return NULL if not found
}

// MP4 II-I: Implement five system calls //
// Use original create

//----------------------------------------------------------------------
// FileSystem::OpenAFile
//----------------------------------------------------------------------

OpenFileId FileSystem::OpenAFile(char *name)
{
    DEBUG(dbgFile, "Opening file" << name);

    // MP4 III: Process Path
    processedData result = ProcessPath(name);

    int sector = result.directory->Find(result.name);
    if (sector >= 0)
        currentFile = new OpenFile(sector); // file was found in directory
    return (currentFile != NULL); // return 0 if not found
}

//----------------------------------------------------------------------
// FileSystem::ReadFile
//----------------------------------------------------------------------

int FileSystem::ReadFile(char *buffer, int size, OpenFileId id)
{
    return currentFile->Read(buffer, size*sizeof(char));
}

//----------------------------------------------------------------------
// FileSystem::WriteFile
//----------------------------------------------------------------------

int FileSystem::WriteFile(char *buffer, int size, OpenFileId id)
{
    return currentFile->Write(buffer, size*sizeof(char));
}

//----------------------------------------------------------------------
// FileSystem::CloseFile
//----------------------------------------------------------------------

int FileSystem::CloseFile(OpenFileId id)
{
    delete currentFile;
    currentFile = NULL;
    return 1; // Always success
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(char *name)
{
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;

    // MP4 III: Process Path
    processedData result = ProcessPath(name);

    int sector = result.directory->Find(result.name);
    if (sector == -1)
    {
        return FALSE; // file not found
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile, NumSectors);

    fileHdr->Deallocate(freeMap);                       // remove data blocks
    freeMap->Clear(sector);                             // remove header block
    result.directory->Remove(result.name);

    freeMap->WriteBack(freeMapFile);                    // flush to disk
    result.directory->WriteBack(result.directoryFile);  // flush to disk
    delete fileHdr;
    delete freeMap;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::RecursiveRemove
// Remove the folder recursively
//----------------------------------------------------------------------

void RecursiveRemoveCall(Directory* directory, PersistentBitmap* freeMap) {
    for (int i = 0; i < NumDirEntries; i++) {
        DirectoryEntry entry = directory->getTableEntry(i);
        if (entry.inUse) {
            if (entry.isDirectory) {
                // Recursively remove
                Directory* subDirectory = new Directory(NumDirEntries);
                OpenFile* subDirectoryFile = new OpenFile(entry.sector);
                subDirectory->FetchFrom(subDirectoryFile);
                RecursiveRemoveCall(subDirectory, freeMap);
                delete subDirectory;
                delete subDirectoryFile;
            }
            // Remove this file / subdirectory
            FileHeader* fileHdr = new FileHeader;
            fileHdr->FetchFrom(entry.sector);
            fileHdr->Deallocate(freeMap);   // Free data sectors
            freeMap->Clear(entry.sector);   // Free header sector
            directory->Remove(entry.name);
        }
    }
}

bool FileSystem::RecursiveRemove(char *name) {
    PersistentBitmap *freeMap;
    freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    FileHeader *fileHdr;

    // MP4 III: Process Path
    processedData result = ProcessPath(name);

    // Get directory entry of target file
    DirectoryEntry entry = result.directory->getTableEntry(result.name);
    // If it is a directory
    if (entry.isDirectory) {
        Directory* targetDiretory = new Directory(NumDirEntries);
        OpenFile* targetDiretoryFile = new OpenFile(entry.sector);  
        targetDiretory->FetchFrom(targetDiretoryFile);
        RecursiveRemoveCall(targetDiretory, freeMap);
        delete targetDiretory;
        delete targetDiretoryFile;
    }
    // Remove the target file / directory
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(entry.sector);
    fileHdr->Deallocate(freeMap);                      // remove data blocks
    freeMap->Clear(entry.sector);                      // remove header block
    result.directory->Remove(result.name);

    freeMap->WriteBack(freeMapFile);                   // flush to disk
    result.directory->WriteBack(result.directoryFile); // flush to disk
    delete fileHdr;
    delete freeMap;
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List(char* name)
{
    int len = 0;
    while(name[len] != '\0') len++;
    char* temp = new char [len+2];
    strncpy(temp, name, len);
    if (temp[len-1] != '/')
        temp[len] = '/', temp[len+1] = '\0';

    // MP4 III: Process Path
    processedData result = ProcessPath(temp);

    result.directory->FetchFrom(result.directoryFile);
    result.directory->List();
    delete temp;
}

//----------------------------------------------------------------------
// FileSystem::RecursiveList
// 	List all the files in the file system directory Recursively.
//----------------------------------------------------------------------

void RecursiveListCall(int depth, Directory* directory) {
    for (int i = 0; i < NumDirEntries; i++) {
        DirectoryEntry entry = directory->getTableEntry(i);
        if (entry.inUse) {
            if (entry.isDirectory) {
                for (int j = 0; j < 4 * depth; j++) printf(" ");
                printf("[D] %s\n", entry.name);
                Directory* subDirectory = new Directory(NumDirEntries);
                OpenFile* subDirectoryFile = new OpenFile(entry.sector);
                subDirectory->FetchFrom(subDirectoryFile);
                RecursiveListCall(depth + 1, subDirectory);
                delete subDirectory;
                delete subDirectoryFile;
            }
            else {
                for (int j = 0; j < 4 * depth; j++) printf(" ");
                printf("[F] %s\n", entry.name);
            }
        }
    }
}

void FileSystem::RecursiveList(char* name) {
    int len = 0;
    while(name[len] != '\0') len++;
    char* temp = new char [len+2];
    strncpy(temp, name, len);
    if (temp[len-1] != '/')
        temp[len] = '/', temp[len+1] = '\0';

    // MP4 III: Process Path
    processedData result = ProcessPath(temp);

    result.directory->FetchFrom(result.directoryFile);
    RecursiveListCall(0, result.directory);
    delete temp;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print(bool printSector, bool printFreeMap)
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("* Total Disk size:    ");
    int size = NumSectors * SectorSize;
    if (size < 1024)
		printf(" %6d  B\n", size);
	else if (size < 1024 * 1024)
		printf(" %6.2f KB\n", (float)size/1024);
	else
		printf(" %6.2f MB\n", (float)size/(1024 * 1024));

    printf("* Current usable size:");
    size = freeMap->NumClear() * SectorSize;
    if (size < 1024)
		printf(" %6d  B\n", size);
	else if (size < 1024 * 1024)
		printf(" %6.2f KB\n", (float)size/1024);
	else
		printf(" %6.2f MB\n", (float)size/(1024 * 1024));
    printf("\n");

    printf("* Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print(0, FreeMapSector, false);
    printf("\n");

    printf("* Directory (root) file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print(0, DirectorySector, false);
    printf("\n");
    
    if (printFreeMap) {
        printf("* FreeMap data:\n");
        freeMap->Print();
        printf("\n");
    }

    printf("* Directory contents:\n");
    directory->FetchFrom(directoryFile);
    directory->Print(0, printSector);

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

#endif // FILESYS_STUB
