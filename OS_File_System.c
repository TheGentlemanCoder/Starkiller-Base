// OS functions pertaining to simple write-once file system
// John Tadrous
// August 9, 2020


#include "tm4c123gh6pm.h"
#include "tm4c123gh6pm_def.h"
#include "FlashProgram.h"

uint32_t Sector_Size = 0x0200;
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
	uint8_t new_sector = find_free_sector();
	uint8_t retVal = 0;
	
	if (new_sector != 255) {
		// at least one sector still available
		eDisk_WriteSector(buf, new_sector);
		
		// update FAT
		append_fat(num, new_sector);
	} else {
		// new_sector == 255, disk full
		retVal = new_sector;
	}
	
	return retVal;
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

}


// Helper function append_fat() modifies the FAT to append 
// the sector with logical address n to the sectors of file
// num
void append_fat(uint8_t num, uint8_t n){
	uint8_t penultimate_file_sector = last_sector(num);

	// make previous last sector point to new last sector
	RAM_FAT[penultimate_file_sector] = n;
	// update last sector FAT entry to indicate that its the last sector
	RAM_FAT[num] = 255;
}


// eDisk_WriteSector
// input: pointer to a 512-byte data buffer in RAM buf[512],
//        sector logical address n
// output: 0 if no error, 1 if error
// use the Flash_Write function
uint8_t eDisk_WriteSector(uint8_t buf[512], uint8_t n){
	uint8_t retVal = 0;
	uint32_t physical_address;
	uint32_t little_endian_val;
	
	// calculate first physical address of sector
	physical_address = Disk_Start_Address + n * Sector_Size;
	
	for (int i = 0; i < 128; ++i) {
		// write buffer in 4-byte increments (512 / 4 = 128)
		
		// recall that TM4C123 is Little-Endian architecture
		little_endian_val = 0;
		
		// byte 0 in LSB (lowest address)
		little_endian_val |= buf[4 * i + 0] & 0x000000FF;
		// byte 1
		little_endian_val |= buf[4 * i + 1] & 0x0000FF00;
		// byte 2
		little_endian_val |= buf[4 * i + 2] & 0x00FF0000;
		// byte 3 in MSB (highest address)
		little_endian_val |= buf[4 * i + 3] & 0xFF000000;
		
		// write value to disk and check if an error occurred
		retVal |= Flash_Write(physical_address, little_endian_val);
		
		// increment physical address to next 32-bit word
		physical_address += 4;
	}
	
	return retVal;
}


//******** OS_File_Read************* 
// Read 512 bytes from the file 
// Inputs: num, 8-bit file number, 0 to 254 
//         location, order of the sector in the file, 0 to 254 
//         buf, pointer to 512 empty spaces in RAM 
// Outputs: 0 if successful 
// Errors: 255 on failure because no data 
uint8_t OS_File_Read( uint8_t num, uint8_t location, uint8_t buf[ 512]){
 
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
uint8_t OS_File_Flush( void){
}


