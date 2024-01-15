// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::FileHeader
//	There is no need to initialize a fileheader,
//	since all the information should be initialized by Allocate or FetchFrom.
//	The purpose of this function is to keep valgrind happy.
//----------------------------------------------------------------------
FileHeader::FileHeader()
{
	numBytes = -1;
	numSectors = -1;
	memset(headerSectorsL1, -1, sizeof(headerSectorsL1));
	memset(headerSectorsL2, -1, sizeof(headerSectorsL2));
	memset(headerSectorsL3, -1, sizeof(headerSectorsL3));
	memset(dataSectors, -1, sizeof(dataSectors));
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileHeader::~FileHeader
//	Currently, there is not need to do anything in destructor function.
//	However, if you decide to add some "in-core" data in header
//	Always remember to deallocate their space or you will leak memory
//----------------------------------------------------------------------
FileHeader::~FileHeader()
{
	// nothing to do now
}

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool FileHeader::Allocate(PersistentBitmap *freeMap, int fileSize)
{
	numBytes = fileSize;
	numSectors = divRoundUp(fileSize, SectorSize);

	// MP4 II-II: Larger File Size
	// MP4 bonus: Even more larger
	// 4 level structure

	int	L3Num = divRoundUp(numSectors, 32);
	int L2Num = divRoundUp(L3Num, 32);
	int L1Num = divRoundUp(L2Num, 32);
	ASSERT(L1Num <= NumDirect);


	if (freeMap->NumClear() < numSectors + L3Num + L2Num + L1Num)
		return FALSE; // not enough space
	
	for (int i = 0; i < L1Num; i++) {
		headerSectorsL1[i] = freeMap->FindAndSet();
		ASSERT(headerSectorsL1[i] >= 0);
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32 && cnt < L2Num; j++, cnt++) {
			headerSectorsL2[i][j] = freeMap->FindAndSet();
			ASSERT(headerSectorsL2[i][j] >= 0);
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32 && cnt < L3Num; k++, cnt++) {
				headerSectorsL3[i][j][k] = freeMap->FindAndSet();	
				ASSERT(headerSectorsL3[i][j][k] >= 0);
			}
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32; k++) {
				for (int l = 0; l < 32 && cnt < numSectors; l++, cnt++) {
					dataSectors[i][j][k][l] = freeMap->FindAndSet();	
					ASSERT(dataSectors[i][j][k][l] >= 0);
				}
			}
		}
	}
	return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void FileHeader::Deallocate(PersistentBitmap *freeMap)
{
	int	L3Num = divRoundUp(numSectors, 32);
	int L2Num = divRoundUp(L3Num, 32);
	int L1Num = divRoundUp(L2Num, 32);
	ASSERT(L1Num <= NumDirect);
	
	for (int i = 0; i < L1Num; i++) {
		ASSERT(freeMap->Test((int)headerSectorsL1[i])); // ought to be marked!
		freeMap->Clear((int)headerSectorsL1[i]);
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32 && cnt < L2Num; j++, cnt++) {
			ASSERT(freeMap->Test((int)headerSectorsL2[i][j])); // ought to be marked!
			freeMap->Clear((int)headerSectorsL2[i][j]);
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32 && cnt < L3Num; k++, cnt++) {
				ASSERT(freeMap->Test((int)headerSectorsL3[i][j][k])); // ought to be marked!
				freeMap->Clear((int)headerSectorsL3[i][j][k]);
			}
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32; k++) {
				for (int l = 0; l < 32 && cnt < numSectors; l++, cnt++) {
					ASSERT(freeMap->Test((int)dataSectors[i][j][k][l])); // ought to be marked!
					freeMap->Clear((int)dataSectors[i][j][k][l]);
				}
			}
		}
	}
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector)
{
	char buf[SectorSize];
	// The first sector
	kernel->synchDisk->ReadSector(sector, buf);
	memcpy(&numBytes, buf, sizeof(numBytes));
	memcpy(&numSectors, buf + 4, sizeof(numSectors));
	memcpy(&headerSectorsL1, buf + 8, sizeof(headerSectorsL1));
	
	int	L3Num = divRoundUp(numSectors, 32);
	int L2Num = divRoundUp(L3Num, 32);
	int L1Num = divRoundUp(L2Num, 32);
	ASSERT(L1Num <= NumDirect);

	// The remaining sectors
	for (int i = 0; i < L1Num; i++) {
		kernel->synchDisk->ReadSector(headerSectorsL1[i], buf);
		memcpy(headerSectorsL2[i], buf, sizeof(headerSectorsL2[i]));
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32 && cnt < L2Num; j++, cnt++) {
			kernel->synchDisk->ReadSector(headerSectorsL2[i][j], buf);
			memcpy(headerSectorsL3[i][j], buf, sizeof(headerSectorsL3[i][j]));
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32 && cnt < L3Num; k++, cnt++) {
				kernel->synchDisk->ReadSector(headerSectorsL3[i][j][k], buf);
				memcpy(dataSectors[i][j][k], buf, sizeof(dataSectors[i][j][k]));
			}
		}
	}
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector)
{
	char buf[SectorSize];
	// The first sector
	memcpy(buf, &numBytes, sizeof(numBytes));
	memcpy(buf + 4, &numSectors, sizeof(numSectors));
	memcpy(buf + 8, &headerSectorsL1, sizeof(headerSectorsL1));
	kernel->synchDisk->WriteSector(sector, buf);

	int	L3Num = divRoundUp(numSectors, 32);
	int L2Num = divRoundUp(L3Num, 32);
	int L1Num = divRoundUp(L2Num, 32);
	ASSERT(L1Num <= NumDirect);

	// The remaining sectors
	for (int i = 0; i < L1Num; i++) {
		memcpy(buf, headerSectorsL2[i], sizeof(headerSectorsL2[i]));
		kernel->synchDisk->WriteSector(headerSectorsL1[i], buf);
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32 && cnt < L2Num; j++, cnt++) {
			memcpy(buf, headerSectorsL3[i][j], sizeof(headerSectorsL3[i][j]));
			kernel->synchDisk->WriteSector(headerSectorsL2[i][j], buf);
		}
	}
	for (int i = 0, cnt = 0; i < NumDirect; i++) {
		for (int j = 0; j < 32; j++) {
			for (int k = 0; k < 32 && cnt < L3Num; k++, cnt++) {
				memcpy(buf, dataSectors[i][j][k], sizeof(dataSectors[i][j][k]));
				kernel->synchDisk->WriteSector(headerSectorsL3[i][j][k], buf);
			}
		}
	}
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int FileHeader::ByteToSector(int offset)
{
	int sectorNum = offset / SectorSize; // Calculate sector number
	int	L3Num = (sectorNum >> 5) % 32;
	int L2Num = (sectorNum >> 10) % 32;
	int L1Num = (sectorNum >> 15);
	ASSERT(L1Num < NumDirect);
	return (dataSectors[L1Num][L2Num][L3Num][sectorNum % 32]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength()
{
	return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the size and # of sectors of the header and data in this file.
//----------------------------------------------------------------------

void FileHeader::Print(int depth, int sector, bool printSector)
{
	int	L3Num = divRoundUp(numSectors, 32);
	int L2Num = divRoundUp(L3Num, 32);
	int L1Num = divRoundUp(L2Num, 32);
	ASSERT(L1Num <= NumDirect);

	// Num of header sectors
    for (int j = 0; j <= 4 * depth; j++) cout << " |"[j%4==0];
	int num = 1 + L1Num + L2Num + L3Num, size = SectorSize * num;
	if (size < 1024)
		printf("  File Header size: %6d  B, # of sectors: %6d", size, num);
	else if (size < 1024 * 1024)
		printf("  File Header size: %6.2f KB, # of sectors: %6d", (float)size/1024, num);
	else
		printf("  File Header size: %6.2f MB, # of sectors: %6d", (float)size/(1024 * 1024), num);
	// Header sectors
	if (printSector) {
		printf(" (");	
		for (int i = 0; i < L1Num; i++) 
			printf("%6d, ", headerSectorsL1[i]);
		for (int i = 0, cnt = 0; i < L1Num; i++) 
			for (int j = 0; j < 32 && cnt < L2Num; j++, cnt++)
				printf ("%6d, ", headerSectorsL2[i][j]);
		for (int i = 0, cnt = 0; i < L1Num; i++) 
			for (int j = 0; j < 32; j++)
				for (int k = 0; k < 32 && cnt < L3Num; k++, cnt++)
					printf ("%6d%c ", headerSectorsL3[i][j][k], ",)"[cnt==L3Num-1]);
	}
	printf("\n");

	// Num of data sectors
    for (int j = 0; j <= 4 * depth; j++) cout << " |"[j%4==0];
	size = SectorSize * numSectors;
	if (size < 1024)
		printf("  File Data   size: %6d  B, # of sectors: %6d", size, numSectors);
	else if (size < 1024 * 1024)
		printf("  File Data   size: %6.2f KB, # of sectors: %6d", (float)size/1024, numSectors);
	else 
		printf("  File Data   size: %6.2f MB, # of sectors: %6d", (float)size/(1024*1024), numSectors);

	printf("\n");
}
