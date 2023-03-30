//CMPSC 311 SP22
//LAB 3

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "mdadm.h"
#include "cache.h"
#include "jbod.h"
#include "net.h"

int mount = 0; //0 representes unmounted, 1 represents mounted
uint32_t operation; //Represents the operation sent to the driver
uint8_t *tempBuf; //Temporary buffer of size 256 bytes to read blocks
uint8_t *cache_buf;

int mdadm_mount(void) {

        //0 out the operation integer
        operation = 0;

        //Check to see if already mounted
        if (mount == 1) {
                return -1;
        }
        
        tempBuf = malloc(256);
        cache_buf = malloc(256);

        //Send operation to driver
        assert(operation == 0);
        jbod_client_operation(operation, NULL);

        //Set mount status to true
        mount = 1;
  return 1;
}

int mdadm_unmount(void) {

        // configure operation
        operation = 1;
        operation = operation << 26;

        // check to see if already unmounted
        if (mount == 0) {
                return -1;
        }
        
        free(tempBuf);
        free(cache_buf);

        jbod_client_operation(operation, NULL);

        mount = 0; //Set mount status to false
  return 1;
}

int validate_parameters(uint32_t addr, uint32_t len, const uint8_t *buf) {

	if (len-addr == 0 && buf == NULL) {
                return len;
        }

        //Check mount status
        if (mount == 0) {
                return -1;
        }

        //Check if address in in range
        if (addr > 1048576) {
                return -1;
        }

        //Check to make sure len is in bounds
        if (len > 1024) {
                return -1;
        }

        //Check to make sure the length is in the address space
        if (addr+len > 1048576) {
                return -1;
        }

        //Check for a non-zero length and a NULL pointer
        if (len > 0 && buf == NULL) {
                return -1;
        }
        
        return 0;

}

//Reads one block
void read_block(uint32_t addr, uint32_t len, uint8_t *buf, int iteration, int *startDisk, int *startBlock, int *readLenRemaining, int *diskCounter_read, int *currentBlock_read, int *blockset_read, int *offset_read) {

	int cache = 0;
    
	//Configure operation for SEEK_TO_DISK for read
        if (*startBlock + *currentBlock_read == 256) {
        	*startDisk += 1;
        	operation = 2;
        	operation = operation << 4;
        	operation = operation + *startDisk;
        	operation = operation << 22;
        	*diskCounter_read += 1;
        	*startBlock = 0;
        	*currentBlock_read = 0;
        	jbod_client_operation(operation, NULL);
        }
        else {
    		operation = 2;
    		operation = operation << 4;
    		operation = operation + *startDisk;
    		operation = operation << 22;
    		jbod_client_operation(operation, NULL);
    	}
    	
    	//If in cache, read to that
    	if (cache_lookup(*startDisk, *startBlock+iteration, tempBuf) == 1) {
    		cache = 1;
    	}
    	
    	//Blockset is a variable that defines the location (out of 256 bytes) within the start block being read at which the read operation is starting
    	if (iteration == 0) {
		*blockset_read = addr - (*startDisk*65536) - (256*(*startBlock));
        	*offset_read = 256 - *blockset_read;
        }
        //Only read from JBOD if we didn't find the block in cache
	if (cache == 0) {
    //Reconfigure operation for SEEK_TO_BLOCK for appropriate block
    operation = 3;
    operation = operation << 26;
    operation = operation + *startBlock + *currentBlock_read;
    jbod_client_operation(operation, NULL);

    //Reconfigure operation for JBOD_READ_BLOCK
    operation = 4;
    operation = operation << 26;
    
    //Perform the read
    jbod_client_operation(operation, tempBuf);
    }
    
    //Copy read data from the temporary buffer to the real buffer
    if (iteration == 0) {
    	if (*blockset_read + len >= 256) {
    		memcpy(buf, &tempBuf[*blockset_read], *offset_read);
    		*readLenRemaining = len - *offset_read;
    	}
    	else {
    		memcpy(buf, &tempBuf[*blockset_read], len);
    		*readLenRemaining = 0;
    	}
    }
    else {
    	if (*readLenRemaining > 256) {
    		memcpy(&buf[*offset_read + 256*(iteration-1)], tempBuf, 256);
    	}
    	else {
    		memcpy(&buf[*offset_read + 256*(iteration-1)], tempBuf, *readLenRemaining);
    	}
    	*readLenRemaining -= 256;
    }
    
    *currentBlock_read += 1;
    
}

