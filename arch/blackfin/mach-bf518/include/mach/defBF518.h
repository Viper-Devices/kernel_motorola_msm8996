/*
 * Copyright 2008-2009 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF518_H
#define _DEF_BF518_H

/* Include all Core registers and bit definitions */
#include <asm/def_LPBlackfin.h>

/* SYSTEM & MMR ADDRESS DEFINITIONS FOR ADSP-BF518 */

/* Include defBF51x_base.h for the set of #defines that are common to all ADSP-BF51x processors */
#include "defBF51x_base.h"

/* The following are the #defines needed by ADSP-BF518 that are not in the common header */
/* 10/100 Ethernet Controller	(0xFFC03000 - 0xFFC031FF) */

#define EMAC_OPMODE             0xFFC03000       /* Operating Mode Register                              */
#define EMAC_ADDRLO             0xFFC03004       /* Address Low (32 LSBs) Register                       */
#define EMAC_ADDRHI             0xFFC03008       /* Address High (16 MSBs) Register                      */
#define EMAC_HASHLO             0xFFC0300C       /* Multicast Hash Table Low (Bins 31-0) Register        */
#define EMAC_HASHHI             0xFFC03010       /* Multicast Hash Table High (Bins 63-32) Register      */
#define EMAC_STAADD             0xFFC03014       /* Station Management Address Register                  */
#define EMAC_STADAT             0xFFC03018       /* Station Management Data Register                     */
#define EMAC_FLC                0xFFC0301C       /* Flow Control Register                                */
#define EMAC_VLAN1              0xFFC03020       /* VLAN1 Tag Register                                   */
#define EMAC_VLAN2              0xFFC03024       /* VLAN2 Tag Register                                   */
#define EMAC_WKUP_CTL           0xFFC0302C       /* Wake-Up Control/Status Register                      */
#define EMAC_WKUP_FFMSK0        0xFFC03030       /* Wake-Up Frame Filter 0 Byte Mask Register            */
#define EMAC_WKUP_FFMSK1        0xFFC03034       /* Wake-Up Frame Filter 1 Byte Mask Register            */
#define EMAC_WKUP_FFMSK2        0xFFC03038       /* Wake-Up Frame Filter 2 Byte Mask Register            */
#define EMAC_WKUP_FFMSK3        0xFFC0303C       /* Wake-Up Frame Filter 3 Byte Mask Register            */
#define EMAC_WKUP_FFCMD         0xFFC03040       /* Wake-Up Frame Filter Commands Register               */
#define EMAC_WKUP_FFOFF         0xFFC03044       /* Wake-Up Frame Filter Offsets Register                */
#define EMAC_WKUP_FFCRC0        0xFFC03048       /* Wake-Up Frame Filter 0,1 CRC-16 Register             */
#define EMAC_WKUP_FFCRC1        0xFFC0304C       /* Wake-Up Frame Filter 2,3 CRC-16 Register             */

#define EMAC_SYSCTL             0xFFC03060       /* EMAC System Control Register                         */
#define EMAC_SYSTAT             0xFFC03064       /* EMAC System Status Register                          */
#define EMAC_RX_STAT            0xFFC03068       /* RX Current Frame Status Register                     */
#define EMAC_RX_STKY            0xFFC0306C       /* RX Sticky Frame Status Register                      */
#define EMAC_RX_IRQE            0xFFC03070       /* RX Frame Status Interrupt Enables Register           */
#define EMAC_TX_STAT            0xFFC03074       /* TX Current Frame Status Register                     */
#define EMAC_TX_STKY            0xFFC03078       /* TX Sticky Frame Status Register                      */
#define EMAC_TX_IRQE            0xFFC0307C       /* TX Frame Status Interrupt Enables Register           */

#define EMAC_MMC_CTL            0xFFC03080       /* MMC Counter Control Register                         */
#define EMAC_MMC_RIRQS          0xFFC03084       /* MMC RX Interrupt Status Register                     */
#define EMAC_MMC_RIRQE          0xFFC03088       /* MMC RX Interrupt Enables Register                    */
#define EMAC_MMC_TIRQS          0xFFC0308C       /* MMC TX Interrupt Status Register                     */
#define EMAC_MMC_TIRQE          0xFFC03090       /* MMC TX Interrupt Enables Register                    */

#define EMAC_RXC_OK             0xFFC03100       /* RX Frame Successful Count                            */
#define EMAC_RXC_FCS            0xFFC03104       /* RX Frame FCS Failure Count                           */
#define EMAC_RXC_ALIGN          0xFFC03108       /* RX Alignment Error Count                             */
#define EMAC_RXC_OCTET          0xFFC0310C       /* RX Octets Successfully Received Count                */
#define EMAC_RXC_DMAOVF         0xFFC03110       /* Internal MAC Sublayer Error RX Frame Count           */
#define EMAC_RXC_UNICST         0xFFC03114       /* Unicast RX Frame Count                               */
#define EMAC_RXC_MULTI          0xFFC03118       /* Multicast RX Frame Count                             */
#define EMAC_RXC_BROAD          0xFFC0311C       /* Broadcast RX Frame Count                             */
#define EMAC_RXC_LNERRI         0xFFC03120       /* RX Frame In Range Error Count                        */
#define EMAC_RXC_LNERRO         0xFFC03124       /* RX Frame Out Of Range Error Count                    */
#define EMAC_RXC_LONG           0xFFC03128       /* RX Frame Too Long Count                              */
#define EMAC_RXC_MACCTL         0xFFC0312C       /* MAC Control RX Frame Count                           */
#define EMAC_RXC_OPCODE         0xFFC03130       /* Unsupported Op-Code RX Frame Count                   */
#define EMAC_RXC_PAUSE          0xFFC03134       /* MAC Control Pause RX Frame Count                     */
#define EMAC_RXC_ALLFRM         0xFFC03138       /* Overall RX Frame Count                               */
#define EMAC_RXC_ALLOCT         0xFFC0313C       /* Overall RX Octet Count                               */
#define EMAC_RXC_TYPED          0xFFC03140       /* Type/Length Consistent RX Frame Count                */
#define EMAC_RXC_SHORT          0xFFC03144       /* RX Frame Fragment Count - Byte Count x < 64          */
#define EMAC_RXC_EQ64           0xFFC03148       /* Good RX Frame Count - Byte Count x = 64              */
#define EMAC_RXC_LT128          0xFFC0314C       /* Good RX Frame Count - Byte Count  64 < x < 128       */
#define EMAC_RXC_LT256          0xFFC03150       /* Good RX Frame Count - Byte Count 128 <= x < 256      */
#define EMAC_RXC_LT512          0xFFC03154       /* Good RX Frame Count - Byte Count 256 <= x < 512      */
#define EMAC_RXC_LT1024         0xFFC03158       /* Good RX Frame Count - Byte Count 512 <= x < 1024     */
#define EMAC_RXC_GE1024         0xFFC0315C       /* Good RX Frame Count - Byte Count x >= 1024           */

