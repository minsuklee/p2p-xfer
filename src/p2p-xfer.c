 /*
  *  p2p-xfer.c
  *
  *  p2p-relay based file tranfer
  *
  *  Copyright (C) 2013,  Minsuk Lee (minsuk@hansug.ac.kr)
  *  All rights reserved.
  *  This software is under BSD license. see LICENSE.txt
  *
  *  2009-12-27  Created
  *  2013-07-15  using p2p relay 
  */ 

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
//#include <sys/types.h>
#include <sys/socket.h>
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
//#include <conio.h>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable : 4996)     // disable security warning
#endif

#define VERSION_STR1    "File Transfer thru p2p Relay v0.1\n"
#define VERSION_STR2    "(c) Copyright 2013, "
#define VERSION_STR3    "by Minsuk Lee (minsuk@hansung.ac.kr)\n"

// Protocol Description

// 0. connect to relay
//    send type(T-receiver, C-sender) and user-id, $
//    sender receive 'O' if receiver is read on user-id
// 1. SENDER -> RECEIVER : FILE HEADER (48B)
// 2. RECEIVER -> SENDER : 'O'
// 3. SENDER -> RECEIVER : File Data (XXX Bytes)
// 4. SENDER -> RECEIVER : CHECKSUM (16B)
// 5. RECEIVER -> SENDER : 'O'
// 6. close connection with relay

// File Transfer protocol ---------------------------------------------

#define MAX_FILE_SIZE   (2047 * 1024 * 1024)    // 2GB int type limited

#define MAX_FILE_NAME_LEN       32
#define MAX_FILE_LENGTH_LEN     16
#define CHECKSUM_LEN            16

struct header {
    char filename[MAX_FILE_NAME_LEN];       // MAX 32 byte ASCIZ file name
    char filelength[MAX_FILE_LENGTH_LEN];   // ASCIZ file length
};
struct header FHeader;
#define HEADER_SIZE sizeof(struct header)

#define RESP_CODE_LEN   1
// Response Code definition
// 'O' : OK - No Error
// 'X' : Not OK - Error

#ifdef _WIN32
#else
#define INVALID_SOCKET     (-1)
#define SOCKET_ERROR       (-1)
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr    SOCKADDR;
typedef int SOCKET;
#endif

SOCKET data_socket;

#define BUF_SIZE        2048
unsigned char BUF[BUF_SIZE];
int verbose;

int
send_data(unsigned char *buf, int len)
{
    int size, count = len;

    while (count) {
        if ((size = send(data_socket, buf, count, 0)) <= 0) {
            fprintf(stderr, "TCP Send Error\n");
            return -1;  // Error
        }
        buf += size;
        count -= size;
    }
    return len;
}

int
recv_data(unsigned char *buf, int count)
{
    int size, tread = 0;

    while (count) {
        if ((size = recv(data_socket, buf, count, 0)) < 0) {
            fprintf(stderr, "TCP Recv Error\n");
            return -1;  // Error
        }
        if (size == 0)
            break;
        buf += size;
        count -= size;
        tread += size;
    }
    return tread;
}

void
disconnect()
{
#ifdef _WIN32
    closesocket(data_socket);
    WSACleanup();
#else
    close(data_socket);
#endif
}

int
connect_relay(int sender, char *target, int port, char *userid)
{
#ifdef _WIN32
    WSADATA wsaData;
#endif
    SOCKADDR_IN relay_sin;

#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR) {
        fprintf(stderr, "Error at WSAStartup()\n");
        return -1;
    }
#endif

    if ((data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        perror("socket");
        return -1;
    }

    relay_sin.sin_family = AF_INET;
    relay_sin.sin_addr.s_addr = inet_addr(target);
    relay_sin.sin_port = htons(port);

    if (connect(data_socket, (SOCKADDR*)&relay_sin, sizeof(relay_sin)) == SOCKET_ERROR) {
        printf("Failed to connect p2p_relay %s:%d\n", target, port);
leave:  disconnect();
        return -1;
    }
    if (sender)
        sprintf(BUF, "C%s$", userid); // Connect to server with userid
    else
        sprintf(BUF, "T%s$", userid); // Wait File Transfer Connection

    send_data(BUF, strlen(BUF));
    if (sender) {
        if (recv_data(BUF, RESP_CODE_LEN) != RESP_CODE_LEN) {
            fprintf(stderr, "Relay not responding\n");
            goto leave;
        }
        if (BUF[0] != 'O') {
            fprintf(stderr, "No receiver found\n");
            goto leave;
        }
    }
    if (verbose)
        printf("Connection Made to %s:%d for %s file\n", target, port,
            sender ? "Send" : "Receive");
    return 0;
}

