#include <stdlib.h>
#include <string.h>

#include "enc28j60.h"
#include "ethernet_protocols.h"

#define FILL16(to, data) \
        enc28j60_buffer[to] = (data) >> 8; \
        enc28j60_buffer[to+1] = data

uint8_t eth_type_is_arp_and_my_ip(const uint16_t len, const uint8_t *ip)
{
	return len >= 42 && enc28j60_buffer[ETH_TYPE_H_P] == ETHTYPE_ARP_H_V &&
	       enc28j60_buffer[ETH_TYPE_L_P] == ETHTYPE_ARP_L_V &&
	       memcmp(enc28j60_buffer + ETH_ARP_DST_IP_P, ip, 4) == 0;
}

void make_arp_answer_from_request(const uint8_t *mac, const uint8_t *ip)
{
	memcpy(enc28j60_buffer + ETH_DST_MAC, enc28j60_buffer + ETH_SRC_MAC, 6);
	memcpy(enc28j60_buffer + ETH_SRC_MAC, mac, 6);
	enc28j60_buffer[ETH_ARP_OPCODE_H_P] = ETH_ARP_OPCODE_REPLY_H_V;
	enc28j60_buffer[ETH_ARP_OPCODE_L_P] = ETH_ARP_OPCODE_REPLY_L_V;
	memcpy(enc28j60_buffer + ETH_ARP_DST_MAC_P, enc28j60_buffer + ETH_ARP_SRC_MAC_P, 6);
	memcpy(enc28j60_buffer + ETH_ARP_SRC_MAC_P, mac, 6);
	memcpy(enc28j60_buffer + ETH_ARP_DST_IP_P, enc28j60_buffer + ETH_ARP_SRC_IP_P, 4);
	memcpy(enc28j60_buffer + ETH_ARP_SRC_IP_P, ip, 4);
	enc28j60_packetSend(42);
}

uint8_t eth_type_is_ip_and_my_ip(const uint16_t len, const uint8_t *ip, const uint8_t *broadcast)
{
	const uint8_t allOnes[] = {0xff, 0xff, 0xff, 0xff};

	return len >= 42 && enc28j60_buffer[ETH_TYPE_H_P] == ETHTYPE_IP_H_V &&
	       enc28j60_buffer[ETH_TYPE_L_P] == ETHTYPE_IP_L_V &&
	       enc28j60_buffer[IP_HEADER_LEN_VER_P] == 0x45 &&
	       (memcmp(enc28j60_buffer + IP_DST_P, ip, 4) == 0 ||
	        memcmp(enc28j60_buffer + IP_DST_P, broadcast, 4) == 0 ||
	        memcmp(enc28j60_buffer + IP_DST_P, allOnes, 4) == 0);
}

static void fill_checksum(uint8_t dest, const uint8_t off, uint16_t len, const uint8_t type)
{
	const uint8_t *ptr = enc28j60_buffer + off;
	uint32_t sum = type==1?IP_PROTO_UDP_V+len-8:type==2?IP_PROTO_TCP_V+len-8:0;

	enc28j60_buffer[dest] = 0x00;
	enc28j60_buffer[dest + 1] = 0x00;

	while (len > 1) {
		sum += (uint16_t) (((uint16_t)*ptr<<8)|*(ptr+1));
		ptr += 2;
		len -= 2;
	}
	if (len) {
		sum += ((uint32_t)*ptr) << 8;
	}
	while (sum >> 16) {
		sum = (uint16_t) sum + (sum >> 16);
	}
	uint16_t ck = ~(uint16_t)sum;
	enc28j60_buffer[dest] = ck >> 8;
	enc28j60_buffer[dest + 1] = ck;
}

static void make_ip_checksum(void)
{
	enc28j60_buffer[IP_FLAGS_P] = 0x40;
	enc28j60_buffer[IP_FLAGS_P + 1] = 0;
	enc28j60_buffer[IP_TTL_P] = 64;
	fill_checksum(IP_CHECKSUM_P, IP_P, IP_HEADER_LEN, 0);
}

static void make_return_packet(const uint8_t *mac, const uint8_t *ip)
{
	memcpy(enc28j60_buffer + ETH_DST_MAC, enc28j60_buffer + ETH_SRC_MAC, 6);
        memcpy(enc28j60_buffer + ETH_SRC_MAC, mac, 6);
        memcpy(enc28j60_buffer + IP_DST_P, enc28j60_buffer + IP_SRC_P, 4);
        memcpy(enc28j60_buffer + IP_SRC_P, ip, 4);
}

void makeUdpReply(uint16_t datalen, const uint8_t *mac, const uint8_t *ip, const uint16_t port)
{
	datalen = datalen>ENC28J60_BUFFERSIZE?ENC28J60_BUFFERSIZE:datalen;
	enc28j60_buffer[IP_TOTLEN_H_P] = (IP_HEADER_LEN + UDP_HEADER_LEN + datalen) >> 8;
	enc28j60_buffer[IP_TOTLEN_L_P] = IP_HEADER_LEN + UDP_HEADER_LEN + datalen;
	make_return_packet(mac, ip);

	make_ip_checksum();

	memcpy(enc28j60_buffer + UDP_DST_PORT_H_P, enc28j60_buffer + UDP_SRC_PORT_H_P, 2);
	enc28j60_buffer[UDP_SRC_PORT_H_P] = port >> 8;
	enc28j60_buffer[UDP_SRC_PORT_L_P] = port;
	enc28j60_buffer[UDP_LEN_H_P] = (UDP_HEADER_LEN + datalen) >> 8;
	enc28j60_buffer[UDP_LEN_L_P] = UDP_HEADER_LEN + datalen;

	/* udp checksum */
	fill_checksum(UDP_CHECKSUM_H_P, IP_SRC_P, 16 + datalen, 1);

	enc28j60_packetSend(ETH_HEADER_LEN + IP_HEADER_LEN + UDP_HEADER_LEN + datalen);
}