#define EMAC_TXC_OK             0xFFC03180       /* TX Frame Successful Count                             */
#define EMAC_TXC_1COL           0xFFC03184       /* TX Frames Successful After Single Collision Count     */
#define EMAC_TXC_GT1COL         0xFFC03188       /* TX Frames Successful After Multiple Collisions Count  */
#define EMAC_TXC_OCTET          0xFFC0318C       /* TX Octets Successfully Received Count                 */
#define EMAC_TXC_DEFER          0xFFC03190       /* TX Frame Delayed Due To Busy Count                    */
#define EMAC_TXC_LATECL         0xFFC03194       /* Late TX Collisions Count                              */
#define EMAC_TXC_XS_COL         0xFFC03198       /* TX Frame Failed Due To Excessive Collisions Count     */
#define EMAC_TXC_DMAUND         0xFFC0319C       /* Internal MAC Sublayer Error TX Frame Count            */
#define EMAC_TXC_CRSERR         0xFFC031A0       /* Carrier Sense Deasserted During TX Frame Count        */
#define EMAC_TXC_UNICST         0xFFC031A4       /* Unicast TX Frame Count                                */
#define EMAC_TXC_MULTI          0xFFC031A8       /* Multicast TX Frame Count                              */
#define EMAC_TXC_BROAD          0xFFC031AC       /* Broadcast TX Frame Count                              */
#define EMAC_TXC_XS_DFR         0xFFC031B0       /* TX Frames With Excessive Deferral Count               */
#define EMAC_TXC_MACCTL         0xFFC031B4       /* MAC Control TX Frame Count                            */
#define EMAC_TXC_ALLFRM         0xFFC031B8       /* Overall TX Frame Count                                */
#define EMAC_TXC_ALLOCT         0xFFC031BC       /* Overall TX Octet Count                                */
#define EMAC_TXC_EQ64           0xFFC031C0       /* Good TX Frame Count - Byte Count x = 64               */
#define EMAC_TXC_LT128          0xFFC031C4       /* Good TX Frame Count - Byte Count  64 < x < 128        */
#define EMAC_TXC_LT256          0xFFC031C8       /* Good TX Frame Count - Byte Count 128 <= x < 256       */
#define EMAC_TXC_LT512          0xFFC031CC       /* Good TX Frame Count - Byte Count 256 <= x < 512       */
#define EMAC_TXC_LT1024         0xFFC031D0       /* Good TX Frame Count - Byte Count 512 <= x < 1024      */
#define EMAC_TXC_GE1024         0xFFC031D4       /* Good TX Frame Count - Byte Count x >= 1024            */
#define EMAC_TXC_ABORT          0xFFC031D8       /* Total TX Frames Aborted Count                         */

/* Listing for IEEE-Supported Count Registers */

#define FramesReceivedOK                EMAC_RXC_OK        /* RX Frame Successful Count                            */
#define FrameCheckSequenceErrors        EMAC_RXC_FCS       /* RX Frame FCS Failure Count                           */
#define AlignmentErrors                 EMAC_RXC_ALIGN     /* RX Alignment Error Count                             */
#define OctetsReceivedOK                EMAC_RXC_OCTET     /* RX Octets Successfully Received Count                */
#define FramesLostDueToIntMACRcvError   EMAC_RXC_DMAOVF    /* Internal MAC Sublayer Error RX Frame Count           */
#define UnicastFramesReceivedOK         EMAC_RXC_UNICST    /* Unicast RX Frame Count                               */
#define MulticastFramesReceivedOK       EMAC_RXC_MULTI     /* Multicast RX Frame Count                             */
#define BroadcastFramesReceivedOK       EMAC_RXC_BROAD     /* Broadcast RX Frame Count                             */
#define InRangeLengthErrors             EMAC_RXC_LNERRI    /* RX Frame In Range Error Count                        */
#define OutOfRangeLengthField           EMAC_RXC_LNERRO    /* RX Frame Out Of Range Error Count                    */
#define FrameTooLongErrors              EMAC_RXC_LONG      /* RX Frame Too Long Count                              */
#define MACControlFramesReceived        EMAC_RXC_MACCTL    /* MAC Control RX Frame Count                           */
#define UnsupportedOpcodesReceived      EMAC_RXC_OPCODE    /* Unsupported Op-Code RX Frame Count                   */
#define PAUSEMACCtrlFramesReceived      EMAC_RXC_PAUSE     /* MAC Control Pause RX Frame Count                     */
#define FramesReceivedAll               EMAC_RXC_ALLFRM    /* Overall RX Frame Count                               */
#define OctetsReceivedAll               EMAC_RXC_ALLOCT    /* Overall RX Octet Count                               */
#define TypedFramesReceived             EMAC_RXC_TYPED     /* Type/Length Consistent RX Frame Count                */
#define FramesLenLt64Received           EMAC_RXC_SHORT     /* RX Frame Fragment Count - Byte Count x < 64          */
#define FramesLenEq64Received           EMAC_RXC_EQ64      /* Good RX Frame Count - Byte Count x = 64              */
#define FramesLen65_127Received         EMAC_RXC_LT128     /* Good RX Frame Count - Byte Count  64 < x < 128       */
#define FramesLen128_255Received        EMAC_RXC_LT256     /* Good RX Frame Count - Byte Count 128 <= x < 256      */
#define FramesLen256_511Received        EMAC_RXC_LT512     /* Good RX Frame Count - Byte Count 256 <= x < 512      */
#define FramesLen512_1023Received       EMAC_RXC_LT1024    /* Good RX Frame Count - Byte Count 512 <= x < 1024     */
#define FramesLen1024_MaxReceived       EMAC_RXC_GE1024    /* Good RX Frame Count - Byte Count x >= 1024           */

