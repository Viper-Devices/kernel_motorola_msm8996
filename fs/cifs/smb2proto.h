/*
 *   fs/cifs/smb2proto.h
 *
 *   Copyright (c) International Business Machines  Corp., 2002, 2011
 *                 Etersoft, 2012
 *   Author(s): Steve French (sfrench@us.ibm.com)
 *              Pavel Shilovsky (pshilovsky@samba.org) 2012
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
#ifndef _SMB2PROTO_H
#define _SMB2PROTO_H
#include <linux/nls.h>
#include <linux/key-type.h>

struct statfs;

/*
 *****************************************************************
 * All Prototypes
 *****************************************************************
 */
extern int map_smb2_to_linux_error(char *buf, bool log_err);
extern int smb2_check_message(char *buf, unsigned int length);
extern unsigned int smb2_calc_size(void *buf);
extern char *smb2_get_data_area_len(int *off, int *len, struct smb2_hdr *hdr);
extern __le16 *cifs_convert_path_to_utf16(const char *from,
					  struct cifs_sb_info *cifs_sb);

extern int smb2_verify_signature2(struct kvec *, unsigned int,
				  struct TCP_Server_Info *);
extern int smb2_check_receive(struct mid_q_entry *mid,
			      struct TCP_Server_Info *server, bool log_error);
extern int smb2_setup_request(struct cifs_ses *ses, struct kvec *iov,
			      unsigned int nvec, struct mid_q_entry **ret_mid);
extern int smb2_setup_async_request(struct TCP_Server_Info *server,
				    struct kvec *iov, unsigned int nvec,
				    struct mid_q_entry **ret_mid);
extern void smb2_echo_request(struct work_struct *work);
extern bool smb2_is_valid_oplock_break(char *buffer,
				       struct TCP_Server_Info *srv);

extern void move_smb2_info_to_cifs(FILE_ALL_INFO *dst,
				   struct smb2_file_all_info *src);
extern int smb2_query_path_info(const unsigned int xid, struct cifs_tcon *tcon,
				struct cifs_sb_info *cifs_sb,
				const char *full_path, FILE_ALL_INFO *data,
				bool *adjust_tz);
extern int smb2_set_path_size(const unsigned int xid, struct cifs_tcon *tcon,
			      const char *full_path, __u64 size,
			      struct cifs_sb_info *cifs_sb, bool set_alloc);
extern int smb2_set_file_info(struct inode *inode, const char *full_path,
			      FILE_BASIC_INFO *buf, const unsigned int xid);
extern int smb2_mkdir(const unsigned int xid, struct cifs_tcon *tcon,
		      const char *name, struct cifs_sb_info *cifs_sb);
extern void smb2_mkdir_setinfo(struct inode *inode, const char *full_path,
			       struct cifs_sb_info *cifs_sb,
			       struct cifs_tcon *tcon, const unsigned int xid);
extern int smb2_rmdir(const unsigned int xid, struct cifs_tcon *tcon,
		      const char *name, struct cifs_sb_info *cifs_sb);
extern int smb2_unlink(const unsigned int xid, struct cifs_tcon *tcon,
		       const char *name, struct cifs_sb_info *cifs_sb);
extern int smb2_rename_path(const unsigned int xid, struct cifs_tcon *tcon,
			    const char *from_name, const char *to_name,
			    struct cifs_sb_info *cifs_sb);
extern int smb2_create_hardlink(const unsigned int xid, struct cifs_tcon *tcon,
				const char *from_name, const char *to_name,
				struct cifs_sb_info *cifs_sb);

extern int smb2_open_file(const unsigned int xid, struct cifs_tcon *tcon,
			  const char *full_path, int disposition,
			  int desired_access, int create_options,
			  struct cifs_fid *fid, __u32 *oplock,
			  FILE_ALL_INFO *buf, struct cifs_sb_info *cifs_sb);
extern void smb2_set_oplock_level(struct cifsInodeInfo *cinode, __u32 oplock);

/*
 * SMB2 Worker functions - most of protocol specific implementation details
 * are contained within these calls.
 */
extern int SMB2_negotiate(const unsigned int xid, struct cifs_ses *ses);
extern int SMB2_sess_setup(const unsigned int xid, struct cifs_ses *ses,
			   const struct nls_table *nls_cp);
extern int SMB2_logoff(const unsigned int xid, struct cifs_ses *ses);
extern int SMB2_tcon(const unsigned int xid, struct cifs_ses *ses,
		     const char *tree, struct cifs_tcon *tcon,
		     const struct nls_table *);
extern int SMB2_tdis(const unsigned int xid, struct cifs_tcon *tcon);
extern int SMB2_open(const unsigned int xid, struct cifs_tcon *tcon,
		     __le16 *path, u64 *persistent_fid, u64 *volatile_fid,
		     __u32 desired_access, __u32 create_disposition,
		     __u32 file_attributes, __u32 create_options,
		     __u8 *oplock, struct smb2_file_all_info *buf);
extern int SMB2_close(const unsigned int xid, struct cifs_tcon *tcon,
		      u64 persistent_file_id, u64 volatile_file_id);
extern int SMB2_flush(const unsigned int xid, struct cifs_tcon *tcon,
		      u64 persistent_file_id, u64 volatile_file_id);
extern int SMB2_query_info(const unsigned int xid, struct cifs_tcon *tcon,
			   u64 persistent_file_id, u64 volatile_file_id,
			   struct smb2_file_all_info *data);
extern int SMB2_get_srv_num(const unsigned int xid, struct cifs_tcon *tcon,
			    u64 persistent_fid, u64 volatile_fid,
			    __le64 *uniqueid);
extern int smb2_async_readv(struct cifs_readdata *rdata);
extern int SMB2_read(const unsigned int xid, struct cifs_io_parms *io_parms,
		     unsigned int *nbytes, char **buf, int *buf_type);
extern int smb2_async_writev(struct cifs_writedata *wdata);
extern int SMB2_write(const unsigned int xid, struct cifs_io_parms *io_parms,
		      unsigned int *nbytes, struct kvec *iov, int n_vec);
extern int SMB2_echo(struct TCP_Server_Info *server);
extern int SMB2_query_directory(const unsigned int xid, struct cifs_tcon *tcon,
				u64 persistent_fid, u64 volatile_fid, int index,
				struct cifs_search_info *srch_inf);
extern int SMB2_rename(const unsigned int xid, struct cifs_tcon *tcon,
		       u64 persistent_fid, u64 volatile_fid,
		       __le16 *target_file);
extern int SMB2_set_hardlink(const unsigned int xid, struct cifs_tcon *tcon,
			     u64 persistent_fid, u64 volatile_fid,
			     __le16 *target_file);
extern int SMB2_set_eof(const unsigned int xid, struct cifs_tcon *tcon,
			u64 persistent_fid, u64 volatile_fid, u32 pid,
			__le64 *eof);
extern int SMB2_set_info(const unsigned int xid, struct cifs_tcon *tcon,
			 u64 persistent_fid, u64 volatile_fid,
			 FILE_BASIC_INFO *buf);
extern int SMB2_oplock_break(const unsigned int xid, struct cifs_tcon *tcon,
			     const u64 persistent_fid, const u64 volatile_fid,
			     const __u8 oplock_level);

#endif			/* _SMB2PROTO_H */
