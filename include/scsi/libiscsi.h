/*
 * iSCSI lib definitions
 *
 * Copyright (C) 2006 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2004 - 2006 Mike Christie
 * Copyright (C) 2004 - 2005 Dmitry Yusupov
 * Copyright (C) 2004 - 2005 Alex Aizman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef LIBISCSI_H
#define LIBISCSI_H

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <scsi/iscsi_proto.h>
#include <scsi/iscsi_if.h>
#include <scsi/scsi_transport_iscsi.h>

struct scsi_transport_template;
struct scsi_host_template;
struct scsi_device;
struct Scsi_Host;
struct scsi_target;
struct scsi_cmnd;
struct socket;
struct iscsi_transport;
struct iscsi_cls_session;
struct iscsi_cls_conn;
struct iscsi_session;
struct iscsi_nopin;
struct device;

#define ISCSI_DEF_XMIT_CMDS_MAX	128	/* must be power of 2 */
#define ISCSI_MGMT_CMDS_MAX	15

#define ISCSI_DEF_CMD_PER_LUN	32

/* Task Mgmt states */
enum {
	TMF_INITIAL,
	TMF_QUEUED,
	TMF_SUCCESS,
	TMF_FAILED,
	TMF_TIMEDOUT,
	TMF_NOT_FOUND,
};

/* Connection suspend "bit" */
#define ISCSI_SUSPEND_BIT		1

#define ISCSI_ITT_MASK			0x1fff
#define ISCSI_TOTAL_CMDS_MAX		4096
/* this must be a power of two greater than ISCSI_MGMT_CMDS_MAX */
#define ISCSI_TOTAL_CMDS_MIN		16
#define ISCSI_AGE_SHIFT			28
#define ISCSI_AGE_MASK			0xf

#define ISCSI_ADDRESS_BUF_LEN		64

enum {
	/* this is the maximum possible storage for AHSs */
	ISCSI_MAX_AHS_SIZE = sizeof(struct iscsi_ecdb_ahdr) +
				sizeof(struct iscsi_rlength_ahdr),
	ISCSI_DIGEST_SIZE = sizeof(__u32),
};


enum {
	ISCSI_TASK_FREE,
	ISCSI_TASK_COMPLETED,
	ISCSI_TASK_PENDING,
	ISCSI_TASK_RUNNING,
};

struct iscsi_r2t_info {
	__be32			ttt;		/* copied from R2T */
	__be32			exp_statsn;	/* copied from R2T */
	uint32_t		data_length;	/* copied from R2T */
	uint32_t		data_offset;	/* copied from R2T */
	int			data_count;	/* DATA-Out payload progress */
	int			datasn;
	/* LLDs should set/update these values */
	int			sent;		/* R2T sequence progress */
};

struct iscsi_task {
	/*
	 * Because LLDs allocate their hdr differently, this is a pointer
	 * and length to that storage. It must be setup at session
	 * creation time.
	 */
	struct iscsi_hdr	*hdr;
	unsigned short		hdr_max;
	unsigned short		hdr_len;	/* accumulated size of hdr used */
	/* copied values in case we need to send tmfs */
	itt_t			hdr_itt;
	__be32			cmdsn;
	uint8_t			lun[8];

	int			itt;		/* this ITT */

	unsigned		imm_count;	/* imm-data (bytes)   */
	/* offset in unsolicited stream (bytes); */
	struct iscsi_r2t_info	unsol_r2t;
	char			*data;		/* mgmt payload */
	unsigned		data_count;
	struct scsi_cmnd	*sc;		/* associated SCSI cmd*/
	struct iscsi_conn	*conn;		/* used connection    */

	/* state set/tested under session->lock */
	int			state;
	atomic_t		refcount;
	struct list_head	running;	/* running cmd list */
	void			*dd_data;	/* driver/transport data */
};

static inline int iscsi_task_has_unsol_data(struct iscsi_task *task)
{
	return task->unsol_r2t.data_length > task->unsol_r2t.sent;
}

static inline void* iscsi_next_hdr(struct iscsi_task *task)
{
	return (void*)task->hdr + task->hdr_len;
}

/* Connection's states */
enum {
	ISCSI_CONN_INITIAL_STAGE,
	ISCSI_CONN_STARTED,
	ISCSI_CONN_STOPPED,
	ISCSI_CONN_CLEANUP_WAIT,
};

struct iscsi_conn {
	struct iscsi_cls_conn	*cls_conn;	/* ptr to class connection */
	void			*dd_data;	/* iscsi_transport data */
	struct iscsi_session	*session;	/* parent session */
	/*
	 * conn_stop() flag: stop to recover, stop to terminate
	 */
        int			stop_stage;
	struct timer_list	transport_timer;
	unsigned long		last_recv;
	unsigned long		last_ping;
	int			ping_timeout;
	int			recv_timeout;
	struct iscsi_task 	*ping_task;

	/* iSCSI connection-wide sequencing */
	uint32_t		exp_statsn;