#define FramesTransmittedOK             EMAC_TXC_OK        /* TX Frame Successful Count                            */
#define SingleCollisionFrames           EMAC_TXC_1COL      /* TX Frames Successful After Single Collision Count    */
#define MultipleCollisionFrames         EMAC_TXC_GT1COL    /* TX Frames Successful After Multiple Collisions Count */
#define OctetsTransmittedOK             EMAC_TXC_OCTET     /* TX Octets Successfully Received Count                */
#define FramesWithDeferredXmissions     EMAC_TXC_DEFER     /* TX Frame Delayed Due To Busy Count                   */
#define LateCollisions                  EMAC_TXC_LATECL    /* Late TX Collisions Count                             */
#define FramesAbortedDueToXSColls       EMAC_TXC_XS_COL    /* TX Frame Failed Due To Excessive Collisions Count    */
#define FramesLostDueToIntMacXmitError  EMAC_TXC_DMAUND    /* Internal MAC Sublayer Error TX Frame Count           */
#define CarrierSenseErrors              EMAC_TXC_CRSERR    /* Carrier Sense Deasserted During TX Frame Count       */
#define UnicastFramesXmittedOK          EMAC_TXC_UNICST    /* Unicast TX Frame Count                               */
#define MulticastFramesXmittedOK        EMAC_TXC_MULTI     /* Multicast TX Frame Count                             */
#define BroadcastFramesXmittedOK        EMAC_TXC_BROAD     /* Broadcast TX Frame Count                             */
#define FramesWithExcessiveDeferral     EMAC_TXC_XS_DFR    /* TX Frames With Excessive Deferral Count              */
#define MACControlFramesTransmitted     EMAC_TXC_MACCTL    /* MAC Control TX Frame Count                           */
#define FramesTransmittedAll            EMAC_TXC_ALLFRM    /* Overall TX Frame Count                               */
#define OctetsTransmittedAll            EMAC_TXC_ALLOCT    /* Overall TX Octet Count                               */
#define FramesLenEq64Transmitted        EMAC_TXC_EQ64      /* Good TX Frame Count - Byte Count x = 64              */
#define FramesLen65_127Transmitted      EMAC_TXC_LT128     /* Good TX Frame Count - Byte Count  64 < x < 128       */
#define FramesLen128_255Transmitted     EMAC_TXC_LT256     /* Good TX Frame Count - Byte Count 128 <= x < 256      */
#define FramesLen256_511Transmitted     EMAC_TXC_LT512     /* Good TX Frame Count - Byte Count 256 <= x < 512      */
#define FramesLen512_1023Transmitted    EMAC_TXC_LT1024    /* Good TX Frame Count - Byte Count 512 <= x < 1024     */
#define FramesLen1024_MaxTransmitted    EMAC_TXC_GE1024    /* Good TX Frame Count - Byte Count x >= 1024           */
#define TxAbortedFrames                 EMAC_TXC_ABORT     /* Total TX Frames Aborted Count                        */

/***********************************************************************************
** System MMR Register Bits And Macros
**
** Disclaimer:	All macros are intended to make C and Assembly code more readable.
**				Use these macros carefully, as any that do left shifts for field
**				depositing will result in the lower order bits being destroyed.  Any
**				macro that shifts left to properly position the bit-field should be
**				used as part of an OR to initialize a register and NOT as a dynamic
**				modifier UNLESS the lower order bits are saved and ORed back in when
**				the macro is used.
*************************************************************************************/

/************************  ETHERNET 10/100 CONTROLLER MASKS  ************************/

/* EMAC_OPMODE Masks */

#define	RE                 0x00000001     /* Receiver Enable                                    */
#define	ASTP               0x00000002     /* Enable Automatic Pad Stripping On RX Frames        */
#define	HU                 0x00000010     /* Hash Filter Unicast Address                        */
#define	HM                 0x00000020     /* Hash Filter Multicast Address                      */
#define	PAM                0x00000040     /* Pass-All-Multicast Mode Enable                     */
#define	PR                 0x00000080     /* Promiscuous Mode Enable                            */
#define	IFE                0x00000100     /* Inverse Filtering Enable                           */
#define	DBF                0x00000200     /* Disable Broadcast Frame Reception                  */
#define	PBF                0x00000400     /* Pass Bad Frames Enable                             */
#define	PSF                0x00000800     /* Pass Short Frames Enable                           */
#define	RAF                0x00001000     /* Receive-All Mode                                   */
#define	TE                 0x00010000     /* Transmitter Enable                                 */
#define	DTXPAD             0x00020000     /* Disable Automatic TX Padding                       */
#define	DTXCRC             0x00040000     /* Disable Automatic TX CRC Generation                */
#define	DC                 0x00080000     /* Deferral Check                                     */
#define	BOLMT              0x00300000     /* Back-Off Limit                                     */
#define	BOLMT_10           0x00000000     /*		10-bit range                            */
#define	BOLMT_8            0x00100000     /*		8-bit range                             */
#define	BOLMT_4            0x00200000     /*		4-bit range                             */
#define	BOLMT_1            0x00300000     /*		1-bit range                             */
#define	DRTY               0x00400000     /* Disable TX Retry On Collision                      */
#define	LCTRE              0x00800000     /* Enable TX Retry On Late Collision                  */
#define	RMII               0x01000000     /* RMII/MII* Mode                                     */
#define	RMII_10            0x02000000     /* Speed Select for RMII Port (10MBit/100MBit*)       */
#define	FDMODE             0x04000000     /* Duplex Mode Enable (Full/Half*)                    */
#define	LB                 0x08000000     /* Internal Loopback Enable                           */
#define	DRO                0x10000000     /* Disable Receive Own Frames (Half-Duplex Mode)      */

/* EMAC_STAADD Masks */

#define	STABUSY            0x00000001     /* Initiate Station Mgt Reg Access / STA Busy Stat    */
#define	STAOP              0x00000002     /* Station Management Operation Code (Write/Read*)    */
#define	STADISPRE          0x00000004     /* Disable Preamble Generation                        */
#define	STAIE              0x00000008     /* Station Mgt. Transfer Done Interrupt Enable        */
#define	REGAD              0x000007C0     /* STA Register Address                               */
#define	PHYAD              0x0000F800     /* PHY Device Address                                 */

#define	SET_REGAD(x) (((x)&0x1F)<<  6 )   /* Set STA Register Address                           */
#define	SET_PHYAD(x) (((x)&0x1F)<< 11 )   /* Set PHY Device Address                             */

/* EMAC_STADAT Mask */

#define	STADATA            0x0000FFFF     /* Station Management Data                            */

/* EMAC_FLC Masks */

