#pragma once

// Uthernet II registers

// only A0 and A1 are decoded
#define U2_C0X_MASK               0x03

#define U2_C0X_MODE_REGISTER      (0x04 & U2_C0X_MASK)
#define U2_C0X_ADDRESS_HIGH       (0x05 & U2_C0X_MASK)
#define U2_C0X_ADDRESS_LOW        (0x06 & U2_C0X_MASK)
#define U2_C0X_DATA_PORT          (0x07 & U2_C0X_MASK)

// W5100 registers and values

#define W5100_MR                0x0000
#define W5100_GAR0              0x0001
#define W5100_GAR3              0x0004
#define W5100_SUBR0             0x0005
#define W5100_SUBR3             0x0008
#define W5100_SHAR0             0x0009
#define W5100_SHAR5             0x000E
#define W5100_SIPR0             0x000F
#define W5100_SIPR3             0x0012
#define W5100_RTR0              0x0017
#define W5100_RTR1              0x0018
#define W5100_RCR               0x0019
#define W5100_RMSR              0x001A
#define W5100_TMSR              0x001B
#define W5100_PTIMER            0x0028
#define W5100_UPORT1            0x002F
#define W5100_S0_BASE           0x0400
#define W5100_S3_MAX            0x07FF
#define W5100_TX_BASE           0x4000
#define W5100_RX_BASE           0x6000
#define W5100_MEM_MAX           0x7FFF
#define W5100_MEM_SIZE          0x8000

#define W5100_MR_IND              0x01  // 0
#define W5100_MR_AI               0x02  // 1
#define W5100_MR_PPOE             0x08  // 3
#define W5100_MR_PB               0x10  // 4
#define W5100_MR_RST              0x80  // 7

#define W5100_SN_MR_PROTO_MASK    0x0F
#define W5100_SN_MR_MF            0x40  // 6
#define W5100_SN_MR_CLOSED        0x00
#define W5100_SN_MR_TCP           0x01
#define W5100_SN_MR_UDP           0x02
#define W5100_SN_MR_IPRAW         0x03
#define W5100_SN_MR_MACRAW        0x04
#define W5100_SN_MR_PPPOE         0x05
#define W5100_SN_VIRTUAL_DNS      0x08  // not present on real hardware, see comment in Uthernet2.cpp
#define W5100_SN_MR_TCP_DNS       (W5100_SN_VIRTUAL_DNS | W5100_SN_MR_TCP)
#define W5100_SN_MR_UDP_DNS       (W5100_SN_VIRTUAL_DNS | W5100_SN_MR_UDP)
#define W5100_SN_MR_IPRAW_DNS     (W5100_SN_VIRTUAL_DNS | W5100_SN_MR_IPRAW)

#define W5100_SN_CR_OPEN          0x01
#define W5100_SN_CR_LISTENT       0x02
#define W5100_SN_CR_CONNECT       0x04
#define W5100_SN_CR_DISCON        0x08
#define W5100_SN_CR_CLOSE         0x10
#define W5100_SN_CR_SEND          0x20
#define W5100_SN_CR_RECV          0x40

#define W5100_SN_MR               0x00
#define W5100_SN_CR               0x01
#define W5100_SN_SR               0x03
#define W5100_SN_PORT0            0x04
#define W5100_SN_PORT1            0x05
#define W5100_SN_DHAR0            0x06
#define W5100_SN_DHAR1            0x07
#define W5100_SN_DHAR2            0x08
#define W5100_SN_DHAR3            0x09
#define W5100_SN_DHAR4            0x0A
#define W5100_SN_DHAR5            0x0B
#define W5100_SN_DIPR0            0x0C
#define W5100_SN_DIPR1            0x0D
#define W5100_SN_DIPR2            0x0E
#define W5100_SN_DIPR3            0x0F
#define W5100_SN_DPORT0           0x10
#define W5100_SN_DPORT1           0x11
#define W5100_SN_PROTO            0x14
#define W5100_SN_TOS              0x15
#define W5100_SN_TTL              0x16

#define W5100_SN_TX_FSR0          0x20  // TX Free Size
#define W5100_SN_TX_FSR1          0x21  // TX Free Size
#define W5100_SN_TX_RD0           0x22  // TX Read Pointer
#define W5100_SN_TX_RD1           0x23  // TX Read Pointer
#define W5100_SN_TX_WR0           0x24  // TX Write Pointer
#define W5100_SN_TX_WR1           0x25  // TX Write Pointer

#define W5100_SN_RX_RSR0          0x26  // RX Receive Size
#define W5100_SN_RX_RSR1          0x27  // RX Receive Size
#define W5100_SN_RX_RD0           0x28  // RX Read Pointer
#define W5100_SN_RX_RD1           0x29  // RX Read Pointer

#define W5100_SN_DNS_NAME_LEN     0x2A  // these are not present on real hardware, see comment in Uthernet2.cpp
#define W5100_SN_DNS_NAME_BEGIN   0x2B
#define W5100_SN_DNS_NAME_END     0xFF
#define W5100_SN_DNS_NAME_CPTY    (W5100_SN_DNS_NAME_END - W5100_SN_DNS_NAME_BEGIN)

#define W5100_SN_SR_CLOSED        0x00
#define W5100_SN_SR_SOCK_INIT     0x13
#define W5100_SN_SR_SOCK_SYNSENT  0x15
#define W5100_SN_SR_ESTABLISHED   0x17
#define W5100_SN_SR_SOCK_UDP      0x22
#define W5100_SN_SR_SOCK_IPRAW    0x32
#define W5100_SN_SR_SOCK_MACRAW   0x42
