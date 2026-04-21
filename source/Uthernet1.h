#pragma once

#include "Card.h"

/* define this only if VICE should write each and every frame received
   and send into the VICE log
   WARNING: The log grows very fast!
*/
/** #define TFE_DEBUG_FRAMES **/
/** #define TFE_DEBUG_DUMP 1 **/

#define TFE_DEBUG_WARN 1 /* this should not be deactivated */
#define TFE_DEBUG_INIT 1
/** #define TFE_DEBUG_LOAD 1 **/
/** #define TFE_DEBUG_STORE 1 **/

#define TFE_COUNT_IO_REGISTER 0x10 /* we have 16 I/O register */
#define MAX_PACKETPAGE_ARRAY 0x1000 /* 4 KB */

/* ------------------------------------------------------------------------- */
/*    reading and writing TFE register functions                             */

/*
These registers are currently fully or partially supported:

TFE_PP_ADDR_CC_RXCFG        0x0102 * # RW - 4.4.6.,  p. 52 - 0003 *
TFE_PP_ADDR_CC_RXCTL        0x0104 * # RW - 4.4.8.,  p. 54 - 0005 *
TFE_PP_ADDR_CC_LINECTL      0x0112 * # RW - 4.4.16., p. 62 - 0013 *
TFE_PP_ADDR_SE_RXEVENT      0x0124 * # R- - 4.4.7.,  p. 53 - 0004 *
TFE_PP_ADDR_SE_BUSST        0x0138 * # R- - 4.4.21., p. 67 - 0018 *
TFE_PP_ADDR_TXCMD           0x0144 * # -W - 4.5., p. 70 - 5.7., p. 98 *
TFE_PP_ADDR_TXLENGTH        0x0146 * # -W - 4.5., p. 70 - 5.7., p. 98 *
TFE_PP_ADDR_MAC_ADDR        0x0158 * # RW - 4.6., p. 71 - 5.3., p. 86 *
                            0x015a
                            0x015c
*/

/*
	RW: RXTXDATA   = DE00/DE01
	RW: RXTXDATA2  = DE02/DE03 (for 32-bit-operation)
	-W: TXCMD      = DE04/DE05 (TxCMD, Transmit Command)   mapped to PP + 0144 (Reg. 9, Sec. 4.4, page 46)
	-W: TXLENGTH   = DE06/DE07 (TxLenght, Transmit Length) mapped to PP + 0146
	R-: INTSTQUEUE = DE08/DE09 (Interrupt Status Queue)    mapped to PP + 0120 (ISQ, Sec. 5.1, page 78)
	RW: PP_PTR     = DE0A/DE0B (PacketPage Pointer)        (see. page 75p: Read -011.---- ----.----)
	RW: PP_DATA0   = DE0C/DE0D (PacketPage Data (Port 0))
	RW: PP_DATA1   = DE0E/DE0F (PacketPage Data (Port 1))  (for 32 bit only)
*/

#define TFE_ADDR_RXTXDATA   0x00 /* RW */
#define TFE_ADDR_RXTXDATA2  0x02 /* RW 32 bit only! */
#define TFE_ADDR_TXCMD      0x04 /* -W Maps to PP+0144 */
#define TFE_ADDR_TXLENGTH   0x06 /* -W Maps to PP+0146 */
#define TFE_ADDR_INTSTQUEUE 0x08 /* R- Interrupt status queue, maps to PP + 0120 */
#define TFE_ADDR_PP_PTR     0x0a /* RW PacketPage Pointer */
#define TFE_ADDR_PP_DATA    0x0c /* RW PacketPage Data, Port 0 */
#define TFE_ADDR_PP_DATA2   0x0e /* RW PacketPage Data, Port 1 - 32 bit only */