static uint8_t seqnum = 0x0a;

void make_tcphead(uint8_t *buf,uint16_t rel_ack_num,uint8_t mss,uint8_t cp_seq)
{
        uint8_t i=0;
        uint8_t tseq;
	uint16_t tmp_port;
	memcpy(&tmp_port, buf + TCP_DST_PORT_H_P, 2);
	memcpy(buf + TCP_DST_PORT_H_P, buf + TCP_SRC_PORT_H_P, 2);
	memcpy(buf + TCP_SRC_PORT_H_P, &tmp_port, 2);
        i=4;
        // sequence numbers:
        // add the rel ack num to SEQACK
        while(i>0){
                rel_ack_num += buf[TCP_SEQ_H_P+i-1];
                tseq=buf[TCP_SEQACK_H_P+i-1];
                buf[TCP_SEQACK_H_P+i-1]=0xff&rel_ack_num;
                if (cp_seq){
                        // copy the acknum sent to us into the sequence number
                        buf[TCP_SEQ_H_P+i-1]=tseq;
                }else{
                        buf[TCP_SEQ_H_P+i-1]= 0; // some preset vallue
                }
                rel_ack_num=rel_ack_num>>8;
                i--;
        }
        if (cp_seq==0){
                // put inital seq number
                buf[TCP_SEQ_H_P+0]= 0;
                buf[TCP_SEQ_H_P+1]= 0;
                // we step only the second byte, this allows us to send packts 
                // with 255 bytes or 512 (if we step the initial seqnum by 2)
                buf[TCP_SEQ_H_P+2]= seqnum; 
                buf[TCP_SEQ_H_P+3]= 0;
                // step the inititial seq num by something we will not use
                // during this tcp session:
                seqnum+=2;
        }
        // zero the checksum
        buf[TCP_CHECKSUM_H_P]=0;
        buf[TCP_CHECKSUM_L_P]=0;

        // The tcp header length is only a 4 bit field (the upper 4 bits).
        // It is calculated in units of 4 bytes. 
        // E.g 24 bytes: 24/4=6 => 0x60=header len field
        //buf[TCP_HEADER_LEN_P]=(((TCP_HEADER_LEN_PLAIN+4)/4)) <<4; // 0x60
        if (mss){
                // the only option we set is MSS to 1408:
                // 1408 in hex is 0x580
                buf[TCP_OPTIONS_P]=2;
                buf[TCP_OPTIONS_P+1]=4;
                buf[TCP_OPTIONS_P+2]=0x05; 
                buf[TCP_OPTIONS_P+3]=0x80;
                // 24 bytes:
                buf[TCP_HEADER_LEN_P]=0x60;
        }else{
                // no options:
                // 20 bytes:
                buf[TCP_HEADER_LEN_P]=0x50;
        }
}

static void make_tcp_checksum_and_send(uint16_t len)
{
	/* checksum calculation starts at IP_SRC_P and contains full TCP header and data */
	fill_checksum(TCP_CHECKSUM_H_P, IP_SRC_P, 8 + TCP_HEADER_LEN_PLAIN + len, 2);

	enc28j60_packetSend(ETH_HEADER_LEN + IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + len);
}

void make_tcp_synack(const uint8_t *mac, const uint8_t *ip)
{
	make_return_packet(mac, ip);

	FILL16(IP_TOTLEN_H_P, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + 4);

	make_ip_checksum();

	enc28j60_buffer[TCP_FLAGS_P] = TCP_FLAGS_SYNACK_V;
	make_tcphead(enc28j60_buffer, 1, 1, 0);

	make_tcp_checksum_and_send(4);
}

uint16_t get_tcp_header_len(void)
{
	return (uint16_t) ((enc28j60_buffer[TCP_HEADER_LEN_P] >> 4) * 4);
}

uint16_t get_tcp_data_len(void)
{
	int16_t i = (int16_t) (((int16_t) (enc28j60_buffer[IP_TOTLEN_H_P] << 8)) | enc28j60_buffer[IP_TOTLEN_L_P]);
	i -= IP_HEADER_LEN;
	i -= get_tcp_header_len();
	i = i<0?0:i;

	return (uint16_t) i;
}

void make_tcp_ack_from_any(const uint8_t *mac, const uint8_t *ip)
{
	uint16_t tcp_datalen;

	make_return_packet(mac, ip);

	FILL16(IP_TOTLEN_H_P, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN);

	make_ip_checksum();

        // fill the header:
	tcp_datalen = get_tcp_data_len();
	tcp_datalen = tcp_datalen==0?1:tcp_datalen;
        enc28j60_buffer[TCP_FLAGS_P]=TCP_FLAGS_ACK_V;
        // if there is no data then we must still acknoledge one packet
        make_tcphead(enc28j60_buffer,tcp_datalen,0,1); // no options

	make_tcp_checksum_and_send(0);
}

/*
 * we do not need to assemble the parts IP and TCP header,
 * as we send the ack with data right after an normal ack,
 * from which we can keep parts.
 *
 * as we do not keep tcp state information,
 * the connection will be closed with a fin.
 */
void make_tcp_ack_with_data(uint16_t dlen)
{
	FILL16(IP_TOTLEN_H_P, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + dlen);

	make_ip_checksum();

	enc28j60_buffer[TCP_FLAGS_P]=TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V|TCP_FLAGS_FIN_V;

	make_tcp_checksum_and_send(dlen);
}
