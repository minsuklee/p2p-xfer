/* FILE        : p2p_relay.c
 *
 * Description : p2p gateway to bridge NAT GAP
 *      send/recv stream data thru NAT environment
 *
 *  Copyright (C) 2013,  Minsuk Lee (minsuk@hansug.ac.kr)
 *  All rights reserved.
 *  This software is under BSD license. see LICENSE.txt
 *
 * Created - 2013-06-27 by Minsuk Lee
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <sys/epoll.h>
#include <time.h>

#define ALIVE_TIME	60	/* Seconds */
#define MAX_CHANNEL	100	/* Maximum simultaneous channels */
#define MAX_ID_LEN	20	/* ID Length */

#define BUFSIZE		2048

struct channel {
    int sd;
    int peer;			/* peer channel */
    char UID[MAX_ID_LEN];	/* pseudo non-ZERO Unique ID */
    struct sockaddr_in from;	/* my client */
    int alive_timer;		/* if alive_timer == 0, kill connection */
};
    
struct channel CHANNEL[MAX_CHANNEL];
struct epoll_event events[MAX_CHANNEL];
int event_fd;

int  SetNonblocking(int fd);
void CloseChannel(struct channel *cp);
void DoProcessing(struct channel *cp, uint32_t events);

int
main(int argc, char* argv[])
{
    struct sockaddr_in gateway;
    struct sockaddr_in client;
    int g_socket, his_socket;
    int port, size;
    time_t old_time = 0, new_time;
    struct channel *cp;

    int ret, i, j;

    if (argc != 2) {
usage: fprintf(stderr, "usage: %s port\n", argv[0]);
       return 1;
    }
    ret = sscanf(argv[1], "%d", &port);
    if (ret != 1)
        goto usage;
    printf("P2P Gateway started with port number : %d\n", port);

    if ((event_fd = epoll_create(1)) < 0) {
        perror("epoll_create()");
        return 1;
    }

    for (i = 0, cp = CHANNEL; i < MAX_CHANNEL; i++, cp++) {
        cp->sd = -1;
        cp->peer = -1;
    }
    if ((g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "Error while open socket()\n");
        return 1;
    }
    SetNonblocking(g_socket);

    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl(INADDR_ANY);
    gateway.sin_port = htons(port);
    printf("Listneing %s:%d\n", inet_ntoa(*(struct in_addr *)&(gateway.sin_addr.s_addr)), ntohs(gateway.sin_port));

    ret = 1;
    if (bind(g_socket, (struct sockaddr *)&gateway, sizeof(gateway)) < 0) {
        perror("bind()");
        goto return_1;
    }
    if (listen(g_socket, MAX_CHANNEL) == -1) {
        perror("listen()");
        goto return_1;
    }

    events[0].events = EPOLLIN;
    events[0].data.fd = g_socket;
    if (epoll_ctl(event_fd, EPOLL_CTL_ADD, g_socket, &events[0]) < 0) {
        perror("epoll_ctl(listen-socket)");
        goto return_1;
    }

    while (1) {
        int nfds;

        if ((nfds = epoll_wait(event_fd, events, MAX_CHANNEL, 500)) < 0) {
            perror("epoll_wait()");
            goto return_2;
        }
        printf("%d ", nfds); fflush(stdout);
        for (i = 0; i < nfds; i++) {
            if (events[i].data.fd == g_socket) {
                size = sizeof(client);
                if ((his_socket = accept(g_socket, (struct sockaddr *)&client, &size)) < 0) {
                    perror("accept()");
                    goto return_2;
                }
                printf("connection from : %s:%d\n", inet_ntoa(*(struct in_addr *)&(client.sin_addr.s_addr)), ntohs(client.sin_port));

                // close former channel from the same client
                for (j = 0, cp = CHANNEL; j < MAX_CHANNEL; j++, cp++) {
                    if (cp->sd < 0)
                        continue;
                    if (!memcmp(&cp->from, &client, size))
                        CloseChannel(cp);
                }
                for (j = 0, cp = CHANNEL; j < MAX_CHANNEL; j++, cp++) {
                    if (cp->sd >= 0)
                        continue;
                    cp->sd = his_socket;
                    memcpy(&cp->from, &client, size);
                    printf("Allocated in Channel %d\n", j);
                    SetNonblocking(his_socket);
                    events[0].events = EPOLLIN | EPOLLRDHUP;
                    events[0].data.fd = his_socket;
                    if (epoll_ctl(event_fd, EPOLL_CTL_ADD, his_socket, &events[0]) < 0) {
                        perror("epoll_ctl(accepted-socket)");
                        goto return_2;
                    }
                    cp->alive_timer = ALIVE_TIME;
                    break;
                }
                if (j == MAX_CHANNEL) {
                    printf("No more Channels\n");
                    close(his_socket);
                }
                continue;
            }
            for (j = 0, cp = CHANNEL; j < MAX_CHANNEL; j++, cp++) {
                if (cp->sd != events[i].data.fd)
                    continue;
                DoProcessing(cp, events[i].events);
            }
        }
        time(&new_time);
        if (old_time == new_time)
            continue;
        old_time = new_time;
        for (i = 0, cp = CHANNEL; i < MAX_CHANNEL; i++, cp++) {
            if (cp->sd < 0)
                continue;
            cp->alive_timer--;
            if (cp->alive_timer > 0)
                continue;
            CloseChannel(cp);
        }
    }
    // in normal condition never return here
    ret = 0;
return_2:
    close(event_fd);
   // close all open socket;
return_1:
    close(g_socket);
    return ret;
}

