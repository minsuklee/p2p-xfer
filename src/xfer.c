 /*
  *  xfer.h
  *
  *  p2p-relay based file tranfer
  *
  *  Copyright (C) 2009-2013  Minsuk Lee, Hansung University
  *
  *  2009-12-27  Created
  *  2009-01-21  Read
  *  2013-07-10  Windows Client Only
  */ 

#include <winsock2.h>
#include <windows.h>
#include <stdlib.h>
#include <conio.h>
#include <stdio.h>
#include <io.h>

#include "xfer.h"

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE    4096

static SOCKET data_socket;
static unsigned char    BUF[BUF_SIZE];
static struct header    FHeader;
static struct response  FResp;

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
disconnect_nds()
{
    closesocket(data_socket);
    WSACleanup();
}

int
connect_nds(char *target, int port, char *userid)
{
    SOCKADDR_IN nds_sin;
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR) {
        fprintf(stderr, "Error at WSAStartup()\n");
        return -1;
    }

    if ((data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
        fprintf(stderr, "Error at socket(): %ld\n", WSAGetLastError());
        return -1;
    }

    nds_sin.sin_family = AF_INET;
    nds_sin.sin_addr.s_addr = inet_addr(target);
    nds_sin.sin_port = htons(port);

    if (connect(data_socket, (SOCKADDR*)&nds_sin, sizeof(nds_sin)) == SOCKET_ERROR) {
        printf("Failed to connect Nintendo thru %s:%d errorcode:%d.\n", target, port, WSAGetLastError());
        closesocket(data_socket);
        return -1;
    }
    sprintf(BUF, "D %s\n", userid);
    send_data(BUF, strlen(BUF));
    printf("Connection Made to %s:%d\n", target, port);
    return 0;
}

int
main(int argc, char* argv[])
{
    FILE *f2send;
    char *fname;

    int port;     // TCP port number
    int total_sent, size, ret, i, checksum;

    printf("Downloader (to Nintendo)\n(c) Copyright 2013, ");
    printf("by Minsuk Lee (minsuk@hansung.ac.kr)\n");

	if ((argc != 4) && (argc != 5)) {
usage:  fprintf(stderr, "usage: %s FILENAME userid ip_address [port]\n", argv[0]);
        return -1;
    }

    if (argc == 5) {
        if (sscanf(argv[4], "%d", &port) != 1) {
            fprintf(stderr, "Invalid port number\n");
            goto usage;
        }
    } else
        port = DOWNLOAD_PORT;

    fname = strrchr(argv[1], '\\');
    if (fname) {
w_path: fname++;
        if (strlen(fname) == 0) {
            fprintf(stderr, "No File Specified\n");
            goto usage;
        }
        strcpy(FHeader.filename, fname);
    } else {
        fname = strrchr(argv[1], '/');
        if (fname)
            goto w_path;
        strcpy(FHeader.filename, argv[1]);
    }

    if ((f2send = fopen(argv[1], "r+b")) == NULL) {
        perror(argv[1]);
        return -1;
    }
    if (_filelength(_fileno(f2send)) > MAX_FILE_SIZE) {
        fprintf(stderr, "File size too big : MAX = %dBytes\n", MAX_FILE_SIZE);
        goto leave0;
    }
    
    sprintf(FHeader.filelength, "%d", _filelength(_fileno(f2send)));

    ret = -1;   // if return in error clause, ret = -1

    if (connect_nds(argv[3], port, argv[2]) < 0) {
        goto leave0;
    }
    if (send_data(MAGIC_DOWNLOAD_STRING, MAGIC_LEN) != MAGIC_LEN) {
        printf("Failed to send Magic String\n");
        goto leave1;
    }
    printf("Magic Download Code Sent\n");

    if (send_data((unsigned char *)&FHeader, sizeof(FHeader)) != sizeof(FHeader)) {
        printf("Failed to send File Header\n");
        goto leave1;
    }
    printf("Header Sent\n");

    if (recv_data((unsigned char *)&FResp, sizeof(FResp)) != sizeof(FResp)) {
        printf("Failed to Recv Reponse\n");
        goto leave1;
    }
    if (strncmp(FResp.code, "000", 3)) {
        FResp.code[3] = 0;
        printf("Abort: %s: %s\n", FResp.code, FResp.string);
        goto leave1;
    }

    printf("Start Send Data\n");
    total_sent = checksum = 0;
    while (!feof(f2send)) {
        size = fread(BUF, 1, BUF_SIZE, f2send);
        if (!size)
            break;
        for (i = 0; i < size; i++)
            checksum += BUF[i];
        if (send_data(BUF, size) != size) {
            fprintf(stderr, "send error\n");
            goto leave1;
        }
        total_sent += size;
        printf("\b\b\b\b\b\b\b\b\b\\b\b\b\b\b\b\b\b\b\b%dB Sent", total_sent);
        fflush(stdout);
    }
    printf("\n");

    sprintf(BUF, "%d", checksum);
    if (send_data(BUF, MAX_FILE_LENGTH_LEN) != MAX_FILE_LENGTH_LEN) {
        printf("Failed to send Checksum\n");
        goto leave1;
    }
    printf("Checksum Sent\n");

    if (recv_data((unsigned char *)&FResp, sizeof(FResp)) != sizeof(FResp)) {
        printf("Failed to Recv Reponse\n");
        goto leave1;
    }
    if (strncmp(FResp.code, "000", 3)) {
        FResp.code[3] = 0;
        printf("Abort: %s: %s\n", FResp.code, FResp.string);
        goto leave1;
    }

    if (send_data(MAGIC_CLEANUP_STRING, MAGIC_LEN) != MAGIC_LEN) {
        printf("Failed to send Magic String\n");
        goto leave1;
    }
    printf("Magic Cleanup Code Sent\n");

    printf("Transfer file: '%s' (%dB) Done!\n", FHeader.filename, _filelength(_fileno(f2send)));
    ret = 0;

leave1:
    disconnect_nds();
leave0:
    fclose(f2send);
    return ret;
}
