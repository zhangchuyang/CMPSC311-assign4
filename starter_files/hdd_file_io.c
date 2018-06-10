////////////////////////////////////////////////////////////////////////////////
//
//  File           : hdd_file_io.c
//  Description    : Extend your device driver to communicate with the HDD over a network
//  Author         : Chuyang Zhang
//  Last Modified  : 2017/12/1
//

// Includes
#include <malloc.h>
#include <string.h>

// Project Includes
#include <hdd_file_io.h>
#include <hdd_driver.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_network.h>

// Defines
#define CIO_UNIT_TEST_MAX_WRITE_SIZE 1024
#define HDD_IO_UNIT_TEST_ITERATIONS 10240


// Type for UNIT test interface
typedef enum {
CIO_UNIT_TEST_READ   = 0,
CIO_UNIT_TEST_WRITE  = 1,
CIO_UNIT_TEST_APPEND = 2,
CIO_UNIT_TEST_SEEK   = 3,
} HDD_UNIT_TEST_TYPE;

char *cio_utest_buffer = NULL;  // Unit test buffer


typedef struct hdd_file{
	int32_t cp;	//current position
	uint32_t blockId;	//block id
	uint64_t blockSize;	//block size
	char fileName[MAX_FILENAME_LENGTH];	//file name
	int status;	//file status

} fileData;

int fh;		//file handler
int init = 0;	//initialization set to 0
int number = 0;	//set the file number
HddBitCmd command;
fileData file[MAX_HDD_FILEDESCR];

