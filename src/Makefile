all:	p2p-relay send_r
p2p-relay:	p2p_relay.c
	gcc -op2p-relay -g p2p_relay.c
send_r: p2p-xfer.c
	gcc -osend_r -g p2p-xfer.c
	ln -f -s send_r recv_r

clean:
	rm -f p2p-relay *.o send_r recv_r