int
main(int argc, char* argv[])
{
    FILE *fp = NULL;
    char *fname;
    int filesize;

    int sender, port;     // TCP port number
    int total_sent, size, ret, i, checksum;

// Argument processing is not for Nintendo DSL

    if ((argc != 5) && (argc != 6)) {
        char *argv0;
usage:  fprintf(stderr, "%s%s%s", VERSION_STR1, VERSION_STR2, VERSION_STR3);
        argv0 = strrchr(argv[0], '\\');
        if (argv0 == NULL) {
            argv0 = strrchr(argv[0], '/');	// linux style
            if (argv0 == NULL) {
                argv0 = argv[0];
                goto putit;
            }
        }
        argv0++;
putit:  fprintf(stderr, "usage: %s %s userid ip_address port [v]\n",
            argv0, (toupper(*argv0) == 'S') ? "filenae" : "-");
        return 1;
    }

    sender = (toupper(argv[1][0]) != '-');
    if (sscanf(argv[4], "%d", &port) != 1) {
        fprintf(stderr, "Invalid port number\n");
        goto usage;
    }

    if (argc == 6) {
        if ((strlen(argv[5]) == 1) && (toupper(argv[5][0]) == 'V'))
            verbose = 1;
        else
            goto usage;
    }
    if (verbose)
        printf("%s%s%s", VERSION_STR1, VERSION_STR2, VERSION_STR3);

    if (sender) {
        // Sender try to open File and get length
        struct stat st_buf;

        fname = strrchr(argv[1], '\\');
        if (fname) {
w_path:     fname++;
            if (strlen(fname) == 0) {
                fprintf(stderr, "No File Specified\n");
                goto usage;
            }
            strcpy(FHeader.filename, fname);
        } else {
            fname = strrchr(argv[1], '/');	// linux style
            if (fname)
                goto w_path;
            strcpy(FHeader.filename, argv[1]);
        }

        if ((fp = fopen(argv[1], "r+b")) == NULL) {
            perror(argv[1]);
            return 1;
        }
        fstat(fileno(fp), &st_buf);
        filesize = st_buf.st_size;

        if (filesize > MAX_FILE_SIZE) {
            fprintf(stderr, "File size too big : MAX = %dBytes\n", MAX_FILE_SIZE);
            goto leave0;
        }
        sprintf(FHeader.filelength, "%d", filesize);
    }
    ret = 1;   // if return in error clause, ret = 1

    if (connect_relay(sender, argv[3], port, argv[2]) < 0) {
        goto leave0;
    }

    if (sender) {
        // Send Header, get 'O', Send Data, Checkum, get 'O'
        if (send_data((unsigned char *)&FHeader, HEADER_SIZE) != HEADER_SIZE) {
            fprintf(stderr, "Failed to send File Header\n");
            goto leave1;
        }
        if (verbose)
            printf("File Transfer Header Sent\n");

        if (recv_data(BUF, RESP_CODE_LEN) != RESP_CODE_LEN) {
            fprintf(stderr, "Failed to header reveice reponse\n");
            goto leave1;
        }
        if (BUF[0] != 'O') {
            fprintf(stderr, "Abort: receiver reject transfer\n");
            goto leave1;
        }
        if (verbose)
            printf("Start Sending Data\n");
        total_sent = 0;
        checksum = 0;
        while (total_sent < filesize) {
            size = fread(BUF, 1, BUF_SIZE, fp);
            if (!size)
                break;
            for (i = 0; i < size; i++)
                checksum += BUF[i];
            if (send_data(BUF, size) != size) {
                fprintf(stderr, "Data send error\n");
                goto leave1;
            }
            total_sent += size;
            if (verbose) {
                printf("\r%dB Sent", total_sent);
                fflush(stdout);
            }
        }
        if (verbose)
            printf("\n");

        sprintf(BUF, "%d", checksum);
        if (send_data(BUF, CHECKSUM_LEN) != CHECKSUM_LEN) {
            fprintf(stderr, "Failed to send Checksum\n");
            goto leave1;
        }
        if (verbose)
            printf("Checksum Sent\n");

        if (recv_data(BUF, RESP_CODE_LEN) != RESP_CODE_LEN) {
            fprintf(stderr, "Failed to Recv Reponse\n");
            goto leave1;
        }
        if (BUF[0] != 'O') {
            fprintf(stderr, "Checksum Error\n");
            goto leave1;
        }
        if (verbose)
            printf("Transfer file: '%s' (%sB) Done!\n", FHeader.filename, FHeader.filelength);
    } else { // I'm receiver
        // receive Header, put 'O', receive Data, Checkum, put 'O'
        if (recv_data((unsigned char *)&FHeader, HEADER_SIZE) != HEADER_SIZE) {
            printf("Failed to Receive File Transfer Header\n");
            goto leave1;
        }
        if (verbose)
            printf("File Transfer Header Received\n");

        if (sscanf(FHeader.filelength, "%d", &filesize) != 1) {
            fprintf(stderr, "Invalid File Length format\n");
            goto leave2;
        }
        if ((filesize <= 0) || (filesize > MAX_FILE_SIZE)) {
            fprintf(stderr, "Invalid File Length : %dB\n", filesize);
            goto leave2;
        }
        if (verbose)
            printf("File: '%s':%d Bytes\n", FHeader.filename, filesize);

        if ((fp = fopen(FHeader.filename, "wb")) == NULL) {
            fprintf(stderr, "File Open Error\n");
            goto leave2;
        }
        send_data("O", RESP_CODE_LEN);
    
        size = filesize;
        checksum = 0;
        while (size > 0) {
            if ((ret = recv_data(BUF, (size < BUF_SIZE) ? size : BUF_SIZE)) <= 0) {
                fprintf(stderr, "Data Receive Error\n");
                goto leave1;
            }
            size -= ret;
            if (ret != fwrite(BUF, 1, ret, fp)) {
                fprintf(stderr, "download: File Write Error\n");
                goto leave2;
            }
            for (i = 0; i < ret; i++)
                checksum += BUF[i];
            if (verbose) {
                printf("\r%dB Left ", size);
                fflush(stdout);
            }
        }
        if (verbose) {
            printf("\rFile Data Received\n");
        }

        if (ret = recv_data(BUF, CHECKSUM_LEN) != CHECKSUM_LEN)
            goto sum_e;
        if (sscanf((char *)BUF, "%d", &ret) != 1)
            goto sum_e;
        if (ret != checksum) {
sum_e:      fprintf(stderr, "Checksum Error\n");
leave2:     send_data("X", RESP_CODE_LEN);
            goto leave1;
        }
        send_data("O", RESP_CODE_LEN);
        if (verbose)
            printf("Checksum OK .. Done\n");
    }
    ret = 0;
leave1:
    disconnect();
leave0:
    if (fp)
        fclose(fp);
    return ret;
}
