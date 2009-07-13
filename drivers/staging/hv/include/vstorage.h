/*
 *
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 *
 */


#pragma once

//#include <vmbuspacketformat.h>
//#include <ntddscsi.h>

#define C_ASSERT(x)
//
//  public interface to the server
//

//
//  Storvsp device interface guid
//


//
//  Protocol versions.
//

//
// vstorage.w revision number.  This is used in the case of a version match,
// to alert the user that structure sizes may be mismatched even though the
// protocol versions match.
//

#define REVISION_STRING(REVISION_) #REVISION_
#define FILL_VMSTOR_REVISION(RESULT_LVALUE_)                     \
{                                                                \
    char *revisionString = REVISION_STRING($Revision: 6 $) + 11; \
    RESULT_LVALUE_ = 0;                                          \
    while (*revisionString >= '0' && *revisionString <= '9')     \
    {                                                            \
        RESULT_LVALUE_ *= 10;                                    \
        RESULT_LVALUE_ += *revisionString - '0';                 \
        revisionString++;                                        \
    }                                                            \
}

//
// Major/minor macros.  Minor version is in LSB, meaning that earlier flat
// version numbers will be interpreted as "0.x" (i.e., 1 becomes 0.1).
//

#define VMSTOR_PROTOCOL_MAJOR(VERSION_)         (((VERSION_) >> 8) & 0xff)
#define VMSTOR_PROTOCOL_MINOR(VERSION_)         (((VERSION_)     ) & 0xff)
#define VMSTOR_PROTOCOL_VERSION(MAJOR_, MINOR_) ((((MAJOR_) & 0xff) << 8) | \
                                                 (((MINOR_) & 0xff)     ))

//
// Invalid version.
//

#define VMSTOR_INVALID_PROTOCOL_VERSION  -1

//
// Version history:
// V1 Beta                    0.1
// V1 RC < 2008/1/31          1.0
// V1 RC > 2008/1/31          2.0
//
#define VMSTOR_PROTOCOL_VERSION_CURRENT VMSTOR_PROTOCOL_VERSION(2, 0)


//
//  This will get replaced with the max transfer length that is possible on
//  the host adapter.
//  The max transfer length will be published when we offer a vmbus channel.
//

#define MAX_TRANSFER_LENGTH 0x40000
#define DEFAULT_PACKET_SIZE (sizeof(VMDATA_GPA_DIRECT) +                            \
                             sizeof(VSTOR_PACKET) +                                 \
                             (sizeof(UINT64) * (MAX_TRANSFER_LENGTH / PAGE_SIZE)))



//
//  Packet structure describing virtual storage requests.
//

typedef enum
{
    VStorOperationCompleteIo            = 1,
    VStorOperationRemoveDevice          = 2,
    VStorOperationExecuteSRB            = 3,
    VStorOperationResetLun              = 4,
    VStorOperationResetAdapter          = 5,
    VStorOperationResetBus              = 6,
    VStorOperationBeginInitialization   = 7,
    VStorOperationEndInitialization     = 8,
    VStorOperationQueryProtocolVersion  = 9,
    VStorOperationQueryProperties       = 10,
    VStorOperationMaximum               = 10

} VSTOR_PACKET_OPERATION;


//
//  Platform neutral description of a scsi request -
//  this remains the same across the write regardless of 32/64 bit
//  note: it's patterned off the SCSI_PASS_THROUGH structure
//


#pragma pack(push,1)


#define CDB16GENERIC_LENGTH 0x10

#ifndef SENSE_BUFFER_SIZE
#define SENSE_BUFFER_SIZE 0x12
#endif
C_ASSERT(SENSE_BUFFER_SIZE == 0x12);

#define MAX_DATA_BUFFER_LENGTH_WITH_PADDING 0x14


typedef struct
{
    USHORT Length;
    UCHAR SrbStatus;
    UCHAR ScsiStatus;

    UCHAR PortNumber;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;

    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    UCHAR Reserved;

    ULONG DataTransferLength;

    union
    {
        UCHAR Cdb[CDB16GENERIC_LENGTH];

        UCHAR SenseData[SENSE_BUFFER_SIZE];

        UCHAR ReservedArray[MAX_DATA_BUFFER_LENGTH_WITH_PADDING];
    };

} VMSCSI_REQUEST, *PVMSCSI_REQUEST;

C_ASSERT((sizeof(VMSCSI_REQUEST) % 4) == 0);


//
//  This structure is sent during the intialization phase to get the different
//  properties of the channel.
//

typedef struct
{
    USHORT ProtocolVersion;
    UCHAR  PathId;
    UCHAR  TargetId;

    //
    // Note: port number is only really known on the client side
    //
    ULONG  PortNumber;

    ULONG  Flags;

    ULONG  MaxTransferBytes;

    //
    //  This id is unique for each channel and will correspond with
    //  vendor specific data in the inquirydata
    //

    ULONGLONG UniqueId;

} VMSTORAGE_CHANNEL_PROPERTIES, *PVMSTORAGE_CHANNEL_PROPERTIES;

C_ASSERT((sizeof(VMSTORAGE_CHANNEL_PROPERTIES) % 4) == 0);


//
//  This structure is sent during the storage protocol negotiations.
//

typedef struct
{
    //
    // Major (MSW) and minor (LSW) version numbers.
    //

    USHORT MajorMinor;


    //
    // Revision number is auto-incremented whenever this file is changed
    // (See FILL_VMSTOR_REVISION macro above).  Mismatch does not definitely
    // indicate incompatibility--but it does indicate mismatched builds.
    //

    USHORT Revision;

} VMSTORAGE_PROTOCOL_VERSION, *PVMSTORAGE_PROTOCOL_VERSION;

C_ASSERT((sizeof(VMSTORAGE_PROTOCOL_VERSION) % 4) == 0);


//
// Channel Property Flags
//

#define STORAGE_CHANNEL_REMOVABLE_FLAG                  0x1
#define STORAGE_CHANNEL_EMULATED_IDE_FLAG               0x2


typedef struct _VSTOR_PACKET
{
    //
    // Requested operation type
    //

    VSTOR_PACKET_OPERATION Operation;

    //
    //  Flags - see below for values
    //

    ULONG     Flags;

    //
    // Status of the request returned from the server side.
    //

    ULONG     Status;

    //
    // Data payload area
    //

    union
    {
        //
        //  Structure used to forward SCSI commands from the client to the server.
        //

        VMSCSI_REQUEST VmSrb;

        //
        // Structure used to query channel properties.
        //

        VMSTORAGE_CHANNEL_PROPERTIES StorageChannelProperties;

        //
        // Used during version negotiations.
        //

        VMSTORAGE_PROTOCOL_VERSION Version;
    };

} VSTOR_PACKET, *PVSTOR_PACKET;

C_ASSERT((sizeof(VSTOR_PACKET) % 4) == 0);

//
//  Packet flags
//

//
//  This flag indicates that the server should send back a completion for this
//  packet.
//

#define REQUEST_COMPLETION_FLAG 0x1

//
//  This is the set of flags that the vsc can set in any packets it sends
//

#define VSC_LEGAL_FLAGS (REQUEST_COMPLETION_FLAG)


#pragma pack(pop)


