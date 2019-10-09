# Simple-TFTP-Application
Simple file transfer application using UDP. Some modifications in the transfer protocol include:
1. A batch of 100 packets of 1 MB each will be transmitted at a time, then an ACK signal will be transmitted for each packet successfully  delivered to the client.
2. Server waits until all ACK signals have been transmitted from the client, then checks if there are any missing packets.
3. Packets that are not sent successfully out of a batch of 100 packets will be retransmitted in the next batch, thus eliminating the risk of packet losses from UDP.
