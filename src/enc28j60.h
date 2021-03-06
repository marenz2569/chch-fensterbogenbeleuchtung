#ifndef ENC28J60_H__
#define ENC28J60_H__

#include <stdint.h>

#define ENC28J60_enable         ENC28J60_CS_PORT &= ~_BV(ENC28J60_CS_PIN)
#define ENC28J60_disable        ENC28J60_CS_PORT |= _BV(ENC28J60_CS_PIN)

#define ENC28J60_MAX_FRAMELEN_M 966
enum {
	ENC28J60_MAX_FRAMELEN = ENC28J60_MAX_FRAMELEN_M
};

enum {
	ENC28J60_MAX_DATALEN = (ENC28J60_MAX_FRAMELEN_M - 60 - 3)
};

uint16_t enc28j60_curPacketPointer;

extern uint8_t *enc28j60_buffer;

uint8_t enc28j60_readOp(uint8_t op, uint8_t addr);

void enc28j60_writeOp(uint8_t op, uint8_t adrr, uint8_t data);

void enc28j60_readBuf(uint16_t len, uint8_t *data);

void enc28j60_writeBuf(uint16_t len, const uint8_t *data);

void enc28j60_set_random_access(uint16_t offset);

void enc28j60_setBank(uint8_t addr);

uint8_t enc28j60_readReg(uint8_t addr);

uint16_t enc28j60_readReg16(uint8_t addr);

void enc28j60_writeReg(uint8_t addr, uint8_t data);

void enc28j60_writeReg16(uint8_t addr, uint16_t data);

uint16_t enc28j60_readPhy(uint8_t addr);

void enc28j60_writePhy(uint8_t addr, uint16_t data);

void enc28j60_reset(void);

uint8_t enc28j60_init(void);

uint16_t enc28j60_packetReceive(void);

void enc28j60_freePacketSpace(void);

void enc28j60_packetSend(uint16_t len);

void enc28j60_dma(uint16_t start, uint16_t end, uint16_t dest);

#endif
