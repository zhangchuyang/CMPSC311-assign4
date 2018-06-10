////////////////////////////////////////////////////////////////////////////////
//
//  File          : hdd_client.c
//  Description   : This is the client side of the CRUD communication protocol.
//
//   Author       : Patrick McDaniel
//  Last Modified : Thu Oct 30 06:59:59 EDT 2014
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <hdd_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_driver.h>


int hdd_network_shutdown = 0;		//shut down
unsigned char *hdd_network_address = NULL;	//address of the network server
unsigned short hdd_network_port = 0;	//Port of the network server
int sockfd = -1;	//initialized to -1



////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_client_operation
// Description  : This the client operation that sends a request to the CRUD
//                server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : cmd - the request opcode for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed
HddBitResp hdd_client_operation(HddBitCmd cmd, void *buf) {
	uint8_t op, flag;
	op = (uint8_t) ((cmd >> 62) & 0x3);	//op
	flag = (uint8_t) ((cmd >> 33) & 0x7);	//flag
    

    struct sockaddr_in caddr;
    HddBitResp responseValue;

    
    if(flag == HDD_INIT){    	// connect to the server if HDD_INIT
        caddr.sin_family = AF_INET;
        caddr.sin_port = htons(HDD_DEFAULT_PORT);
        if(inet_aton(HDD_DEFAULT_IP, &(caddr.sin_addr)) == 0){
            return(-1);
        }

        else{	//create socket 
		sockfd = socket(PF_INET, SOCK_STREAM, 0);
		if(sockfd == -1){	//check during the creation
		    printf("failed when create socket [%s]\n", strerror(errno));
		    return(-1);
		}

		if(connect(sockfd, (const struct sockaddr *)&caddr, sizeof(struct sockaddr)) == -1){	//check if the connection is correct
		    printf("failed when connect socket [%s]\n", strerror(errno));
		    return(-1);
		}
	  }			 
     }

    int request_length = sizeof(HddBitCmd);
    int request_write;
    HddBitCmd *network_request = malloc(request_length);
    int buffer_write;
    int buffer_length = (cmd >> 36) & 0x3ffffff;


    *network_request = htonll64(cmd);

    request_write = write(sockfd, network_request, request_length);

    while(request_write < request_length){	//when the write is less then the request length
        request_write += write(sockfd, &((char *)network_request)[request_write], request_length - request_write);

    }
    free(network_request);		//free the buffer

    // check if needs to send buffer as well for create block and write block
    if(op == HDD_BLOCK_CREATE || op == HDD_BLOCK_OVERWRITE){
        if(flag == HDD_NULL_FLAG || flag == HDD_META_BLOCK){
            buffer_write = write(sockfd, buf, buffer_length);
            while(buffer_write < buffer_length){
                buffer_write += write(sockfd, &((char *)buf)[buffer_write], buffer_length - buffer_write);
            }
        }
    }
    
    HddBitResp host_response;
    int response_length = HDD_NET_HEADER_SIZE, response_read;
    HddBitResp *response = malloc(response_length);
    int buffer_read;
    int response_op;

    response_read = read(sockfd, response, response_length);
    while(response_read < response_length){		////when the read is less then the request length
        response_read += read(sockfd, &((char *)response)[response_read], response_length - response_read);

    }
    
    host_response = ntohll64(*response);
    free(response);

    response_op = ((host_response >> 62) & 0x3);
    buffer_length = ((host_response >> 36) & 0x3ffffff);


    if(response_op == HDD_BLOCK_READ){		    // check if needs to receive buffer as well
        buffer_read = read(sockfd, buf, buffer_length);
        while(buffer_read < buffer_length){
            buffer_read += read(sockfd, &((char *)buf)[buffer_read], buffer_length - buffer_read);
        }
    }

    responseValue = host_response;

    if(flag == HDD_SAVE_AND_CLOSE){		//check the flag to save and close
        close(sockfd);		//close the socket
        sockfd = -1;
    }

    return responseValue;

}
