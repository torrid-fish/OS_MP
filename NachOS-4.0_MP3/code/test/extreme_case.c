#include "syscall.h"

int main(void) {
	char test[] = "abcdefghijklmnopqrstuvwxyz";
    char temp [26];
	int success= Create("file2.test");
    int i;
	OpenFileId fid, fid2;
	if (success != 1) MSG("Failed on creating file");
    
    /* Test 1: Open multiple times */ 
    
    for (i = 0; i < 30; i++)
    	fid = Open("file2.test");
    Close(5);       // Close one file first
    Close(10);      // Close another file
    
    /* Test 2: Open Non-exist file */
    fid = Open("file7.test");

    /* Test 3: Close Non-exist file */
    Close(5);       // Non used fid
    Close(1000);     // Out of range fid

    /* TEST 4: Write and Read same file with different fid */
    // Should be synchronized?
    fid = Open("file2.test");   // Should be 5
    fid2 = Open("file2.test");  // Should be 10
    Write(test, 1, fid);
    Read(temp, 1, fid);
    Read(temp, 1, fid2);
    Write(test + 1, 25, fid2);
    Read(temp, 100, fid);
    Read(temp, 100, fid2);

    Halt();
}