#define	FLCBUSY            0x00000001     /* Send Flow Ctrl Frame / Flow Ctrl Busy Status       */
#define	FLCE               0x00000002     /* Flow Control Enable                                */
#define	PCF                0x00000004     /* Pass Control Frames                                */
#define	BKPRSEN            0x00000008     /* Enable Backpressure                                */
#define	FLCPAUSE           0xFFFF0000     /* Pause Time                                         */

#define	SET_FLCPAUSE(x) (((x)&0xFFFF)<< 16) /* Set Pause Time                                   */

/* EMAC_WKUP_CTL Masks */

#define	CAPWKFRM           0x00000001    /* Capture Wake-Up Frames                              */
#define	MPKE               0x00000002    /* Magic Packet Enable                                 */
#define	RWKE               0x00000004    /* Remote Wake-Up Frame Enable                         */
#define	GUWKE              0x00000008    /* Global Unicast Wake Enable                          */
#define	MPKS               0x00000020    /* Magic Packet Received Status                        */
#define	RWKS               0x00000F00    /* Wake-Up Frame Received Status, Filters 3:0          */

/* EMAC_WKUP_FFCMD Masks */

#define	WF0_E              0x00000001    /* Enable Wake-Up Filter 0                              */
#define	WF0_T              0x00000008    /* Wake-Up Filter 0 Addr Type (Multicast/Unicast*)      */
#define	WF1_E              0x00000100    /* Enable Wake-Up Filter 1                              */
#define	WF1_T              0x00000800    /* Wake-Up Filter 1 Addr Type (Multicast/Unicast*)      */
#define	WF2_E              0x00010000    /* Enable Wake-Up Filter 2                              */
#define	WF2_T              0x00080000    /* Wake-Up Filter 2 Addr Type (Multicast/Unicast*)      */
#define	WF3_E              0x01000000    /* Enable Wake-Up Filter 3                              */
#define	WF3_T              0x08000000    /* Wake-Up Filter 3 Addr Type (Multicast/Unicast*)      */

/* EMAC_WKUP_FFOFF Masks */

#define	WF0_OFF            0x000000FF    /* Wake-Up Filter 0 Pattern Offset                      */
#define	WF1_OFF            0x0000FF00    /* Wake-Up Filter 1 Pattern Offset                      */
#define	WF2_OFF            0x00FF0000    /* Wake-Up Filter 2 Pattern Offset                      */
#define	WF3_OFF            0xFF000000    /* Wake-Up Filter 3 Pattern Offset                      */

#define	SET_WF0_OFF(x) (((x)&0xFF)<<  0 ) /* Set Wake-Up Filter 0 Byte Offset                    */
#define	SET_WF1_OFF(x) (((x)&0xFF)<<  8 ) /* Set Wake-Up Filter 1 Byte Offset                    */
#define	SET_WF2_OFF(x) (((x)&0xFF)<< 16 ) /* Set Wake-Up Filter 2 Byte Offset                    */
#define	SET_WF3_OFF(x) (((x)&0xFF)<< 24 ) /* Set Wake-Up Filter 3 Byte Offset                    */
/* Set ALL Offsets */
#define	SET_WF_OFFS(x0,x1,x2,x3) (SET_WF0_OFF((x0))|SET_WF1_OFF((x1))|SET_WF2_OFF((x2))|SET_WF3_OFF((x3)))

/* EMAC_WKUP_FFCRC0 Masks */

#define	WF0_CRC           0x0000FFFF    /* Wake-Up Filter 0 Pattern CRC                           */
#define	WF1_CRC           0xFFFF0000    /* Wake-Up Filter 1 Pattern CRC                           */

#define	SET_WF0_CRC(x) (((x)&0xFFFF)<<   0 ) /* Set Wake-Up Filter 0 Target CRC                   */
#define	SET_WF1_CRC(x) (((x)&0xFFFF)<<  16 ) /* Set Wake-Up Filter 1 Target CRC                   */

/* EMAC_WKUP_FFCRC1 Masks */

#define	WF2_CRC           0x0000FFFF    /* Wake-Up Filter 2 Pattern CRC                           */
#define	WF3_CRC           0xFFFF0000    /* Wake-Up Filter 3 Pattern CRC                           */

#define	SET_WF2_CRC(x) (((x)&0xFFFF)<<   0 ) /* Set Wake-Up Filter 2 Target CRC                   */
#define	SET_WF3_CRC(x) (((x)&0xFFFF)<<  16 ) /* Set Wake-Up Filter 3 Target CRC                   */

/* EMAC_SYSCTL Masks */

#define	PHYIE             0x00000001    /* PHY_INT Interrupt Enable                               */
#define	RXDWA             0x00000002    /* Receive Frame DMA Word Alignment (Odd/Even*)           */
#define	RXCKS             0x00000004    /* Enable RX Frame TCP/UDP Checksum Computation           */
#define	TXDWA             0x00000010    /* Transmit Frame DMA Word Alignment (Odd/Even*)          */
#define	MDCDIV            0x00003F00    /* SCLK:MDC Clock Divisor [MDC=SCLK/(2*(N+1))]            */

#define	SET_MDCDIV(x) (((x)&0x3F)<< 8)   /* Set MDC Clock Divisor                                 */

/* EMAC_SYSTAT Masks */

#define	PHYINT            0x00000001    /* PHY_INT Interrupt Status                               */
#define	MMCINT            0x00000002    /* MMC Counter Interrupt Status                           */
#define	RXFSINT           0x00000004    /* RX Frame-Status Interrupt Status                       */
#define	TXFSINT           0x00000008    /* TX Frame-Status Interrupt Status                       */
#define	WAKEDET           0x00000010    /* Wake-Up Detected Status                                */
#define	RXDMAERR          0x00000020    /* RX DMA Direction Error Status                          */
#define	TXDMAERR          0x00000040    /* TX DMA Direction Error Status                          */
#define	STMDONE           0x00000080    /* Station Mgt. Transfer Done Interrupt Status            */

/* EMAC_RX_STAT, EMAC_RX_STKY, and EMAC_RX_IRQE Masks */