//Write the contents of buf to the appropriate block
void write_block(uint32_t addr, uint32_t len, const uint8_t *buf, int iteration, int *startDisk, int *startBlock, int *writeLenRemaining, int *diskCounter_write, int *currentBlock, int *blockset_write, int *offset_write) {
        
        //Read block into tempBuf
        mdadm_read(*startBlock*256 + *startDisk*65536 + *currentBlock*256, 256, tempBuf);
        
        //Configure operation for SEEK_TO_DISK for write
        if (*startBlock + *currentBlock == 256) {
        	*startDisk += 1;
        	operation = 2;
        	operation = operation << 4;
        	operation = operation + *startDisk;
        	operation = operation << 22;
        	*diskCounter_write += 1;
        	*startBlock = 0;
        	*currentBlock = 0;
        	jbod_client_operation(operation, NULL);
        }
        else {
    		operation = 2;
    		operation = operation << 4;
    		operation = operation + *startDisk;
    		operation = operation << 22;
    		jbod_client_operation(operation, NULL);
    	}
    	
    	if (iteration == 0) {
    		//Blockset is a variable that defines the location (out of 256 bytes) within the start block being written to at which the write operation is starting
        	*blockset_write = addr - (*startDisk*65536) - (256*(*startBlock));
        	*offset_write = 256-*blockset_write;
        }

    	//Reconfigure operation for SEEK_TO_BLOCK for write
    	operation = 3;
    	operation = operation << 26;
    	operation = operation + *startBlock + *currentBlock;
    	jbod_client_operation(operation, NULL);
    	
    	//Reconfigure operation for write operation
    	operation = 5;
    	operation = operation << 26;

    	//Prepare the temporary buffer for write
    	if (iteration == 0) {
    		if (*blockset_write + len >= 256) {
    			memcpy(&tempBuf[*blockset_write], buf, *offset_write);
    			*writeLenRemaining = len - *offset_write;
    		}
    		else {
    			memcpy(&tempBuf[*blockset_write], buf, len);
    			*writeLenRemaining = 0;
    		}
    	}
    	else {
    		if (*writeLenRemaining > 256) {
    			memcpy(tempBuf, &buf[*offset_write + (256*(iteration-1))], 256);
    			*writeLenRemaining -= 256;
    		}
    		else {
    			memcpy(tempBuf, &buf[*offset_write + (256*(iteration-1))], *writeLenRemaining);
    			*writeLenRemaining = 0;
    		}
    	}
    	
    	//Insert into cache
        cache_insert(*startDisk, *startBlock+*currentBlock, tempBuf);

    	//Perform the write operation and increment block counter
        jbod_client_operation(operation, tempBuf);   
        
        *currentBlock += 1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
	
        //Ensure the parameters are valid
        if (validate_parameters(addr, len, buf) == -1) {
        	return validate_parameters(addr, len, buf);
        }
        
        //Assign crucial parameters such as start/end disk/block numbers
        int startDisk = (int) addr / 65536;
        int endDisk = (int) (addr + len) / 65536;
        if ((addr + len) % 65536 == 0) {
        	endDisk -= 1;
        }
        int startBlock = (int) (addr - (65536 * startDisk)) / 256;
        int endBlock = (int) ((addr + len) - (65536 * startDisk)) / 256;
        if (((addr + len) - (65536 * startDisk)) % 256 == 0) {
        	endBlock -= 1;
        }
        int blockNum = endBlock - (startBlock-1);
	int readLenRemaining = len;
	int diskCounter_read = 1;
        int currentBlock_read = 0;
        int blockset_read = 0;
        int offset_read = 0;
        
        
        //Call the read operation block by block
        for (int i=0; i < blockNum; i++) {
        	read_block(addr, len, buf, i, &startDisk, &startBlock, &readLenRemaining, &diskCounter_read, &currentBlock_read, &blockset_read, &offset_read);
        }
  return len;
}



int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {

	//Ensure the parameters are valid
        if (validate_parameters(addr, len, buf) == -1 || validate_parameters(addr, len, buf) == len) {
        	return validate_parameters(addr, len, buf);
        }
        
        //Assign starting parameters
        int startDisk = (int) addr / 65536;
        int endDisk = (int) (addr + len) / 65536;
        if ((addr + len) % 65536 == 0) {
        	endDisk -= 1;
        }
        int startBlock = (int) (addr - (65536 * startDisk)) / 256;
        int endBlock = (int) ((addr + len) - (65536 * startDisk)) / 256;
        if (((addr + len) - (65536 * startDisk)) % 256 == 0) {
        	endBlock -= 1;
        }
        int blockNum = endBlock - (startBlock-1);
	int writeLenRemaining = len;
	int diskCounter_write = 1;
        int currentBlock = 0;
        int blockset_write = 0;
        int offset_write = 0;
        
        //Call the write operation block by block
        for (int i=0; i < blockNum; i++) {	
    		write_block(addr, len, buf, i, &startDisk, &startBlock, &writeLenRemaining, &diskCounter_write, &currentBlock, &blockset_write, &offset_write);	
    	}
        
return len;

}