// function that helps to accomplish the tasks
///////////////////////////////////////////////////////////////////////////////
HddBitCmd setCmd(uint8_t op, uint32_t block_size, uint8_t flag, uint8_t R, uint32_t block_id){

command = (uint64_t) 0; 	// initialize all bits of the comment to 0
command = command | ((uint64_t) op << 62); 	// op
command = command | ((uint64_t) block_size << 36); 	// block size
command = command | ((uint64_t) flag << 33);	 // flag
command = command | ((uint64_t) R << 32); 	// r
command = command | ((int32_t) block_id); 	// block id
return command;

}

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_format
// Description  : sends a format request to the device delete all blocks
// Inputs       : void
// Outputs      : 0 on success or -1 on failure
//
uint16_t hdd_format(void) {
	HddBitCmd cmd1, fcmd, cmetacmd;
	HddBitResp resp1, fResp, cmetaResp;
	if(init == 0){		//check the initalization
		cmd1 = setCmd(HDD_DEVICE, 0, HDD_INIT, 0, 0);
		resp1 = hdd_client_operation(cmd1, NULL);
		resp1 = (resp1 >> 32) & 0x1;
		if(resp1){
			printf("initialize the block incorrectly\n");
			return -1;
		}
		else{
	
			init = 1;
		}
	}
	
		fcmd = setCmd(HDD_DEVICE, 0, HDD_FORMAT, 0, 0);
		fResp = hdd_client_operation(fcmd, NULL);
		fResp = (fResp >> 32) & 0x1;		//if condition
		if(fResp){	//check the format
			printf("format debug1\n");
			return -1;
		}	
		int j = 0;
		while (j < MAX_HDD_FILEDESCR){		//initialize by loop
			file[j].cp = 0;
			file[j].blockId = 0;
			file[j].blockSize = 0;
			memset(file[j].fileName, '\0', sizeof(file[j].fileName));
			file[j].status = 0; 
			j++;
		}


		cmetacmd = setCmd(HDD_BLOCK_CREATE, sizeof(fileData) * MAX_HDD_FILEDESCR, HDD_META_BLOCK, 0, 0);
		cmetaResp = hdd_client_operation(cmetacmd, file);
		cmetaResp = (cmetaResp >> 32) & 0x1;
		if(cmetaResp){	//check if meta block is created correctly
			printf("Meta block created incorrectly\n");	//for debug
			return -1; 
		}
		else{		
			return 0;
		}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_mount
// Description  : read from the meta block to populate global data.
// Inputs       : void
// Outputs      : return 0 on success and -1 on failure
//
uint16_t hdd_mount(void) {
	HddBitCmd cmd1, rmetacmd;
	HddBitResp resp1, rmetaResp;
	if(init == 0){		// check the  initialization
		cmd1 = setCmd(HDD_DEVICE, 0, HDD_INIT, 0, 0);
		resp1 = hdd_client_operation(cmd1, NULL);
		resp1 = (resp1 >> 32) & 0x1;
		if(resp1){	//check the block if initalized correctly
			printf("initialize the block incorrectly\n");		
			return -1;
		}
		else{
			init = 1;	//if correct, set the init to 1
		}
	}

	rmetacmd = setCmd(HDD_BLOCK_READ, sizeof(fileData) * MAX_HDD_FILEDESCR, HDD_META_BLOCK, 0, 0);
	rmetaResp = hdd_client_operation(rmetacmd, file);
	rmetaResp = (rmetaResp >> 32) & 0x1;
	if(rmetaResp){		// check if meta block is correct
		printf("meta block read incorrectly\n");
		return -1;
	}
	else{
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_unmount(void)
// Description  : read from the meta block to populate your global data.
//
// Inputs       : void
// Outputs      : return 0 on success and -1 on failure
//
uint16_t hdd_unmount(void) {
	HddBitCmd metacmd, sccmd;
	HddBitResp metaResp, scResp;
	// save tables to meta block request
	metacmd = setCmd(HDD_BLOCK_OVERWRITE, sizeof(fileData) * MAX_HDD_FILEDESCR, HDD_META_BLOCK, 0, 0);
	metaResp = hdd_client_operation(metacmd, file);
	metaResp = (metaResp >> 32) & 0x1;
	if(metaResp){	//check if the table saves correctly
		printf("incorrectly saved the meta block\n");
		return -1;
	}
	else{
		sccmd = setCmd(HDD_DEVICE, 0, HDD_SAVE_AND_CLOSE, 0, 0);
		scResp = hdd_client_operation(sccmd, NULL);
		if((scResp >> 32) & 0x1){	//check if save and close correctly
			printf("incorrectly save and close the mata block\n");	//for debug
			return -1;
		}
		else{
			return 0; 	//if correctly
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_open(char*)
// Description  : hdd_open will open a file and return an integer file handle.
//                and it return -1 on failure and integer on sucess.
//
// Inputs       : path  - the given filename
// Outputs      : fileHandle    - the unique integer that needed to be returned.
//
int16_t hdd_open(char *path) {
	HddBitCmd cmd1;
	HddBitResp resp1;
	int i; 
	if (path == NULL){
		printf("empty file\n");
        	return -1;
	}
	else if(path != NULL){	//check if the path is empty
		if(init == 0){		//check if it's init
			cmd1 = setCmd(HDD_DEVICE, 0, HDD_INIT, 0, 0);
			resp1 = hdd_client_operation(cmd1, NULL);
			if((resp1 >> 32) & 0x1){
				printf("initialize the block incorrectly\n");	//for debug
				return -1; 
			}
			init = 1; 	//initalized to 1 when success
		}
		if(strlen(path) > MAX_FILENAME_LENGTH){		//check the path is valid
			printf("The requested path is incorrect\n");	//for debug
			return -1;
		}

		for(i = 0; i < MAX_HDD_FILEDESCR; i++){		//find the filename in the table
			if(strcmp(file[i].fileName, path) == 0){	//check if it's opened
				if(file[i].status == 1){
					printf("File already opened\n");
					return -1;
				}
				else if(file[i].status != 1){	//when it's not opened
					fh = i;
					file[i].status = 1;
					file[i].cp = 0; 
					return fh;
				}
			}
		}
		int fh = 0; 
		
		while(strcmp(file[fh].fileName, "") != 0){	//when it's empty
			fh++;
			if(fh == MAX_FILENAME_LENGTH){		//when it's full
				printf("Max out. Debug2\n");	//for debug
				return -1;
			}
		}
		number = fh;
		file[fh].blockId = 0;
		file[fh].cp = 0;
		file[fh].blockSize = 0;
		strcpy(file[fh].fileName, path); 
		file[fh].status = 1;
		fh += 1;
		return number;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_close(int16_t)
// Description  : close the file reference by the file handle
//
// Inputs       : fh    -file handle
// Outputs      : 0 sucess     -1 failure
//
int16_t hdd_close(int16_t fh) {

	if(fh > MAX_HDD_FILEDESCR || fh < 0){	// check if file handle is valid
		printf("Invalid file handle\n");		
		return -1;
	}
	if(file[fh].status == 0){		// check if file is closed
		printf("File is closed\n");
		return -1;
	}
	else{
		file[fh].status = 0;
		file[fh].cp = 0;
		return 0;
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_read(int16_t, void *, int32_t)
// Description  : read a count number and places them into buffer
//
// Inputs       : fh    -file handle    data    -the file content that needed to put in
//                count -count number of bytes from the current position
// Outputs      : --1 failure   -number of bytes read sucess
//
int32_t hdd_read(int16_t fh, void * data, int32_t count) {
	HddBitResp rResp;
	HddBitCmd rcmd;

	if(init == 0){		// check if block is initialized
		printf("It is not initialized\n");
		return -1;
	}
	if(file[fh].blockId == 0){	// check if the block to read from is existing
		printf("block id is empty\n");
		return -1;
	}

	if(file[fh].cp + count > file[fh].blockSize){		// read the buffer when it's not over the size
		char *newData;
		newData = (char*)malloc(file[fh].blockSize);
		rcmd = setCmd(HDD_BLOCK_READ, file[fh].blockSize, 0, 0, file[fh].blockId);
		rResp = hdd_client_operation(rcmd, newData);
		rResp = (rResp >> 32) & 0x1;
		if(rResp){		// check if read and send data successful
			printf("read block incorrectly\n");
			return -1;
		}
		else{
			memcpy(data, &newData[file[fh].cp], file[fh].blockSize - file[fh].cp);
			int32_t expectedCount = file[fh].blockSize - file[fh].cp;
			file[fh].cp = file[fh].blockSize;
			free(newData);
			return expectedCount;
		}
	}

	else{		//get more memory for buffer
		char *newData; 
		newData = (char*)malloc(file[fh].blockSize);
		// request read and copy content into the new data buffer
		rcmd = setCmd(HDD_BLOCK_READ, file[fh].blockSize, 0, 0, file[fh].blockId);
		// check if read and send data successful
		rResp = hdd_client_operation(rcmd, newData);
		rResp = (rResp >> 32) & 0x1;
		if(rResp){		// when read the block incorrectly
			printf("Read block incorrectly\n");
			return -1;
		}
		else{		//when correctly
			memcpy(data, &newData[file[fh].cp], count);
			file[fh].cp += count;
			free(newData);
			return count;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_write(int16_t, void *, int 32_t)
// Description  : write a count number of bytes at the current position, and create new blocks when number of bytes are exceeded.
//
// Inputs       : fh    -file handle    data    - the file content that needed to put in
//                count -count number of bytes from the current position
// Outputs      : --1 if failure -number of written read if sucess
//
int32_t hdd_write(int16_t fh, void *data, int32_t count) {
	HddBitCmd ccmd, rcmd, wcmd, ccmd1, dcmd;
	HddBlockID bid, bid1;
	HddBitResp rResp, wResp, dResp;
	if(init == 0){		// check if the block is initialized
		printf("It is not initialized\n");
		return -1;
        }
        // create a block if no block exist
        if(file[fh].blockId == HDD_NO_BLOCK){	// if block is empty
		ccmd = setCmd(HDD_BLOCK_CREATE, count, 0, 0, 0);
		bid = hdd_client_operation(ccmd, data);
                file[fh].cp = count;
                file[fh].blockId = bid; 
                file[fh].blockSize = count;
                file[fh].status = 1;
                return count;
        }
        else{		//when the content size is less than the total size
                if(count + file[fh].cp <= file[fh].blockSize){                       
			char *ori_data;
                        ori_data = (char*)malloc(file[fh].blockSize);              
                        rcmd = setCmd(HDD_BLOCK_READ, file[fh].blockSize, 0, 0, file[fh].blockId);
                        rResp = hdd_client_operation(rcmd, ori_data);
			//rResp = 
                        if((rResp >> 32) & 0x1){		//check if read correctly
                                printf("read bug 5\n");
                                return -1;
                        }
                        	memcpy(&ori_data[file[fh].cp], data, count);
		                wcmd = setCmd(HDD_BLOCK_OVERWRITE, file[fh].blockSize, 0, 0, file[fh].blockId);
		                wResp = hdd_client_operation(wcmd, ori_data);
				wResp = (wResp >> 32) & 0x1;
		                if(wResp){		// check if write successful
		                        printf("write bug6\n");
		                        return -1;
		                }
				else{
				        file[fh].cp = file[fh].cp + count;
				        free(ori_data);
				        return count;
				}

                }
                else{		//when the content size is larger than the total number                 
			char *oldData;
                        char *newData;
			oldData = (char*)malloc(file[fh].blockSize);
                        newData = (char*)malloc(file[fh].cp + count);
			rcmd = setCmd(HDD_BLOCK_READ, file[fh].blockSize, 0, 0, file[fh].blockId);
                        rResp = hdd_client_operation(rcmd, oldData);
			rResp = (rResp >> 32) & 0x1;
                        if(rResp){		//check if read successfully
                                printf("Read bug3\n");
                                return -1;
                        }
			else{
				memcpy(newData, oldData, file[fh].blockSize);
				free(oldData);
		                memcpy(&newData[file[fh].cp], data, count);
		                ccmd1 = setCmd(HDD_BLOCK_CREATE, file[fh].cp + count, 0, 0, 0);
	       			bid1 = hdd_client_operation(ccmd1, newData);
				free(newData);
				dcmd = setCmd(HDD_BLOCK_DELETE, 0, 0, 0, file[fh].blockId);		// delete the command

		                dResp = hdd_client_operation(dcmd, NULL);
				dResp = (dResp >> 32) & 0x1;	// check if read to buffer successful
		                if(dResp){	// check if delete successfully
		                        printf("delete bug4\n");
		                        return -1;
		                }
				else{
				        // update metadata
				        file[fh].cp = file[fh].cp + count;
				        file[fh].blockId = bid1;
					file[fh].blockSize = file[fh].cp;
				        return count;
				}
		        }
		}
		return -1;
	}
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_seek
// Description  : changes the current seek postion of file to loc
//
// Inputs       : file handler, loc
// Outputs      : -1 on failure or 0 on success
//
int32_t hdd_seek(int16_t fh, uint32_t loc) {
        
        if(init == 0){	// check if the block is initialized
		printf("The device is not initialized\n");
		return -1;
	}

	if(fh > MAX_HDD_FILEDESCR || fh < 0){	// check if the file handle is matched
		printf("file handler not correct\n");
		return -1;
	}
	
	if(file[fh].status == 0){	// check if the file is openning 
		printf("File is not opened\n");
		return -1;
	}

        // change current position to loc
        if(loc <= file[fh].blockSize){
                file[fh].cp = loc;
                return 0;
        }
        else{	//return -1 when out of range
                printf("seek out of range.\n");
                return -1;
        }
}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : hddIOUnitTest
// Description  : Perform a test of the HDD IO implementation
//
// Inputs       : None
// Outputs      : 0 if successful or -1 if failure

int hddIOUnitTest(void) {

	// Local variables
	uint8_t ch;
	int16_t fh, i;
	int32_t cio_utest_length, cio_utest_position, count, bytes, expected;
	char *cio_utest_buffer, *tbuf;
	HDD_UNIT_TEST_TYPE cmd;
	char lstr[1024];

	// Setup some operating buffers, zero out the mirrored file contents
	cio_utest_buffer = malloc(HDD_MAX_BLOCK_SIZE);
	tbuf = malloc(HDD_MAX_BLOCK_SIZE);
	memset(cio_utest_buffer, 0x0, HDD_MAX_BLOCK_SIZE);
	cio_utest_length = 0;
	cio_utest_position = 0;

	// Format and mount the file system
	if (hdd_format() || hdd_mount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on format or mount operation.");
		return(-1);
	}

	// Start by opening a file
	fh = hdd_open("temp_file.txt");
	if (fh == -1) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure open operation.");
		return(-1);
	}

	// Now do a bunch of operations
	for (i=0; i<HDD_IO_UNIT_TEST_ITERATIONS; i++) {

		// Pick a random command
		if (cio_utest_length == 0) {
			cmd = CIO_UNIT_TEST_WRITE;
		} else {
			cmd = getRandomValue(CIO_UNIT_TEST_READ, CIO_UNIT_TEST_SEEK);
		}
		logMessage(LOG_INFO_LEVEL, "----------");

		// Execute the command
		switch (cmd) {

		case CIO_UNIT_TEST_READ: // read a random set of data
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d at position %d", count, cio_utest_position);
			bytes = hdd_read(fh, tbuf, count);
			if (bytes == -1) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Read failure.");
				return(-1);
			}

			// Compare to what we expected
			if (cio_utest_position+count > cio_utest_length) {
				expected = cio_utest_length-cio_utest_position;
			} else {
				expected = count;
			}
			if (bytes != expected) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : short/long read of [%d!=%d]", bytes, expected);
				return(-1);
			}
			if ( (bytes > 0) && (memcmp(&cio_utest_buffer[cio_utest_position], tbuf, bytes)) ) {

				bufToString((unsigned char *)tbuf, bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST R: %s", lstr);
				bufToString((unsigned char *)&cio_utest_buffer[cio_utest_position], bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST U: %s", lstr);

				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : read data mismatch (%d)", bytes);
				return(-1);
			}
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d match", bytes);


			// update the position pointer
			cio_utest_position += bytes;
			break;

		case CIO_UNIT_TEST_APPEND: // Append data onto the end of the file
			// Create random block, check to make sure that the write is not too large
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			if (cio_utest_length+count >= HDD_MAX_BLOCK_SIZE) {

				// Log, seek to end of file, create random value
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : append of %d bytes [%x]", count, ch);
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", cio_utest_length);
				if (hdd_seek(fh, cio_utest_length)) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", cio_utest_length);
					return(-1);
				}
				cio_utest_position = cio_utest_length;
				memset(&cio_utest_buffer[cio_utest_position], ch, count);

				// Now write
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes != count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST :append failed [%d].", count);
					return(-1);
				}
				cio_utest_length = cio_utest_position += bytes;
			}
			break;

		case CIO_UNIT_TEST_WRITE: // Write random block to the file
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			// Check to make sure that the write is not too large
			if (cio_utest_length+count < HDD_MAX_BLOCK_SIZE) {
				// Log the write, perform it
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : write of %d bytes [%x]", count, ch);
				memset(&cio_utest_buffer[cio_utest_position], ch, count);
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes!=count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : write failed [%d].", count);
					return(-1);
				}
				cio_utest_position += bytes;
				if (cio_utest_position > cio_utest_length) {
					cio_utest_length = cio_utest_position;
				}
			}
			break;

		case CIO_UNIT_TEST_SEEK:
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", count);
			if (hdd_seek(fh, count)) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", count);
				return(-1);
			}
			cio_utest_position = count;
			break;

		default: // This should never happen
			CMPSC_ASSERT0(0, "HDD_IO_UNIT_TEST : illegal test command.");
			break;

		}

	}

	// Close the files and cleanup buffers, assert on failure
	if (hdd_close(fh)) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure close close.", fh);
		return(-1);
	}
	free(cio_utest_buffer);
	free(tbuf);

	// Format and mount the file system
	if (hdd_unmount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on unmount operation.");
		return(-1);
	}

	// Return successfully
	return(0);
}