#define	RX_FRLEN          0x000007FF    /* Frame Length In Bytes                                  */
#define	RX_COMP           0x00001000    /* RX Frame Complete                                      */
#define	RX_OK             0x00002000    /* RX Frame Received With No Errors                       */
#define	RX_LONG           0x00004000    /* RX Frame Too Long Error                                */
#define	RX_ALIGN          0x00008000    /* RX Frame Alignment Error                               */
#define	RX_CRC            0x00010000    /* RX Frame CRC Error                                     */
#define	RX_LEN            0x00020000    /* RX Frame Length Error                                  */
#define	RX_FRAG           0x00040000    /* RX Frame Fragment Error                                */
#define	RX_ADDR           0x00080000    /* RX Frame Address Filter Failed Error                   */
#define	RX_DMAO           0x00100000    /* RX Frame DMA Overrun Error                             */
#define	RX_PHY            0x00200000    /* RX Frame PHY Error                                     */
#define	RX_LATE           0x00400000    /* RX Frame Late Collision Error                          */
#define	RX_RANGE          0x00800000    /* RX Frame Length Field Out of Range Error               */
#define	RX_MULTI          0x01000000    /* RX Multicast Frame Indicator                           */
#define	RX_BROAD          0x02000000    /* RX Broadcast Frame Indicator                           */
#define	RX_CTL            0x04000000    /* RX Control Frame Indicator                             */
#define	RX_UCTL           0x08000000    /* Unsupported RX Control Frame Indicator                 */
#define	RX_TYPE           0x10000000    /* RX Typed Frame Indicator                               */
#define	RX_VLAN1          0x20000000    /* RX VLAN1 Frame Indicator                               */
#define	RX_VLAN2          0x40000000    /* RX VLAN2 Frame Indicator                               */
#define	RX_ACCEPT         0x80000000    /* RX Frame Accepted Indicator                            */

/*  EMAC_TX_STAT, EMAC_TX_STKY, and EMAC_TX_IRQE Masks  */

#define	TX_COMP           0x00000001    /* TX Frame Complete                                      */
#define	TX_OK             0x00000002    /* TX Frame Sent With No Errors                           */
#define	TX_ECOLL          0x00000004    /* TX Frame Excessive Collision Error                     */
#define	TX_LATE           0x00000008    /* TX Frame Late Collision Error                          */
#define	TX_DMAU           0x00000010    /* TX Frame DMA Underrun Error (STAT)                     */
#define	TX_MACE           0x00000010    /* Internal MAC Error Detected (STKY and IRQE)            */
#define	TX_EDEFER         0x00000020    /* TX Frame Excessive Deferral Error                      */
#define	TX_BROAD          0x00000040    /* TX Broadcast Frame Indicator                           */
#define	TX_MULTI          0x00000080    /* TX Multicast Frame Indicator                           */
#define	TX_CCNT           0x00000F00    /* TX Frame Collision Count                               */
#define	TX_DEFER          0x00001000    /* TX Frame Deferred Indicator                            */
#define	TX_CRS            0x00002000    /* TX Frame Carrier Sense Not Asserted Error              */
#define	TX_LOSS           0x00004000    /* TX Frame Carrier Lost During TX Error                  */
#define	TX_RETRY          0x00008000    /* TX Frame Successful After Retry                        */
#define	TX_FRLEN          0x07FF0000    /* TX Frame Length (Bytes)                                */

/* EMAC_MMC_CTL Masks */
#define	RSTC              0x00000001    /* Reset All Counters                                     */
#define	CROLL             0x00000002    /* Counter Roll-Over Enable                               */
#define	CCOR              0x00000004    /* Counter Clear-On-Read Mode Enable                      */
#define	MMCE              0x00000008    /* Enable MMC Counter Operation                           */

/* EMAC_MMC_RIRQS and EMAC_MMC_RIRQE Masks */
#define	RX_OK_CNT         0x00000001    /* RX Frames Received With No Errors                      */
#define	RX_FCS_CNT        0x00000002    /* RX Frames W/Frame Check Sequence Errors                */
#define	RX_ALIGN_CNT      0x00000004    /* RX Frames With Alignment Errors                        */
#define	RX_OCTET_CNT      0x00000008    /* RX Octets Received OK                                  */
#define	RX_LOST_CNT       0x00000010    /* RX Frames Lost Due To Internal MAC RX Error            */
#define	RX_UNI_CNT        0x00000020    /* Unicast RX Frames Received OK                          */
#define	RX_MULTI_CNT      0x00000040    /* Multicast RX Frames Received OK                        */
#define	RX_BROAD_CNT      0x00000080    /* Broadcast RX Frames Received OK                        */
#define	RX_IRL_CNT        0x00000100    /* RX Frames With In-Range Length Errors                  */
#define	RX_ORL_CNT        0x00000200    /* RX Frames With Out-Of-Range Length Errors              */
#define	RX_LONG_CNT       0x00000400    /* RX Frames With Frame Too Long Errors                   */
#define	RX_MACCTL_CNT     0x00000800    /* MAC Control RX Frames Received                         */
#define	RX_OPCODE_CTL     0x00001000    /* Unsupported Op-Code RX Frames Received                 */
#define	RX_PAUSE_CNT      0x00002000    /* PAUSEMAC Control RX Frames Received                    */
#define	RX_ALLF_CNT       0x00004000    /* All RX Frames Received                                 */
#define	RX_ALLO_CNT       0x00008000    /* All RX Octets Received                                 */
#define	RX_TYPED_CNT      0x00010000    /* Typed RX Frames Received                               */
#define	RX_SHORT_CNT      0x00020000    /* RX Frame Fragments (< 64 Bytes) Received               */
#define	RX_EQ64_CNT       0x00040000    /* 64-Byte RX Frames Received                             */
#define	RX_LT128_CNT      0x00080000    /* 65-127-Byte RX Frames Received                         */
#define	RX_LT256_CNT      0x00100000    /* 128-255-Byte RX Frames Received                        */
#define	RX_LT512_CNT      0x00200000    /* 256-511-Byte RX Frames Received                        */
#define	RX_LT1024_CNT     0x00400000    /* 512-1023-Byte RX Frames Received                       */
#define	RX_GE1024_CNT     0x00800000    /* 1024-Max-Byte RX Frames Received                       */

/* EMAC_MMC_TIRQS and EMAC_MMC_TIRQE Masks  */

