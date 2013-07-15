P2P-xfer is a simple / unsecure p2p (peer to peer) relay service.

  Copyright (C) 2013,  Minsuk Lee (minsuk@hansug.ac.kr)
  All rights reserved.
  This software is under BSD license. see LICENSE.txt

Listen channels for incoming connection may not get any incoming connection
request due to the firewalls and/or NAT (Network address translation) devices. 

There is a well known method called 'hole punching' to handle the situation,
which is used by many p2p applications including Skype. Hole punching technique
is using UDP channel for p2p data transfer.
That means the applications have the responsibility to reliable data transfer
which is usually guaranteed by TCP protocol.

p2p-xfer implements very simple TCP-based p2p data transfer service.
p2p-xfer still needs a relay server which allows incoming connection thru
the firewall with public IP address and firewall-safe TCP port.

Any two hosts can make connection via p2p-relay server.

Here is the scenario.

1. starts p2p-relay with public IP and firewall-free listening TCP port
2. a host (A), being the server of the connection connects to the p2p-relay
3. A send its UserID, as 'T<userid>$', to p2p-relay
4. another host (B), being the client, connects to the p2p-relay
5. B send A's UserID, as 'C<userid>$', to p2p-relay
6. p2p-relay made logical connection between A and B
   if the userid is not on p2p-relay's list, reject B's connection request.
7. p2p-relay starts relay data from A to B, and data B to A.
8. if A or B close connection, p2p-relay close both connection.

Build and running

$ make in Linux builds p2p-relay server and sender/receiver clients
sender/receiver can be built also in Windows using Microsoft
Visual Studio. Actually, sender/receiver (send_r/recv_r) is the same
execution binary, which works as the receiver if the filename is '-'.

1. p2p-relay.c (Linux relay server)

    $ p2p-relay port-number

 where the port(TCP) is not blocked by Firewall,
 and the host is reachable by pucblic IP address.
	
2. p2p-xfer.c (Linux/Windows sender)

 p2p-xfer.c is a sample p2p-client appication to test p2p-relay.
 But it's still very useful to transfer files via p2p-relay.

 In linux, send_r, recv_r is the sender/receiver, in windows '.exe'
 suffix will be added.
 
     $ send_r filename user-id ip-address port-number [v]
     $ recv_r - user-id ip-address port-number [v]

 where filename is the file to send. For receiver it should be '-'.
 user-id is the only key to match sender/receiver. p2p-relay only
 lookup the userid of receivers when a sender wants to connect a receiver.
 This limits only two connection from a host, one for sender the other for
 receiver. (This can be easily modified by changing matching logic)
 'v' option if to enable verbose mode, in which the sender/receiver
 shows version, copyright message and its progress.

--- summary in Korean (한글 요약)
 
p2p-relay.c 는 리눅스용 p2p relay 서버 소스
p2p-xfer.c는 Linux 및 윈도우즈용 파일 sender/receiver 소스

Linux에서는 make로 p2p-relay, send_r, recv_r 이 만들어지고,
Windows에서는 Visual Studio Express로 xfer.exe를 만든 뒤
  send_r.exe, recv_r.exe로 복사해서 사용
  recv_r (recv_r.exe)은 반드시 filename를 '-'로 설정
