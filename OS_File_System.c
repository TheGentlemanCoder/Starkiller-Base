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


void LED_Init(void);
void LED_Red(void);
void LED_Green(void);
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

void LED_Init(void) {
	 //Setting up RGB output
	SYSCTL->RCGCGPIO |= 0x20;                   // initialize clock for port F
	while ((SYSCTL->PRGPIO & 0x20) != 0x20) {}; // wait until ready
	GPIOF->PCTL &= ~0x0000FFF0;     // configure port PF1-PF3 as GPIO
	GPIOF->AMSEL &= ~0x0E;          // disable analog mode PF1-PF3
	GPIOF->AFSEL &= ~0x0E;          // disable alternative functions PF1-PF3
	GPIOF->DIR |= 0x0E;             // set pins PF0-PF3 as outputs	
	GPIOF->DEN |= 0x0E;             // enable ports PF1-PF3
}

void LED_Red(void) {
	// clear LED
	GPIOF->DATA &= ~0x0E;
	// set LED red
	GPIOF->DATA |= 0x02;
}

void LED_Green(void) {
	// clear LED
	GPIOF->DATA &= ~0x0E;
	// set LED green
	GPIOF->DATA |= 0x08;
}

// OS_FS_Init()  Temporarily initialize RAM_Directory and RAM_FAT
void OS_FS_Init(void){
	LED_Init();
	
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
	for (int i = 0; i < 255; ++i)
	{
		if (RAM_Directory[i] == 255) {
			// directory not full
			new_file_number = i;
			break;
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
	LED_Red();
	uint8_t retVal = 0;
	uint8_t file_first_sector = RAM_Directory[num];
	uint8_t next_free_sector = find_free_sector();
	
	if (next_free_sector == 255) {
		// disk is full
		retVal = 255;
	} else {
		// at least one sector still available
		retVal = eDisk_WriteSector(buf, next_free_sector);
		
		// update FAT
		append_fat(num, next_free_sector);
	}
	
	LED_Green();
	return retVal;
}

// Helper function find_free_sector returns the logical 
// address of the first free sector
uint8_t find_free_sector(void){
	uint8_t free_sector_index = 255;
	uint8_t highest_file_sector = 0;
	
	// get number of files in disk
	uint8_t num_files = 0;
	uint8_t file_num;
	for (file_num = 0; file_num < 255; ++file_num) {
		if (RAM_Directory[file_num] != 255) {
			++num_files;
		} else {
			break;
		}
	}
	
	if (num_files == 0) {
		// special case: no files yet, so we return first sector
		return 0;
	}
	
	uint8_t ptr;
	
	for (file_num = 0; file_num < num_files; ++file_num) {
		// step through file until we reach the end
		ptr = RAM_Directory[file_num];
		
		while (RAM_FAT[ptr] != 255) {
			if (ptr > highest_file_sector) {
				highest_file_sector = ptr;
			}
			
			// move to next sector
			ptr = RAM_FAT[ptr];
		}
	}
	
	// return the sector immediately following
	// the last claimed sector on the disk
	free_sector_index = highest_file_sector + 1; 
	
	// used to keep track if free_sector_index changed during any iteration
	uint8_t dirty;
	
	do {
		dirty = 0;
		for (file_num = 0; file_num < num_files; ++file_num) {
			// ensure that this sector is referenced by a file in the directory
			if (RAM_Directory[file_num] == free_sector_index) {
				// increment to the next sector - if that sector is also
				// referenced, the next iteration will take care of it
				++free_sector_index;
				++dirty;
				continue;
			}
			
			// ensure that this sector is not referenced by a sector in the FAT
			ptr = RAM_Directory[file_num];
			
			while (ptr != 255) {
				if (RAM_FAT[ptr] == free_sector_index) {
					++free_sector_index;
					++dirty;
					continue;
				}
				
				ptr = RAM_FAT[ptr];
			}
		}
	} while (dirty > 0);
	
	return free_sector_index;
}

// Helper function last_sector returns the logical address
// of the last sector assigned to the file whose number is 'start'
uint8_t last_sector(uint8_t start){
	// files must occupy at least one sector
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
	// get first sector pointed to by directory entry 
	uint8_t ptr = RAM_Directory[num];
	uint8_t prev_ptr; // cache the last pointer used while iterating
	
	if (ptr == 255) {
		// first write to file, no need to update FAT
		RAM_Directory[num] = n;
	} else {
		prev_ptr = ptr;
		ptr = RAM_FAT[ptr];
		
		while (ptr != 255) {
			prev_ptr = ptr;
			ptr = RAM_FAT[ptr];
		}
		
		// make previous last sector point to new last sector
		RAM_FAT[prev_ptr] = n;
	}
}


// eDisk_WriteSector
// input: pointer to a 512-byte data buffer in RAM buf[512],
//        sector logical address n
// output: 0 if no error, 1 if error
// use the Flash_Write function
uint8_t eDisk_WriteSector(uint8_t buf[512], uint8_t n){
	LED_Red();
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
		little_endian_val |=  buf[4 * i + 0] & 0x000000FF;
		// byte 1
		little_endian_val |= (buf[4 * i + 1] << 8) & 0x0000FF00;
		// byte 2
		little_endian_val |= (buf[4 * i + 2] << 16) & 0x00FF0000;
		// byte 3 in MSB (highest address)
		little_endian_val |= (buf[4 * i + 3] << 24) & 0xFF000000;
		
		// write value to disk and check if an error occurred
		retVal |= Flash_Write(physical_address, little_endian_val);
		
		// increment physical address to next 32-bit word
		physical_address += 4;
	}
	
	LED_Green();
	return retVal;
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
	for(int i = 0; i < location ; i++){
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
	LED_Red();
  uint32_t address;
  address = 0x00020000; // start of disk
  while( address <= 0x00040000){
    Flash_Erase(address); // erase 1k block
    address = address + 1024;
  }
	LED_Green();
}

//******** OS_File_Flush************* 
// Update working buffers onto the disk 
// Power can be removed after calling flush 
// Inputs: none 
// Outputs: 0 if success 
// Errors: 255 on disk write failure 
uint8_t OS_File_Flush(void){
	//storing data from 254 into buffer
	uint32_t lastDataofDisk = 0x3FC00;
	uint32_t* lastRowData = (uint32_t*)lastDataofDisk;
	uint32_t buf[64];
	for(int i = 0; i < 64; i++){
		buf[i] = *(lastRowData+(i*4));
	}
	//erasing last 2 sectors 254 and 255
	Flash_Erase(0x3FC00);
	//writing old data back into 254
	//lastRowData = (uint32_t*)lastDataofDisk;
	for(int i = 0; i<64; i++){
		Flash_Write(lastDataofDisk+(i*4), buf[i]);
	}
	// writing directory and FAT into 255
  uint32_t endOfDisk = 0x3FE00; 
	uint32_t* sectorWriteStart = (uint32_t*) endOfDisk;
	for(int i = 0; i<64; i++){
		uint32_t data = 0;
		data |= RAM_Directory[i*4] & 0x000000FF;
		data |= (RAM_Directory[(i*4)+1] << 8) & 0x0000FF00;
		data |= (RAM_Directory[(i*4)+2] << 16) & 0x00FF0000;
		data |= (RAM_Directory[(i*4)+3] << 24) & 0xFF000000;
		Flash_Write(endOfDisk+(i*4), data);
	}
	for(int i = 0; i<64; i++){
		uint32_t data = 0;
		data |= RAM_FAT[i*4] & 0x000000FF;
		data |= (RAM_FAT[(i*4)+1] << 8) & 0x0000FF00;
		data |= (RAM_FAT[(i*4)+2] << 16) & 0x00FF0000;
		data |= (RAM_FAT[(i*4)+3] << 24) & 0xFF000000;
		Flash_Write(endOfDisk+(i*4)+256, data);
	}
	return 0;
}