	/* control data */
	int			id;		/* CID */
	int			c_stage;	/* connection state */
	/*
	 * Preallocated buffer for pdus that have data but do not
	 * originate from scsi-ml. We never have two pdus using the
	 * buffer at the same time. It is only allocated to
	 * the default max recv size because the pdus we support
	 * should always fit in this buffer
	 */
	char			*data;
	struct iscsi_task 	*login_task;	/* mtask used for login/text */
	struct iscsi_task	*task;		/* xmit task in progress */

	/* xmit */
	struct list_head	mgmtqueue;	/* mgmt (control) xmit queue */
	struct list_head	cmdqueue;	/* data-path cmd queue */
	struct list_head	requeue;	/* tasks needing another run */
	struct work_struct	xmitwork;	/* per-conn. xmit workqueue */
	unsigned long		suspend_tx;	/* suspend Tx */
	unsigned long		suspend_rx;	/* suspend Rx */

	/* abort */
	wait_queue_head_t	ehwait;		/* used in eh_abort() */
	struct iscsi_tm		tmhdr;
	struct timer_list	tmf_timer;
	int			tmf_state;	/* see TMF_INITIAL, etc.*/

	/* negotiated params */
	unsigned		max_recv_dlength; /* initiator_max_recv_dsl*/
	unsigned		max_xmit_dlength; /* target_max_recv_dsl */
	int			hdrdgst_en;
	int			datadgst_en;
	int			ifmarker_en;
	int			ofmarker_en;
	/* values userspace uses to id a conn */
	int			persistent_port;
	char			*persistent_address;
	/* remote portal currently connected to */
	int			portal_port;
	char			portal_address[ISCSI_ADDRESS_BUF_LEN];

	/* MIB-statistics */
	uint64_t		txdata_octets;
	uint64_t		rxdata_octets;
	uint32_t		scsicmd_pdus_cnt;
	uint32_t		dataout_pdus_cnt;
	uint32_t		scsirsp_pdus_cnt;
	uint32_t		datain_pdus_cnt;
	uint32_t		r2t_pdus_cnt;
	uint32_t		tmfcmd_pdus_cnt;
	int32_t			tmfrsp_pdus_cnt;

	/* custom statistics */
	uint32_t		eh_abort_cnt;
	uint32_t		fmr_unalign_cnt;
};

struct iscsi_pool {
	struct kfifo		*queue;		/* FIFO Queue */
	void			**pool;		/* Pool of elements */
	int			max;		/* Max number of elements */
};

/* Session's states */
enum {
	ISCSI_STATE_FREE = 1,
	ISCSI_STATE_LOGGED_IN,
	ISCSI_STATE_FAILED,
	ISCSI_STATE_TERMINATE,
	ISCSI_STATE_IN_RECOVERY,
	ISCSI_STATE_RECOVERY_FAILED,
	ISCSI_STATE_LOGGING_OUT,
};

struct iscsi_session {
	struct iscsi_cls_session *cls_session;
	/*
	 * Syncs up the scsi eh thread with the iscsi eh thread when sending
	 * task management functions. This must be taken before the session
	 * and recv lock.
	 */
	struct mutex		eh_mutex;

	/* iSCSI session-wide sequencing */
	uint32_t		cmdsn;
	uint32_t		exp_cmdsn;
	uint32_t		max_cmdsn;

	/* This tracks the reqs queued into the initiator */
	uint32_t		queued_cmdsn;

	/* configuration */
	int			abort_timeout;
	int			lu_reset_timeout;
	int			initial_r2t_en;
	unsigned		max_r2t;
	int			imm_data_en;
	unsigned		first_burst;
	unsigned		max_burst;
	int			time2wait;
	int			time2retain;
	int			pdu_inorder_en;
	int			dataseq_inorder_en;
	int			erl;
	int			fast_abort;
	int			tpgt;
	char			*username;
	char			*username_in;
	char			*password;
	char			*password_in;
	char			*targetname;
	char			*ifacename;
	char			*initiatorname;
	/* control data */
	struct iscsi_transport	*tt;
	struct Scsi_Host	*host;
	struct iscsi_conn	*leadconn;	/* leading connection */
	spinlock_t		lock;		/* protects session state, *
						 * sequence numbers,       *
						 * session resources:      *
						 * - cmdpool,		   *
						 * - mgmtpool,		   *
						 * - r2tpool		   */
	int			state;		/* session state           */
	int			age;		/* counts session re-opens */

	int			scsi_cmds_max; 	/* max scsi commands */
	int			cmds_max;	/* size of cmds array */
	struct iscsi_task	**cmds;		/* Original Cmds arr */
	struct iscsi_pool	cmdpool;	/* PDU's pool */
};

enum {
	ISCSI_HOST_SETUP,
	ISCSI_HOST_REMOVED,
};

struct iscsi_host {
	char			*initiatorname;
	/* hw address or netdev iscsi connection is bound to */
	char			*hwaddress;
	char			*netdev;
	/* local address */
	int			local_port;
	char			local_address[ISCSI_ADDRESS_BUF_LEN];

	wait_queue_head_t	session_removal_wq;
	/* protects sessions and state */
	spinlock_t		lock;
	int			num_sessions;
	int			state;

	struct workqueue_struct	*workq;
	char			workq_name[20];
};

