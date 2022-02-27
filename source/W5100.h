#pragma once

// Uthernet II registers

#define C0X_MODE_REGISTER   0x04
#define C0X_ADDRESS_HIGH    0x05
#define C0X_ADDRESS_LOW     0x06
#define C0X_DATA_PORT       0x07

// W5100 registers and values

#define MR                0x0000
#define GAR0              0x0001
#define GAR3              0x0004
#define SUBR0             0x0005
#define SUBR3             0x0008
#define SHAR0             0x0009
#define SHAR5             0x000E
#define SIPR0             0x000F
#define SIPR3             0x0012
#define RTR0              0x0017
#define RTR1              0x0018
#define RMSR              0x001A
#define TMSR              0x001B
#define UPORT1            0x002F
#define S0_BASE           0x0400
#define S3_MAX            0x07FF
#define TX_BASE           0x4000
#define RX_BASE           0x6000
#define MEM_MAX           0x7FFF
#define MEM_SIZE          0x8000

#define MR_IND              0x01  // 0
#define MR_AI               0x02  // 1
#define MR_PPOE             0x08  // 3
#define MR_PB               0x10  // 4
#define MR_RST              0x80  // 7

#define SN_MR_PROTO_MASK    0x0F
#define SN_MR_MF            0x40  // 6
#define SN_MR_CLOSED        0x00
#define SN_MR_TCP           0x01
#define SN_MR_UDP           0x02
#define SN_MR_IPRAW         0x03
#define SN_MR_MACRAW        0x04
#define SN_MR_PPPOE         0x05

#define SN_CR_OPEN          0x01
#define SN_CR_LISTENT       0x02
#define SN_CR_CONNECT       0x04
#define SN_CR_DISCON        0x08
#define SN_CR_CLOSE         0x10
#define SN_CR_SEND          0x20
#define SN_CR_RECV          0x40

#define SN_MR               0x00
#define SN_CR               0x01
#define SN_SR               0x03
#define SN_PORT0            0x04
#define SN_PORT1            0x05
#define SN_DIPR0            0x0C
#define SN_DIPR1            0x0D
#define SN_DIPR2            0x0E
#define SN_DIPR3            0x0F
#define SN_DPORT0           0x10
#define SN_DPORT1           0x11
#define SN_PROTO            0x14
#define SN_TOS              0x15
#define SN_TTL              0x16
#define SN_TX_FSR0          0x20
#define SN_TX_FSR1          0x21
#define SN_TX_RD0           0x22
#define SN_TX_RD1           0x23
#define SN_TX_WR0           0x24
#define SN_TX_WR1           0x25
#define SN_RX_RSR0          0x26
#define SN_RX_RSR1          0x27
#define SN_RX_RD0           0x28
#define SN_RX_RD1           0x29

#define SN_SR_CLOSED        0x00
#define SN_SR_SOCK_INIT     0x13
#define SN_SR_ESTABLISHED   0x17
#define SN_SR_SOCK_UDP      0x22
#define SN_SR_SOCK_IPRAW    0x32
#define SN_SR_SOCK_MACRAW   0x42
