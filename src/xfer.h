/*
 *  xfer.h
 *
 *  Header for p2p-relay based file tranfer
 *
 *  Copyright (C) 2009-2013  Minsuk Lee, Hansung University
 *
 *  2009-12-27  Created
 *  2009-01-21  Read
 *  2009-01-25  Enhance Error Processing by Minsuk Lee
 */ 

// Download Port Number for TCP/IP connection

#define DOWNLOAD_PORT   80      // Firewall opens http channel

// File Transfer protocol ---------------------------------------------

#define MAX_FILE_SIZE           (2 * 1024 * 1024)   // Limited by Nintendo DSL
#define MAGIC_LEN               16
#define MAGIC_DOWNLOAD_STRING   "(&^@%=DALL=}`~+)"
#define MAGIC_CLEANUP_STRING    ")+~`}=LLAD=%@^&("

#define MAX_FILE_LENGTH_LEN     16
#define MAX_FILE_NAME_LEN       32

#define RESV_LEN                8
struct header {
    char filename[MAX_FILE_NAME_LEN + 4];   // MAX 32 byte ASCIZ file name
    char flash;                             // 'F' if write after download, otherwise 'X'
    char run;                               // 'R' if run after download, otherwise 'X'
    char debug;                             // 'D' if run with debug, otherwise 'X'
    char zero;                              // set 0
    char filelength[MAX_FILE_LENGTH_LEN];   // ASCIZ file length
    char reserved[RESV_LEN];                // Reserved
};
#define HEADER_SIZE sizeof(struct header)

#define RESP_CODE_LEN   4
#define RESP_STRING_LEN 28
struct response {
    char code[RESP_CODE_LEN];       // 3 Byte Reponse Code and Blank
    char string[RESP_STRING_LEN];   // Response String (ASCIZ)
};
#define RESPONSE_SIZE sizeof(struct response)

// Response Code definition
// "000" : No Error
// "001" : No Error, with Some Comments as follows
// "100" ~ "899" : Error and Cause
// "999" : Internal Error

//-Protocol Description

// 1. HOST -> NDS  : MAGIC DOWNLOAD STRING
// 2. HOST -> NDS  : FILE HEADER
// 3. NDS  -> HOST : OK
// 4. HOST -> NDS  : File Data (in Bytes)
// 5. HOST -> NDS  : CHECKSUM
// 6. NDS  -> HOST : OK
// 7. HOST -> NDS  : MAGIC CLEANUP STRING