/*
 * scsi host template
 */
extern int iscsi_change_queue_depth(struct scsi_device *sdev, int depth);
extern int iscsi_eh_abort(struct scsi_cmnd *sc);
extern int iscsi_eh_target_reset(struct scsi_cmnd *sc);
extern int iscsi_eh_device_reset(struct scsi_cmnd *sc);
extern int iscsi_queuecommand(struct scsi_cmnd *sc,
			      void (*done)(struct scsi_cmnd *));

/*
 * iSCSI host helpers.
 */
#define iscsi_host_priv(_shost) \
	(shost_priv(_shost) + sizeof(struct iscsi_host))

extern int iscsi_host_set_param(struct Scsi_Host *shost,
				enum iscsi_host_param param, char *buf,
				int buflen);
extern int iscsi_host_get_param(struct Scsi_Host *shost,
				enum iscsi_host_param param, char *buf);
extern int iscsi_host_add(struct Scsi_Host *shost, struct device *pdev);
extern struct Scsi_Host *iscsi_host_alloc(struct scsi_host_template *sht,
					  int dd_data_size,
					  bool xmit_can_sleep);
extern void iscsi_host_remove(struct Scsi_Host *shost);
extern void iscsi_host_free(struct Scsi_Host *shost);
extern int iscsi_target_alloc(struct scsi_target *starget);

/*
 * session management
 */
extern struct iscsi_cls_session *
iscsi_session_setup(struct iscsi_transport *, struct Scsi_Host *shost,
		    uint16_t, int, uint32_t, unsigned int);
extern void iscsi_session_teardown(struct iscsi_cls_session *);
extern void iscsi_session_recovery_timedout(struct iscsi_cls_session *);
extern int iscsi_set_param(struct iscsi_cls_conn *cls_conn,
			   enum iscsi_param param, char *buf, int buflen);
extern int iscsi_session_get_param(struct iscsi_cls_session *cls_session,
				   enum iscsi_param param, char *buf);

#define iscsi_session_printk(prefix, _sess, fmt, a...)	\
	iscsi_cls_session_printk(prefix, _sess->cls_session, fmt, ##a)

/*
 * connection management
 */
extern struct iscsi_cls_conn *iscsi_conn_setup(struct iscsi_cls_session *,
					       int, uint32_t);
extern void iscsi_conn_teardown(struct iscsi_cls_conn *);
extern int iscsi_conn_start(struct iscsi_cls_conn *);
extern void iscsi_conn_stop(struct iscsi_cls_conn *, int);
extern int iscsi_conn_bind(struct iscsi_cls_session *, struct iscsi_cls_conn *,
			   int);
extern void iscsi_conn_failure(struct iscsi_conn *conn, enum iscsi_err err);
extern void iscsi_session_failure(struct iscsi_session *session,
				  enum iscsi_err err);
extern int iscsi_conn_get_param(struct iscsi_cls_conn *cls_conn,
				enum iscsi_param param, char *buf);
extern void iscsi_suspend_tx(struct iscsi_conn *conn);
extern void iscsi_conn_queue_work(struct iscsi_conn *conn);

#define iscsi_conn_printk(prefix, _c, fmt, a...) \
	iscsi_cls_conn_printk(prefix, ((struct iscsi_conn *)_c)->cls_conn, \
			      fmt, ##a)

/*
 * pdu and task processing
 */
extern void iscsi_update_cmdsn(struct iscsi_session *, struct iscsi_nopin *);
extern void iscsi_prep_data_out_pdu(struct iscsi_task *task,
				    struct iscsi_r2t_info *r2t,
				    struct iscsi_data *hdr);
extern int iscsi_conn_send_pdu(struct iscsi_cls_conn *, struct iscsi_hdr *,
				char *, uint32_t);
extern int iscsi_complete_pdu(struct iscsi_conn *, struct iscsi_hdr *,
			      char *, int);
extern int __iscsi_complete_pdu(struct iscsi_conn *, struct iscsi_hdr *,
				char *, int);
extern int iscsi_verify_itt(struct iscsi_conn *, itt_t);
extern struct iscsi_task *iscsi_itt_to_ctask(struct iscsi_conn *, itt_t);
extern struct iscsi_task *iscsi_itt_to_task(struct iscsi_conn *, itt_t);
extern void iscsi_requeue_task(struct iscsi_task *task);
extern void iscsi_put_task(struct iscsi_task *task);
extern void __iscsi_get_task(struct iscsi_task *task);

/*
 * generic helpers
 */
extern void iscsi_pool_free(struct iscsi_pool *);
extern int iscsi_pool_init(struct iscsi_pool *, int, void ***, int);

/*
 * inline functions to deal with padding.
 */
static inline unsigned int
iscsi_padded(unsigned int len)
{
	return (len + ISCSI_PAD_LEN - 1) & ~(ISCSI_PAD_LEN - 1);
}

static inline unsigned int
iscsi_padding(unsigned int len)
{
	len &= (ISCSI_PAD_LEN - 1);
	if (len)
		len = ISCSI_PAD_LEN - len;
	return len;
}

#endif