/* The packetpage register: see p. 39f */
#define TFE_PP_ADDR_PRODUCTID       0x0000 /*   R- - 4.3., p. 41 */
#define TFE_PP_ADDR_IOBASE          0x0020 /* i RW - 4.3., p. 41 - 4.7., p. 72 */
#define TFE_PP_ADDR_INTNO           0x0022 /* i RW - 3.2., p. 17 - 4.3., p. 41 */
#define TFE_PP_ADDR_DMA_CHAN        0x0024 /* i RW - 3.2., p. 17 - 4.3., p. 41 */
#define TFE_PP_ADDR_DMA_SOF         0x0026 /* ? R- - 4.3., p. 41 - 5.4., p. 89 */
#define TFE_PP_ADDR_DMA_FC          0x0028 /* ? R- - 4.3., p. 41, "Receive DMA" */
#define TFE_PP_ADDR_RXDMA_BC        0x002a /* ? R- - 4.3., p. 41 - 5.4., p. 89 */
#define TFE_PP_ADDR_MEMBASE         0x002c /* i RW - 4.3., p. 41 - 4.9., p. 73 */
#define TFE_PP_ADDR_BPROM_BASE      0x0030 /* i RW - 3.6., p. 24 - 4.3., p. 41 */
#define TFE_PP_ADDR_BPROM_MASK      0x0034 /* i RW - 3.6., p. 24 - 4.3., p. 41 */
/* 0x0038 - 0x003F: reserved */
#define TFE_PP_ADDR_EEPROM_CMD      0x0040 /* i RW - 3.5., p. 23 - 4.3., p. 41 */
#define TFE_PP_ADDR_EEPROM_DATA     0x0042 /* i RW - 3.5., p. 23 - 4.3., p. 41 */
/* 0x0044 - 0x004F: reserved */
#define TFE_PP_ADDR_REC_FRAME_BC    0x0050 /*   RW - 4.3., p. 41 - 5.2.9., p. 86 */
/* 0x0052 - 0x00FF: reserved */
#define TFE_PP_ADDR_CONF_CTRL       0x0100 /* - RW - 4.4., p. 46; see below */
#define TFE_PP_ADDR_STATUS_EVENT    0x0120 /* - R- - 4.4., p. 46; see below */
/* 0x0140 - 0x0143: reserved */
#define TFE_PP_ADDR_TXCMD           0x0144 /* # -W - 4.5., p. 70 - 5.7., p. 98 */
#define TFE_PP_ADDR_TXLENGTH        0x0146 /* # -W - 4.5., p. 70 - 5.7., p. 98 */
/* 0x0148 - 0x014F: reserved */
#define TFE_PP_ADDR_LOG_ADDR_FILTER 0x0150 /*   RW - 4.6., p. 71 - 5.3., p. 86 */
#define TFE_PP_ADDR_MAC_ADDR        0x0158 /* # RW - 4.6., p. 71 - 5.3., p. 86 */
/* 0x015E - 0x03FF: reserved */
#define TFE_PP_ADDR_RXSTATUS        0x0400 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
#define TFE_PP_ADDR_RXLENGTH        0x0402 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
#define TFE_PP_ADDR_RX_FRAMELOC     0x0404 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
/* here, the received frame is stored */
#define TFE_PP_ADDR_TX_FRAMELOC     0x0A00 /*   -W - 4.7., p. 72 - 5.7., p. 98 */
/* here, the frame to transmit is stored */
#define TFE_PP_ADDR_END             0x1000 /* memory to TFE_PP_ADDR_END-1 is used */


/* TFE_PP_ADDR_CONF_CTRL is subdivided: */
#define TFE_PP_ADDR_CC_RXCFG        0x0102 /* # RW - 4.4.6.,  p. 52 - 0003 */
#define TFE_PP_ADDR_CC_RXCTL        0x0104 /* # RW - 4.4.8.,  p. 54 - 0005 */
#define TFE_PP_ADDR_CC_TXCFG        0x0106 /*   RW - 4.4.9.,  p. 55 - 0007 */
#define TFE_PP_ADDR_CC_TXCMD        0x0108 /*   R- - 4.4.11., p. 57 - 0009 */
#define TFE_PP_ADDR_CC_BUFCFG       0x010A /*   RW - 4.4.12., p. 58 - 000B */
#define TFE_PP_ADDR_CC_LINECTL      0x0112 /* # RW - 4.4.16., p. 62 - 0013 */
#define TFE_PP_ADDR_CC_SELFCTL      0x0114 /*   RW - 4.4.18., p. 64 - 0015 */
#define TFE_PP_ADDR_CC_BUSCTL       0x0116 /*   RW - 4.4.20., p. 66 - 0017 */
#define TFE_PP_ADDR_CC_TESTCTL      0x0118 /*   RW - 4.4.22., p. 68 - 0019 */