#define	TX_OK_CNT         0x00000001    /* TX Frames Sent OK                                      */
#define	TX_SCOLL_CNT      0x00000002    /* TX Frames With Single Collisions                       */
#define	TX_MCOLL_CNT      0x00000004    /* TX Frames With Multiple Collisions                     */
#define	TX_OCTET_CNT      0x00000008    /* TX Octets Sent OK                                      */
#define	TX_DEFER_CNT      0x00000010    /* TX Frames With Deferred Transmission                   */
#define	TX_LATE_CNT       0x00000020    /* TX Frames With Late Collisions                         */
#define	TX_ABORTC_CNT     0x00000040    /* TX Frames Aborted Due To Excess Collisions             */
#define	TX_LOST_CNT       0x00000080    /* TX Frames Lost Due To Internal MAC TX Error            */
#define	TX_CRS_CNT        0x00000100    /* TX Frames With Carrier Sense Errors                    */
#define	TX_UNI_CNT        0x00000200    /* Unicast TX Frames Sent                                 */
#define	TX_MULTI_CNT      0x00000400    /* Multicast TX Frames Sent                               */
#define	TX_BROAD_CNT      0x00000800    /* Broadcast TX Frames Sent                               */
#define	TX_EXDEF_CTL      0x00001000    /* TX Frames With Excessive Deferral                      */
#define	TX_MACCTL_CNT     0x00002000    /* MAC Control TX Frames Sent                             */
#define	TX_ALLF_CNT       0x00004000    /* All TX Frames Sent                                     */
#define	TX_ALLO_CNT       0x00008000    /* All TX Octets Sent                                     */
#define	TX_EQ64_CNT       0x00010000    /* 64-Byte TX Frames Sent                                 */
#define	TX_LT128_CNT      0x00020000    /* 65-127-Byte TX Frames Sent                             */
#define	TX_LT256_CNT      0x00040000    /* 128-255-Byte TX Frames Sent                            */
#define	TX_LT512_CNT      0x00080000    /* 256-511-Byte TX Frames Sent                            */
#define	TX_LT1024_CNT     0x00100000    /* 512-1023-Byte TX Frames Sent                           */
#define	TX_GE1024_CNT     0x00200000    /* 1024-Max-Byte TX Frames Sent                           */
#define	TX_ABORT_CNT      0x00400000    /* TX Frames Aborted                                      */

/* SDH Registers */

#define SDH_PWR_CTL                    0xFFC03900 /* SDH Power Control */
#define SDH_CLK_CTL                    0xFFC03904 /* SDH Clock Control */
#define SDH_ARGUMENT                   0xFFC03908 /* SDH Argument */
#define SDH_COMMAND                    0xFFC0390C /* SDH Command */
#define SDH_RESP_CMD                   0xFFC03910 /* SDH Response Command */
#define SDH_RESPONSE0                  0xFFC03914 /* SDH Response0 */
#define SDH_RESPONSE1                  0xFFC03918 /* SDH Response1 */
#define SDH_RESPONSE2                  0xFFC0391C /* SDH Response2 */
#define SDH_RESPONSE3                  0xFFC03920 /* SDH Response3 */
#define SDH_DATA_TIMER                 0xFFC03924 /* SDH Data Timer */
#define SDH_DATA_LGTH                  0xFFC03928 /* SDH Data Length */
#define SDH_DATA_CTL                   0xFFC0392C /* SDH Data Control */
#define SDH_DATA_CNT                   0xFFC03930 /* SDH Data Counter */
#define SDH_STATUS                     0xFFC03934 /* SDH Status */
#define SDH_STATUS_CLR                 0xFFC03938 /* SDH Status Clear */
#define SDH_MASK0                      0xFFC0393C /* SDH Interrupt0 Mask */
#define SDH_MASK1                      0xFFC03940 /* SDH Interrupt1 Mask */
#define SDH_FIFO_CNT                   0xFFC03948 /* SDH FIFO Counter */
#define SDH_FIFO                       0xFFC03980 /* SDH Data FIFO */
#define SDH_E_STATUS                   0xFFC039C0 /* SDH Exception Status */
#define SDH_E_MASK                     0xFFC039C4 /* SDH Exception Mask */
#define SDH_CFG                        0xFFC039C8 /* SDH Configuration */
#define SDH_RD_WAIT_EN                 0xFFC039CC /* SDH Read Wait Enable */
#define SDH_PID0                       0xFFC039D0 /* SDH Peripheral Identification0 */
#define SDH_PID1                       0xFFC039D4 /* SDH Peripheral Identification1 */
#define SDH_PID2                       0xFFC039D8 /* SDH Peripheral Identification2 */
#define SDH_PID3                       0xFFC039DC /* SDH Peripheral Identification3 */
#define SDH_PID4                       0xFFC039E0 /* SDH Peripheral Identification4 */
#define SDH_PID5                       0xFFC039E4 /* SDH Peripheral Identification5 */
#define SDH_PID6                       0xFFC039E8 /* SDH Peripheral Identification6 */
#define SDH_PID7                       0xFFC039EC /* SDH Peripheral Identification7 */

/* Removable Storage Interface Registers */

#define RSI_PWR_CONTROL                0xFFC03800 /* RSI Power Control Register */
#define RSI_CLK_CONTROL                0xFFC03804 /* RSI Clock Control Register */
#define RSI_ARGUMENT                   0xFFC03808 /* RSI Argument Register */
#define RSI_COMMAND                    0xFFC0380C /* RSI Command Register */
#define RSI_RESP_CMD                   0xFFC03810 /* RSI Response Command Register */
#define RSI_RESPONSE0                  0xFFC03814 /* RSI Response Register */
#define RSI_RESPONSE1                  0xFFC03818 /* RSI Response Register */
#define RSI_RESPONSE2                  0xFFC0381C /* RSI Response Register */
#define RSI_RESPONSE3                  0xFFC03820 /* RSI Response Register */
#define RSI_DATA_TIMER                 0xFFC03824 /* RSI Data Timer Register */
#define RSI_DATA_LGTH                  0xFFC03828 /* RSI Data Length Register */
#define RSI_DATA_CONTROL               0xFFC0382C /* RSI Data Control Register */
#define RSI_DATA_CNT                   0xFFC03830 /* RSI Data Counter Register */
#define RSI_STATUS                     0xFFC03834 /* RSI Status Register */
#define RSI_STATUSCL                   0xFFC03838 /* RSI Status Clear Register */
#define RSI_MASK0                      0xFFC0383C /* RSI Interrupt 0 Mask Register */
#define RSI_MASK1                      0xFFC03840 /* RSI Interrupt 1 Mask Register */
#define RSI_FIFO_CNT                   0xFFC03848 /* RSI FIFO Counter Register */
#define RSI_CEATA_CONTROL              0xFFC0384C /* RSI CEATA Register */
#define RSI_FIFO                       0xFFC03880 /* RSI Data FIFO Register */
#define RSI_ESTAT                      0xFFC038C0 /* RSI Exception Status Register */
#define RSI_EMASK                      0xFFC038C4 /* RSI Exception Mask Register */
#define RSI_CONFIG                     0xFFC038C8 /* RSI Configuration Register */
#define RSI_RD_WAIT_EN                 0xFFC038CC /* RSI Read Wait Enable Register */
#define RSI_PID0                       0xFFC03FE0 /* RSI Peripheral ID Register 0 */
#define RSI_PID1                       0xFFC03FE4 /* RSI Peripheral ID Register 1 */
#define RSI_PID2                       0xFFC03FE8 /* RSI Peripheral ID Register 2 */
#define RSI_PID3                       0xFFC03FEC /* RSI Peripheral ID Register 3 */
#define RSI_PID4                       0xFFC03FF0 /* RSI Peripheral ID Register 4 */
#define RSI_PID5                       0xFFC03FF4 /* RSI Peripheral ID Register 5 */
#define RSI_PID6                       0xFFC03FF8 /* RSI Peripheral ID Register 6 */
#define RSI_PID7                       0xFFC03FFC /* RSI Peripheral ID Register 7 */

