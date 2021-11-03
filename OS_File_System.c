// OS functions pertaining to simple write-once file system
// John Tadrous
// August 9, 2020


#include "tm4c123gh6pm.h"
#include "tm4c123gh6pm_def.h"


uint32_t Disk_Start_Address=0x20000; // First address in the ROM

uint8_t	RAM_Directory[256];				// Directory loaded in RAM
uint8_t	RAM_FAT[256];							// FAT in RAM
uint8_t Access_FB;                // Access Feedback


void OS_FS_Init(void);
uint8_t OS_File_New( void);
uint8_t OS_File_Size(uint8_t);
uint8_t find_free_sector(void);
uint8_t last_sector(uint8_t);
void append_fat(uint8_t, uint8_t);
uint8_t OS_File_Read( uint8_t, uint8_t, uint8_t*);
uint8_t eDisk_WriteSector(uint8_t*, uint8_t);
uint8_t OS_File_Flush( void);
int Flash_Erase(uint32_t);
uint8_t OS_File_Format( void);

// OS_FS_Init()  Temporarily initialize RAM_Directory and RAM_FAT
void OS_FS_Init(void){
  int i;
  for(i=0; i<256 ; i++){
    RAM_Directory[i]=255;
    RAM_FAT[i]=255;
  }
}


//******** OS_File_New************* 
// Returns a file number of a new file for writing 
// Inputs: none 
// Outputs: number of a new file 
// Errors: return 255 on failure or disk full
uint8_t OS_File_New(void){
	uint8_t new_file_number = 255;
	uint8_t next_free_sector_index = find_free_sector();
	
	if (next_free_sector_index != 255) {
		// disk not full
		for (int i = 0; i < 255; ++i)
		{
			if (RAM_Directory[i] == 255) {
				// directory not full
				new_file_number = i;
				
				// update directory
				RAM_Directory[i] = next_free_sector_index;
				break;
			}
		}
	}
	return new_file_number;
}


//******** OS_File_Size************* 
// Check the size of this file 
// Inputs: num, 8-bit file number, 0 to 254 
// Outputs: 0 if empty, otherwise the number of sectors 
// Errors: none 
uint8_t OS_File_Size(uint8_t num){
	uint8_t ptr = RAM_Directory[num];
	uint8_t size = 0;

	while (ptr != 255) {
		// one more sector in file
		++size;
		
		// find next sector in file
		ptr = RAM_FAT[ptr];
	}
	
	return size;
}

//******** OS_File_Append************* 
// Save 512 bytes into the file 
// Inputs: num, 8-bit file number, 0 to 254 
// buf, pointer to 512 bytes of data 
// Outputs: 0 if successful 
// Errors: 255 on failure or disk full 
uint8_t OS_File_Append(uint8_t num, uint8_t buf[512]){
 
}

// Helper function find_free_sector returns the logical 
// address of the first free sector
uint8_t find_free_sector(void){
	uint8_t free_sector_index = 255;
	
	for (int i = 0; i < 255; ++i) {
		if (RAM_FAT[i] == 255) {
			free_sector_index = i;
			break;
		}
	}
	
	return free_sector_index;
}

// Helper function last_sector returns the logical address
// of the last sector assigned to the file whose number is 'start'
uint8_t last_sector(uint8_t start){
	uint8_t ptr = RAM_Directory[start];
	while(RAM_FAT[ptr] != 255){
			ptr = RAM_FAT[ptr];
	}
	return ptr;
}


// Helper function append_fat() modifies the FAT to append 
// the sector with logical address n to the sectors of file
// num
void append_fat(uint8_t num, uint8_t n){
 
}


// eDisk_WriteSector
// input: pointer to a 512-byte data buffer in RAM buf[512],
//        sector logical address n
// output: 0 if no error, 1 if error
// use the Flash_Write function
uint8_t eDisk_WriteSector(uint8_t buf[512], uint8_t n){
 
}


//******** OS_File_Read************* 
// Read 512 bytes from the file 
// Inputs: num, 8-bit file number, 0 to 254 
//         location, order of the sector in the file, 0 to 254 
//         buf, pointer to 512 empty spaces in RAM 
// Outputs: 0 if successful 
// Errors: 255 on failure because no data 
uint8_t OS_File_Read( uint8_t num, uint8_t location, uint8_t buf[512]){
	uint8_t ptr = RAM_Directory[num];
	ptr = RAM_FAT[ptr];
	for(int i = 0; i<location ; i++){
			if(ptr == 255){
				return ptr;
			}
			ptr = RAM_FAT[ptr];
	}

	uint8_t* sectorReadStart = (uint8_t*) Disk_Start_Address+(ptr*4);
	for(int i = 0; i<512; i++){
		buf[i] = *(sectorReadStart+i);
	}
	return 0;
}

//******** OS_File_Format************* 
// Erase all files and all data 
// Inputs: none 
// Outputs: 0 if success 
// Errors: 255 on disk write failure 
uint8_t OS_File_Format( void){
  uint32_t address;
  address = 0x00020000; // start of disk
  while( address <= 0x00040000){
    Flash_Erase(address); // erase 1k block
    address = address + 1024;
  }
}

//******** OS_File_Flush************* 
// Update working buffers onto the disk 
// Power can be removed after calling flush 
// Inputs: none 
// Outputs: 0 if success 
// Errors: 255 on disk write failure 
uint8_t OS_File_Flush(void){
  uint32_t endOfDisk = 0x0003FFFF; 
	uint8_t* sectorWriteStart = (uint8_t*) endOfDisk;
	for(int i = 0; i<255; i++){
		*(sectorWriteStart+i) = RAM_Directory[i];
	}
	for(int i = 0; i<255; i++){
		*(sectorWriteStart+i) = RAM_FAT[i];
	}
	return 0;
}