/* TFE_PP_ADDR_STATUS_EVENT is subdivided: */
#define TFE_PP_ADDR_SE_ISQ          0x0120 /*   R- - 4.4.5.,  p. 51 - 0000 */
#define TFE_PP_ADDR_SE_RXEVENT      0x0124 /* # R- - 4.4.7.,  p. 53 - 0004 */
#define TFE_PP_ADDR_SE_TXEVENT      0x0128 /*   R- - 4.4.10., p. 56 - 0008 */
#define TFE_PP_ADDR_SE_BUFEVENT     0x012C /*   R- - 4.4.13., p. 59 - 000C */
#define TFE_PP_ADDR_SE_RXMISS       0x0130 /*   R- - 4.4.14., p. 60 - 0010 */
#define TFE_PP_ADDR_SE_TXCOL        0x0132 /*   R- - 4.4.15., p. 61 - 0012 */
#define TFE_PP_ADDR_SE_LINEST       0x0134 /*   R- - 4.4.17., p. 63 - 0014 */
#define TFE_PP_ADDR_SE_SELFST       0x0136 /*   R- - 4.4.19., p. 65 - 0016 */
#define TFE_PP_ADDR_SE_BUSST        0x0138 /* # R- - 4.4.21., p. 67 - 0018 */
#define TFE_PP_ADDR_SE_TDR          0x013C /*   R- - 4.4.23., p. 69 - 001C */

/* ------------------------------------------------------------------------- */
/*    some parameter definitions                                             */

class NetworkBackend;

class Uthernet1 : public Card
{
public:
	Uthernet1(UINT slot);

	virtual void Destroy(void) {}
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	BYTE tfe_read(WORD ioaddress);
	void tfe_store(WORD ioaddress, BYTE byte);

	static const std::string& GetSnapshotCardName(void);

private:

	void Init();

	void tfe_sideeffects_write_pp_on_txframe(WORD ppaddress);
	void tfe_sideeffects_write_pp(WORD ppaddress, int oddaddress);
	void tfe_sideeffects_read_pp(WORD ppaddress);
	void tfe_proceed_rx_buffer(int oddaddress);

	WORD tfe_receive(void);
	int tfe_should_accept(unsigned char *buffer, int length, int *phashed, int *phash_index,
                          int *pcorrect_mac, int *pbroadcast, int *pmulticast);

	// this function is virtually useless
	// it is only here to keep a record of these unused arguments
	void tfe_transmit(
		int force,				/* FORCE: Delete waiting frames in transmit buffer */
		int onecoll,			/* ONECOLL: Terminate after just one collision */
		int inhibit_crc,		/* INHIBITCRC: Do not append CRC to the transmission */
		int tx_pad_dis,			/* TXPADDIS: Disable padding to 60 Bytes */
		int txlength,			/* Frame length */
		uint8_t *txframe		/* Pointer to the frame to be transmitted */
	);

	std::shared_ptr<NetworkBackend> networkBackend;

#ifdef TFE_DEBUG_DUMP
	void tfe_debug_output_general( const char *what, WORD (Uthernet1::*getFunc)(int), int count );
	WORD tfe_debug_output_io_getFunc( int i );
	WORD tfe_debug_output_pp_getFunc( int i );
	void tfe_debug_output_io( void );
	void tfe_debug_output_pp( void );
#endif

	/* status which received packages to accept
	   This is used in tfe_should_accept().
	*/
	BYTE  tfe_ia_mac[6];

	/* remember the value of the hash mask */
	DWORD tfe_hash_mask[2];

	int  tfe_recv_broadcast;	/* broadcast */
	int  tfe_recv_mac;			/* individual address (IA) */
	int  tfe_recv_multicast;	/* multicast if address passes the hash filter */
	int  tfe_recv_correct;		/* accept correct frames */
	int  tfe_recv_promiscuous;	/* promiscuous mode */
	int  tfe_recv_hashfilter;	/* accept if IA passes the hash filter */

#ifdef TFE_DEBUG_WARN
	/* remember if the TXCMD has been completed before a new one is issued */
	int tfe_started_tx;
#endif

	/* TFE registers */
	/* these are the 8 16-bit-ports for "I/O space configuration"
	   (see 4.10 on page 75 of cs8900a-4.pdf, the cs8900a data sheet)

	   REMARK: The TFE operatoes the cs8900a in IO space configuration, as
	   it generates I/OW and I/OR signals.
	*/

	BYTE tfe[TFE_COUNT_IO_REGISTER];

	WORD txcollect_buffer;
	WORD rx_buffer;

	/* The PacketPage register */
	/* note: The locations 0 to MAX_PACKETPAGE_ARRAY-1 are handled in this array. */

	BYTE tfe_packetpage[MAX_PACKETPAGE_ARRAY];
	WORD tfe_packetpage_ptr;
};
