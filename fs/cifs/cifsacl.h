/*
 *   fs/cifs/cifsacl.h
 *
 *   Copyright (c) International Business Machines  Corp., 2007
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _CIFSACL_H
#define _CIFSACL_H


#define NUM_AUTHS 6 /* number of authority fields */
#define SID_MAX_SUB_AUTHORITIES (15) /* max number of sub authority fields */
#define NUM_WK_SIDS 7 /* number of well known sids */
#define SIDNAMELENGTH 20 /* long enough for the ones we care about */
#define DEFSECDESCLEN 192 /* sec desc len contaiting a dacl with three aces */

#define READ_BIT        0x4
#define WRITE_BIT       0x2
#define EXEC_BIT        0x1

#define UBITSHIFT	6
#define GBITSHIFT	3

#define ACCESS_ALLOWED	0
#define ACCESS_DENIED	1

#define SIDOWNER 1
#define SIDGROUP 2

/*
 * Maximum size of a string representation of a SID:
 *
 * The fields are unsigned values in decimal. So:
 *
 * u8:  max 3 bytes in decimal
 * u32: max 10 bytes in decimal
 *
 * "S-" + 3 bytes for version field + 4 bytes for each authority field (3 bytes
 * per number + 1 for '-') + 11 bytes for each subauthority field (10 bytes
 * per number + 1 for '-') + NULL terminator.
 */
#define SID_STRING_MAX (195)

#define SID_ID_MAPPED 0
#define SID_ID_PENDING 1
#define SID_MAP_EXPIRE (3600 * HZ) /* map entry expires after one hour */
#define SID_MAP_RETRY (300 * HZ)   /* wait 5 minutes for next attempt to map */

struct cifs_ntsd {
	__le16 revision; /* revision level */
	__le16 type;
	__le32 osidoffset;
	__le32 gsidoffset;
	__le32 sacloffset;
	__le32 dacloffset;
} __attribute__((packed));

struct cifs_sid {
	__u8 revision; /* revision level */
	__u8 num_subauth;
	__u8 authority[NUM_AUTHS];
	__le32 sub_auth[SID_MAX_SUB_AUTHORITIES]; /* sub_auth[num_subauth] */
} __attribute__((packed));

/* size of a struct cifs_sid, sans sub_auth array */
#define CIFS_SID_BASE_SIZE (1 + 1 + NUM_AUTHS)

struct cifs_acl {
	__le16 revision; /* revision level */
	__le16 size;
	__le32 num_aces;
} __attribute__((packed));

struct cifs_ace {
	__u8 type;
	__u8 flags;
	__le16 size;
	__le32 access_req;
	struct cifs_sid sid; /* ie UUID of user or group who gets these perms */
} __attribute__((packed));

struct cifs_wksid {
	struct cifs_sid cifssid;
	char sidname[SIDNAMELENGTH];
} __attribute__((packed));

struct cifs_sid_id {
	unsigned int refcount; /* increment with spinlock, decrement without */
	unsigned long id;
	unsigned long time;
	unsigned long state;
	char *sidstr;
	struct rb_node rbnode;
	struct cifs_sid sid;
};

#endif /* _CIFSACL_H */