/* PTP TSYNC Registers */

#define EMAC_PTP_CTL                   0xFFC030A0 /* PTP Block Control */
#define EMAC_PTP_IE                    0xFFC030A4 /* PTP Block Interrupt Enable */
#define EMAC_PTP_ISTAT                 0xFFC030A8 /* PTP Block Interrupt Status */
#define EMAC_PTP_FOFF                  0xFFC030AC /* PTP Filter offset Register */
#define EMAC_PTP_FV1                   0xFFC030B0 /* PTP Filter Value Register 1 */
#define EMAC_PTP_FV2                   0xFFC030B4 /* PTP Filter Value Register 2 */
#define EMAC_PTP_FV3                   0xFFC030B8 /* PTP Filter Value Register 3 */
#define EMAC_PTP_ADDEND                0xFFC030BC /* PTP Addend for Frequency Compensation */
#define EMAC_PTP_ACCR                  0xFFC030C0 /* PTP Accumulator for Frequency Compensation */
#define EMAC_PTP_OFFSET                0xFFC030C4 /* PTP Time Offset Register */
#define EMAC_PTP_TIMELO                0xFFC030C8 /* PTP Precision Clock Time Low */
#define EMAC_PTP_TIMEHI                0xFFC030CC /* PTP Precision Clock Time High */
#define EMAC_PTP_RXSNAPLO              0xFFC030D0 /* PTP Receive Snapshot Register Low */
#define EMAC_PTP_RXSNAPHI              0xFFC030D4 /* PTP Receive Snapshot Register High */
#define EMAC_PTP_TXSNAPLO              0xFFC030D8 /* PTP Transmit Snapshot Register Low */
#define EMAC_PTP_TXSNAPHI              0xFFC030DC /* PTP Transmit Snapshot Register High */
#define EMAC_PTP_ALARMLO               0xFFC030E0 /* PTP Alarm time Low */
#define EMAC_PTP_ALARMHI               0xFFC030E4 /* PTP Alarm time High */
#define EMAC_PTP_ID_OFF                0xFFC030E8 /* PTP Capture ID offset register */
#define EMAC_PTP_ID_SNAP               0xFFC030EC /* PTP Capture ID register */
#define EMAC_PTP_PPS_STARTLO           0xFFC030F0 /* PPS Start Time Low */
#define EMAC_PTP_PPS_STARTHI           0xFFC030F4 /* PPS Start Time High */
#define EMAC_PTP_PPS_PERIOD            0xFFC030F8 /* PPS Count Register */

/* ********************************************************** */
/*     SINGLE BIT MACRO PAIRS (bit mask and negated one)      */
/*     and MULTI BIT READ MACROS                              */
/* ********************************************************** */

/* Bit masks for SDH_COMMAND */

#define                   CMD_IDX  0x3f       /* Command Index */
#define                   CMD_RSP  0x40       /* Response */
#define                 CMD_L_RSP  0x80       /* Long Response */
#define                 CMD_INT_E  0x100      /* Command Interrupt */
#define                CMD_PEND_E  0x200      /* Command Pending */
#define                     CMD_E  0x400      /* Command Enable */

/* Bit masks for SDH_PWR_CTL */

#define                    PWR_ON  0x3        /* Power On */
#if 0
#define                       TBD  0x3c       /* TBD */
#endif
#define                 SD_CMD_OD  0x40       /* Open Drain Output */
#define                   ROD_CTL  0x80       /* Rod Control */

/* Bit masks for SDH_CLK_CTL */

#define                    CLKDIV  0xff       /* MC_CLK Divisor */
#define                     CLK_E  0x100      /* MC_CLK Bus Clock Enable */
#define                  PWR_SV_E  0x200      /* Power Save Enable */
#define             CLKDIV_BYPASS  0x400      /* Bypass Divisor */
#define                  WIDE_BUS  0x800      /* Wide Bus Mode Enable */

/* Bit masks for SDH_RESP_CMD */

#define                  RESP_CMD  0x3f       /* Response Command */

/* Bit masks for SDH_DATA_CTL */

#define                     DTX_E  0x1        /* Data Transfer Enable */
#define                   DTX_DIR  0x2        /* Data Transfer Direction */
#define                  DTX_MODE  0x4        /* Data Transfer Mode */
#define                 DTX_DMA_E  0x8        /* Data Transfer DMA Enable */
#define              DTX_BLK_LGTH  0xf0       /* Data Transfer Block Length */

/* Bit masks for SDH_STATUS */

#define              CMD_CRC_FAIL  0x1        /* CMD CRC Fail */
#define              DAT_CRC_FAIL  0x2        /* Data CRC Fail */
#define               CMD_TIME_OUT  0x4        /* CMD Time Out */
#define               DAT_TIME_OUT  0x8        /* Data Time Out */
#define               TX_UNDERRUN  0x10       /* Transmit Underrun */
#define                RX_OVERRUN  0x20       /* Receive Overrun */
#define              CMD_RESP_END  0x40       /* CMD Response End */
#define                  CMD_SENT  0x80       /* CMD Sent */
#define                   DAT_END  0x100      /* Data End */
#define             START_BIT_ERR  0x200      /* Start Bit Error */
#define               DAT_BLK_END  0x400      /* Data Block End */
#define                   CMD_ACT  0x800      /* CMD Active */
#define                    TX_ACT  0x1000     /* Transmit Active */
#define                    RX_ACT  0x2000     /* Receive Active */
#define              TX_FIFO_STAT  0x4000     /* Transmit FIFO Status */
#define              RX_FIFO_STAT  0x8000     /* Receive FIFO Status */
#define              TX_FIFO_FULL  0x10000    /* Transmit FIFO Full */
#define              RX_FIFO_FULL  0x20000    /* Receive FIFO Full */
#define              TX_FIFO_ZERO  0x40000    /* Transmit FIFO Empty */
#define               RX_DAT_ZERO  0x80000    /* Receive FIFO Empty */
#define                TX_DAT_RDY  0x100000   /* Transmit Data Available */
#define               RX_FIFO_RDY  0x200000   /* Receive Data Available */