int
SetNonblocking(int fd)
{
    int flags;

#if defined(O_NONBLOCK)
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}   

void
CloseChannel(struct channel *cp)
{
    if (cp->sd < 0) {
        fprintf(stderr, "Something wrong, [%d].sd < 0\n", cp - CHANNEL);
        return;
    }
    epoll_ctl(event_fd, EPOLL_CTL_DEL, cp->sd, NULL);
    close(cp->sd);
    cp->sd = -1;
    printf("close channel %d: %s:%d\n", cp - CHANNEL, inet_ntoa(*(struct in_addr *)&(cp->from.sin_addr.s_addr)), ntohs(cp->from.sin_port));
    if (cp->peer >= 0) {
        CHANNEL[cp->peer].peer = -1;
        CloseChannel(CHANNEL + cp->peer);
    }
    cp->peer = -1;
}


//
// Sequence to use p2p-gate
//    connect to p2p-gate server
//    send "T<id>$" for wating file transfer connection
//    send "D<id>$" for wating debug connection
//    send "C<id>$" for client connection
//

void
DoProcessing(struct channel *cp, uint32_t events)
{
    char buf[BUFSIZE];
    struct channel *pp;
    int rsize, wsize, count;
    int i;

    if (events == EPOLLRDHUP) {
        printf("Connection Closed by client CH:%d\n", cp - CHANNEL);
        goto error;
    }
    if (cp->peer == -1) {
        // This part is for matching two hosts
        if ((rsize = recv(cp->sd, buf, BUFSIZE - 1, 0)) <= 0)
            goto leave;
        for (i = 0; i < rsize; i++) {
            if (buf[i] == '$') {
                buf[i] = 0;
                goto ready;
            }
        }
        printf("(%d)Protocol Error channel:%d no '$' found\nclose connection.\n", rsize, cp - CHANNEL);
        goto error;
ready:  buf[rsize] = 0; printf("'%s'\n", buf);
        for (i = 0; i < BUFSIZE; i++) {
            if (buf[i] == '$') {
                buf[i] = 0;
                break;
            }
        }

        switch (buf[0]) {
          case 'T' :
            cp->alive_timer = ALIVE_TIME;
            strncpy(cp->UID, buf, MAX_ID_LEN);
            break;
          case 'C' :
            for (i = 0, pp = CHANNEL; i < MAX_CHANNEL; i++, pp++) {
                if ((cp == pp) || (pp->sd < 0))
                   continue;
                if (strncmp(buf + 1, pp->UID + 1, MAX_ID_LEN - 2))
                   continue;
                printf("Match found ch:%d=%d\n", pp - CHANNEL, cp - CHANNEL);
                cp->peer = i;
                pp->peer = cp - CHANNEL;
                strncpy(cp->UID, pp->UID, MAX_ID_LEN);
                send(cp->sd, "O", 1, 0);
                return;                
            }
          default: // invalid protocol
            send(cp->sd, "X", 1, 0);
            break;
        }
        return;
    }
    while ((rsize = recv(cp->sd, buf, BUFSIZE - 1, 0)) > 0) {
        cp->alive_timer = CHANNEL[cp->peer].alive_timer = ALIVE_TIME;
        // write-repeative
        printf("(%d->%d:%d)", cp - CHANNEL, cp->peer, rsize);
        count = 0;
        while (rsize) {
            if ((wsize = send(CHANNEL[cp->peer].sd, buf + count, rsize, 0)) < 0) {
                if (errno == EAGAIN) {
                    printf("."); fflush(stdout);
                    continue; // may incur endless loop;
                }
                perror("socket");
                printf("\nData Send Error CH=%d\n", cp->peer);
                goto leave;
            } else {
                rsize -= wsize;
                count += wsize;
            }
        }
    }
leave:
    if ((rsize != 0) && (errno == EAGAIN))
        return;
error:
    CloseChannel(cp);
    return;
}