/* Bit masks for SDH_STATUS_CLR */

#define         CMD_CRC_FAIL_STAT  0x1        /* CMD CRC Fail Status */
#define         DAT_CRC_FAIL_STAT  0x2        /* Data CRC Fail Status */
#define          CMD_TIMEOUT_STAT  0x4        /* CMD Time Out Status */
#define          DAT_TIMEOUT_STAT  0x8        /* Data Time Out status */
#define          TX_UNDERRUN_STAT  0x10       /* Transmit Underrun Status */
#define           RX_OVERRUN_STAT  0x20       /* Receive Overrun Status */
#define         CMD_RESP_END_STAT  0x40       /* CMD Response End Status */
#define             CMD_SENT_STAT  0x80       /* CMD Sent Status */
#define              DAT_END_STAT  0x100      /* Data End Status */
#define        START_BIT_ERR_STAT  0x200      /* Start Bit Error Status */
#define          DAT_BLK_END_STAT  0x400      /* Data Block End Status */

/* Bit masks for SDH_MASK0 */

#define         CMD_CRC_FAIL_MASK  0x1        /* CMD CRC Fail Mask */
#define         DAT_CRC_FAIL_MASK  0x2        /* Data CRC Fail Mask */
#define          CMD_TIMEOUT_MASK  0x4        /* CMD Time Out Mask */
#define          DAT_TIMEOUT_MASK  0x8        /* Data Time Out Mask */
#define          TX_UNDERRUN_MASK  0x10       /* Transmit Underrun Mask */
#define           RX_OVERRUN_MASK  0x20       /* Receive Overrun Mask */
#define         CMD_RESP_END_MASK  0x40       /* CMD Response End Mask */
#define             CMD_SENT_MASK  0x80       /* CMD Sent Mask */
#define              DAT_END_MASK  0x100      /* Data End Mask */
#define        START_BIT_ERR_MASK  0x200      /* Start Bit Error Mask */
#define          DAT_BLK_END_MASK  0x400      /* Data Block End Mask */
#define              CMD_ACT_MASK  0x800      /* CMD Active Mask */
#define               TX_ACT_MASK  0x1000     /* Transmit Active Mask */
#define               RX_ACT_MASK  0x2000     /* Receive Active Mask */
#define         TX_FIFO_STAT_MASK  0x4000     /* Transmit FIFO Status Mask */
#define         RX_FIFO_STAT_MASK  0x8000     /* Receive FIFO Status Mask */
#define         TX_FIFO_FULL_MASK  0x10000    /* Transmit FIFO Full Mask */
#define         RX_FIFO_FULL_MASK  0x20000    /* Receive FIFO Full Mask */
#define         TX_FIFO_ZERO_MASK  0x40000    /* Transmit FIFO Empty Mask */
#define          RX_DAT_ZERO_MASK  0x80000    /* Receive FIFO Empty Mask */
#define           TX_DAT_RDY_MASK  0x100000   /* Transmit Data Available Mask */
#define          RX_FIFO_RDY_MASK  0x200000   /* Receive Data Available Mask */

/* Bit masks for SDH_FIFO_CNT */

#define                FIFO_COUNT  0x7fff     /* FIFO Count */

/* Bit masks for SDH_E_STATUS */

#define              SDIO_INT_DET  0x2        /* SDIO Int Detected */
#define               SD_CARD_DET  0x10       /* SD Card Detect */

/* Bit masks for SDH_E_MASK */

#define                  SDIO_MSK  0x2        /* Mask SDIO Int Detected */
#define                   SCD_MSK  0x40       /* Mask Card Detect */

/* Bit masks for SDH_CFG */

#define                   CLKS_EN  0x1        /* Clocks Enable */
#define                      SD4E  0x4        /* SDIO 4-Bit Enable */
#define                       MWE  0x8        /* Moving Window Enable */
#define                    SD_RST  0x10       /* SDMMC Reset */
#define                 PUP_SDDAT  0x20       /* Pull-up SD_DAT */
#define                PUP_SDDAT3  0x40       /* Pull-up SD_DAT3 */
#define                 PD_SDDAT3  0x80       /* Pull-down SD_DAT3 */

/* Bit masks for SDH_RD_WAIT_EN */

#define                       RWR  0x1        /* Read Wait Request */

/* Bit masks for EMAC_PTP_CTL */

#define                    PTP_EN  0x1        /* Enable the PTP_TSYNC module */
#define                        TL  0x2        /* Timestamp lock control */
#define                      ASEN  0x10       /* Auxiliary snapshot control */
#define                     PPSEN  0x80       /* Pulse-per-second (PPS) control */
#define                     CKOEN  0x2000     /* Clock output control */

/* Bit masks for EMAC_PTP_IE */

#define                      ALIE  0x1        /* Alarm interrupt enable */
#define                     RXEIE  0x2        /* Receive event interrupt enable */
#define                     RXGIE  0x4        /* Receive general interrupt enable */
#define                      TXIE  0x8        /* Transmit interrupt enable */
#define                     RXOVE  0x10       /* Receive overrun error interrupt enable */
#define                     TXOVE  0x20       /* Transmit overrun error interrupt enable */
#define                      ASIE  0x40       /* Auxiliary snapshot interrupt enable */

/* Bit masks for EMAC_PTP_ISTAT */

#define                       ALS  0x1        /* Alarm status */
#define                      RXEL  0x2        /* Receive event interrupt status */
#define                      RXGL  0x4        /* Receive general interrupt status */
#define                      TXTL  0x8        /* Transmit snapshot status */
#define                      RXOV  0x10       /* Receive snapshot overrun status */
#define                      TXOV  0x20       /* Transmit snapshot overrun status */
#define                       ASL  0x40       /* Auxiliary snapshot interrupt status */

#endif /* _DEF_BF518_H */
