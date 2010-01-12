/*
 * Copyright (c) 2006 Oracle.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <net/sock.h>
#include <linux/in.h>
#include <linux/list.h>

#include "rds.h"

/* When transmitting messages in rds_send_xmit, we need to emerge from
 * time to time and briefly release the CPU. Otherwise the softlock watchdog
 * will kick our shin.
 * Also, it seems fairer to not let one busy connection stall all the
 * others.
 *
 * send_batch_count is the number of times we'll loop in send_xmit. Setting
 * it to 0 will restore the old behavior (where we looped until we had
 * drained the queue).
 */
static int send_batch_count = 64;
module_param(send_batch_count, int, 0444);
MODULE_PARM_DESC(send_batch_count, " batch factor when working the send queue");

/*
 * Reset the send state. Caller must hold c_send_lock when calling here.
 */
void rds_send_reset(struct rds_connection *conn)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;

	if (conn->c_xmit_rm) {
		/* Tell the user the RDMA op is no longer mapped by the
		 * transport. This isn't entirely true (it's flushed out
		 * independently) but as the connection is down, there's
		 * no ongoing RDMA to/from that memory */
		rds_message_unmapped(conn->c_xmit_rm);
		rds_message_put(conn->c_xmit_rm);
		conn->c_xmit_rm = NULL;
	}
	conn->c_xmit_sg = 0;
	conn->c_xmit_hdr_off = 0;
	conn->c_xmit_data_off = 0;
	conn->c_xmit_rdma_sent = 0;
	conn->c_xmit_atomic_sent = 0;

	conn->c_map_queued = 0;

	conn->c_unacked_packets = rds_sysctl_max_unacked_packets;
	conn->c_unacked_bytes = rds_sysctl_max_unacked_bytes;

	/* Mark messages as retransmissions, and move them to the send q */
	spin_lock_irqsave(&conn->c_lock, flags);
	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);
		set_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags);
	}
	list_splice_init(&conn->c_retrans, &conn->c_send_queue);
	spin_unlock_irqrestore(&conn->c_lock, flags);
}

/*
 * We're making the concious trade-off here to only send one message
 * down the connection at a time.
 *   Pro:
 *      - tx queueing is a simple fifo list
 *   	- reassembly is optional and easily done by transports per conn
 *      - no per flow rx lookup at all, straight to the socket
 *   	- less per-frag memory and wire overhead
 *   Con:
 *      - queued acks can be delayed behind large messages
 *   Depends:
 *      - small message latency is higher behind queued large messages
 *      - large message latency isn't starved by intervening small sends
 */
int rds_send_xmit(struct rds_connection *conn)
{
	struct rds_message *rm;
	unsigned long flags;
	unsigned int tmp;
	unsigned int send_quota = send_batch_count;
	struct scatterlist *sg;
	int ret = 0;
	int was_empty = 0;
	LIST_HEAD(to_be_dropped);

	/*
	 * sendmsg calls here after having queued its message on the send
	 * queue.  We only have one task feeding the connection at a time.  If
	 * another thread is already feeding the queue then we back off.  This
	 * avoids blocking the caller and trading per-connection data between
	 * caches per message.
	 *
	 * The sem holder will issue a retry if they notice that someone queued
	 * a message after they stopped walking the send queue but before they
	 * dropped the sem.
	 */
	if (!mutex_trylock(&conn->c_send_lock)) {
		rds_stats_inc(s_send_sem_contention);
		ret = -ENOMEM;
		goto out;
	}

	if (conn->c_trans->xmit_prepare)
		conn->c_trans->xmit_prepare(conn);

	/*
	 * spin trying to push headers and data down the connection until
	 * the connection doens't make forward progress.
	 */
	while (--send_quota) {
		/*
		 * See if need to send a congestion map update if we're
		 * between sending messages.  The send_sem protects our sole
		 * use of c_map_offset and _bytes.
		 * Note this is used only by transports that define a special
		 * xmit_cong_map function. For all others, we create allocate
		 * a cong_map message and treat it just like any other send.
		 */
		if (conn->c_map_bytes) {
			ret = conn->c_trans->xmit_cong_map(conn, conn->c_lcong,
							   conn->c_map_offset);
			if (ret <= 0)
				break;

			conn->c_map_offset += ret;
			conn->c_map_bytes -= ret;
			if (conn->c_map_bytes)
				continue;
		}

		/* If we're done sending the current message, clear the
		 * offset and S/G temporaries.
		 */
		rm = conn->c_xmit_rm;
		if (rm &&
		    conn->c_xmit_hdr_off == sizeof(struct rds_header) &&
		    conn->c_xmit_sg == rm->data.m_nents) {
			conn->c_xmit_rm = NULL;
			conn->c_xmit_sg = 0;
			conn->c_xmit_hdr_off = 0;
			conn->c_xmit_data_off = 0;
			conn->c_xmit_rdma_sent = 0;
			conn->c_xmit_atomic_sent = 0;

			/* Release the reference to the previous message. */
			rds_message_put(rm);
			rm = NULL;
		}

		/* If we're asked to send a cong map update, do so.
		 */
		if (!rm && test_and_clear_bit(0, &conn->c_map_queued)) {
			if (conn->c_trans->xmit_cong_map) {
				conn->c_map_offset = 0;
				conn->c_map_bytes = sizeof(struct rds_header) +
					RDS_CONG_MAP_BYTES;
				continue;
			}

			rm = rds_cong_update_alloc(conn);
			if (IS_ERR(rm)) {
				ret = PTR_ERR(rm);
				break;
			}

			conn->c_xmit_rm = rm;
		}

		/*
		 * Grab the next message from the send queue, if there is one.
		 *
		 * c_xmit_rm holds a ref while we're sending this message down
		 * the connction.  We can use this ref while holding the
		 * send_sem.. rds_send_reset() is serialized with it.
		 */
		if (!rm) {
			unsigned int len;

			spin_lock_irqsave(&conn->c_lock, flags);

			if (!list_empty(&conn->c_send_queue)) {
				rm = list_entry(conn->c_send_queue.next,
						struct rds_message,
						m_conn_item);
				rds_message_addref(rm);

				/*
				 * Move the message from the send queue to the retransmit
				 * list right away.
				 */
				list_move_tail(&rm->m_conn_item, &conn->c_retrans);
			}

			spin_unlock_irqrestore(&conn->c_lock, flags);

			if (!rm) {
				was_empty = 1;
				break;
			}

			/* Unfortunately, the way Infiniband deals with
			 * RDMA to a bad MR key is by moving the entire
			 * queue pair to error state. We cold possibly
			 * recover from that, but right now we drop the
			 * connection.
			 * Therefore, we never retransmit messages with RDMA ops.
			 */
			if (rm->rdma.m_rdma_op.r_active &&
			    test_bit(RDS_MSG_RETRANSMITTED, &rm->m_flags)) {
				spin_lock_irqsave(&conn->c_lock, flags);
				if (test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags))
					list_move(&rm->m_conn_item, &to_be_dropped);
				spin_unlock_irqrestore(&conn->c_lock, flags);
				rds_message_put(rm);
				continue;
			}

			/* Require an ACK every once in a while */
			len = ntohl(rm->m_inc.i_hdr.h_len);
			if (conn->c_unacked_packets == 0 ||
			    conn->c_unacked_bytes < len) {
				__set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

				conn->c_unacked_packets = rds_sysctl_max_unacked_packets;
				conn->c_unacked_bytes = rds_sysctl_max_unacked_bytes;
				rds_stats_inc(s_send_ack_required);
			} else {
				conn->c_unacked_bytes -= len;
				conn->c_unacked_packets--;
			}

			conn->c_xmit_rm = rm;
		}


		if (rm->atomic.op_active && !conn->c_xmit_atomic_sent) {
			ret = conn->c_trans->xmit_atomic(conn, &rm->atomic);
			if (ret)
				break;
			conn->c_xmit_atomic_sent = 1;
			/* The transport owns the mapped memory for now.
			 * You can't unmap it while it's on the send queue */
			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
		}

		/*
		 * Try and send an rdma message.  Let's see if we can
		 * keep this simple and require that the transport either
		 * send the whole rdma or none of it.
		 */
		if (rm->rdma.m_rdma_op.r_active && !conn->c_xmit_rdma_sent) {
			ret = conn->c_trans->xmit_rdma(conn, &rm->rdma.m_rdma_op);
			if (ret)
				break;
			conn->c_xmit_rdma_sent = 1;
			/* The transport owns the mapped memory for now.
			 * You can't unmap it while it's on the send queue */
			set_bit(RDS_MSG_MAPPED, &rm->m_flags);
		}

		if (conn->c_xmit_hdr_off < sizeof(struct rds_header) ||
		    conn->c_xmit_sg < rm->data.m_nents) {
			ret = conn->c_trans->xmit(conn, rm,
						  conn->c_xmit_hdr_off,
						  conn->c_xmit_sg,
						  conn->c_xmit_data_off);
			if (ret <= 0)
				break;

			if (conn->c_xmit_hdr_off < sizeof(struct rds_header)) {
				tmp = min_t(int, ret,
					    sizeof(struct rds_header) -
					    conn->c_xmit_hdr_off);
				conn->c_xmit_hdr_off += tmp;
				ret -= tmp;
			}

			sg = &rm->data.m_sg[conn->c_xmit_sg];
			while (ret) {
				tmp = min_t(int, ret, sg->length -
						      conn->c_xmit_data_off);
				conn->c_xmit_data_off += tmp;
				ret -= tmp;
				if (conn->c_xmit_data_off == sg->length) {
					conn->c_xmit_data_off = 0;
					sg++;
					conn->c_xmit_sg++;
					BUG_ON(ret != 0 &&
					       conn->c_xmit_sg == rm->data.m_nents);
				}
			}
		}
	}

	/* Nuke any messages we decided not to retransmit. */
	if (!list_empty(&to_be_dropped))
		rds_send_remove_from_sock(&to_be_dropped, RDS_RDMA_DROPPED);

	if (conn->c_trans->xmit_complete)
		conn->c_trans->xmit_complete(conn);

	/*
	 * We might be racing with another sender who queued a message but
	 * backed off on noticing that we held the c_send_lock.  If we check
	 * for queued messages after dropping the sem then either we'll
	 * see the queued message or the queuer will get the sem.  If we
	 * notice the queued message then we trigger an immediate retry.
	 *
	 * We need to be careful only to do this when we stopped processing
	 * the send queue because it was empty.  It's the only way we
	 * stop processing the loop when the transport hasn't taken
	 * responsibility for forward progress.
	 */
	mutex_unlock(&conn->c_send_lock);

	if (conn->c_map_bytes || (send_quota == 0 && !was_empty)) {
		/* We exhausted the send quota, but there's work left to
		 * do. Return and (re-)schedule the send worker.
		 */
		ret = -EAGAIN;
	}

	if (ret == 0 && was_empty) {
		/* A simple bit test would be way faster than taking the
		 * spin lock */
		spin_lock_irqsave(&conn->c_lock, flags);
		if (!list_empty(&conn->c_send_queue)) {
			rds_stats_inc(s_send_sem_queue_raced);
			ret = -EAGAIN;
		}
		spin_unlock_irqrestore(&conn->c_lock, flags);
	}
out:
	return ret;
}

static void rds_send_sndbuf_remove(struct rds_sock *rs, struct rds_message *rm)
{
	u32 len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	assert_spin_locked(&rs->rs_lock);

	BUG_ON(rs->rs_snd_bytes < len);
	rs->rs_snd_bytes -= len;

	if (rs->rs_snd_bytes == 0)
		rds_stats_inc(s_send_queue_empty);
}

static inline int rds_send_is_acked(struct rds_message *rm, u64 ack,
				    is_acked_func is_acked)
{
	if (is_acked)
		return is_acked(rm, ack);
	return be64_to_cpu(rm->m_inc.i_hdr.h_sequence) <= ack;
}

/*
 * Returns true if there are no messages on the send and retransmit queues
 * which have a sequence number greater than or equal to the given sequence
 * number.
 */
int rds_send_acked_before(struct rds_connection *conn, u64 seq)
{
	struct rds_message *rm, *tmp;
	int ret = 1;

	spin_lock(&conn->c_lock);

	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		if (be64_to_cpu(rm->m_inc.i_hdr.h_sequence) < seq)
			ret = 0;
		break;
	}

	list_for_each_entry_safe(rm, tmp, &conn->c_send_queue, m_conn_item) {
		if (be64_to_cpu(rm->m_inc.i_hdr.h_sequence) < seq)
			ret = 0;
		break;
	}

	spin_unlock(&conn->c_lock);

	return ret;
}

/*
 * This is pretty similar to what happens below in the ACK
 * handling code - except that we call here as soon as we get
 * the IB send completion on the RDMA op and the accompanying
 * message.
 */
void rds_rdma_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rds_rdma_op *ro;
	struct rds_notifier *notifier;
	unsigned long flags;

	spin_lock_irqsave(&rm->m_rs_lock, flags);

	ro = &rm->rdma.m_rdma_op;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags) &&
	    ro->r_active && ro->r_notify && ro->r_notifier) {
		notifier = ro->r_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ro->r_notifier = NULL;
	}

	spin_unlock_irqrestore(&rm->m_rs_lock, flags);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_rdma_send_complete);

/*
 * Just like above, except looks at atomic op
 */
void rds_atomic_send_complete(struct rds_message *rm, int status)
{
	struct rds_sock *rs = NULL;
	struct rm_atomic_op *ao;
	struct rds_notifier *notifier;

	spin_lock(&rm->m_rs_lock);

	ao = &rm->atomic;
	if (test_bit(RDS_MSG_ON_SOCK, &rm->m_flags)
	    && ao->op_active && ao->op_notify && ao->op_notifier) {
		notifier = ao->op_notifier;
		rs = rm->m_rs;
		sock_hold(rds_rs_to_sk(rs));

		notifier->n_status = status;
		spin_lock(&rs->rs_lock);
		list_add_tail(&notifier->n_list, &rs->rs_notify_queue);
		spin_unlock(&rs->rs_lock);

		ao->op_notifier = NULL;
	}

	spin_unlock(&rm->m_rs_lock);

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}
EXPORT_SYMBOL_GPL(rds_atomic_send_complete);

/*
 * This is the same as rds_rdma_send_complete except we
 * don't do any locking - we have all the ingredients (message,
 * socket, socket lock) and can just move the notifier.
 */
static inline void
__rds_rdma_send_complete(struct rds_sock *rs, struct rds_message *rm, int status)
{
	struct rds_rdma_op *ro;

	ro = &rm->rdma.m_rdma_op;
	if (ro->r_active && ro->r_notify && ro->r_notifier) {
		ro->r_notifier->n_status = status;
		list_add_tail(&ro->r_notifier->n_list, &rs->rs_notify_queue);
		ro->r_notifier = NULL;
	}

	/* No need to wake the app - caller does this */
}

/*
 * This is called from the IB send completion when we detect
 * a RDMA operation that failed with remote access error.
 * So speed is not an issue here.
 */
struct rds_message *rds_send_get_message(struct rds_connection *conn,
					 struct rds_rdma_op *op)
{
	struct rds_message *rm, *tmp, *found = NULL;
	unsigned long flags;

	spin_lock_irqsave(&conn->c_lock, flags);

	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		if (&rm->rdma.m_rdma_op == op) {
			atomic_inc(&rm->m_refcount);
			found = rm;
			goto out;
		}
	}

	list_for_each_entry_safe(rm, tmp, &conn->c_send_queue, m_conn_item) {
		if (&rm->rdma.m_rdma_op == op) {
			atomic_inc(&rm->m_refcount);
			found = rm;
			break;
		}
	}

out:
	spin_unlock_irqrestore(&conn->c_lock, flags);

	return found;
}
EXPORT_SYMBOL_GPL(rds_send_get_message);

/*
 * This removes messages from the socket's list if they're on it.  The list
 * argument must be private to the caller, we must be able to modify it
 * without locks.  The messages must have a reference held for their
 * position on the list.  This function will drop that reference after
 * removing the messages from the 'messages' list regardless of if it found
 * the messages on the socket list or not.
 */
void rds_send_remove_from_sock(struct list_head *messages, int status)
{
	unsigned long flags;
	struct rds_sock *rs = NULL;
	struct rds_message *rm;

	while (!list_empty(messages)) {
		int was_on_sock = 0;

		rm = list_entry(messages->next, struct rds_message,
				m_conn_item);
		list_del_init(&rm->m_conn_item);

		/*
		 * If we see this flag cleared then we're *sure* that someone
		 * else beat us to removing it from the sock.  If we race
		 * with their flag update we'll get the lock and then really
		 * see that the flag has been cleared.
		 *
		 * The message spinlock makes sure nobody clears rm->m_rs
		 * while we're messing with it. It does not prevent the
		 * message from being removed from the socket, though.
		 */
		spin_lock_irqsave(&rm->m_rs_lock, flags);
		if (!test_bit(RDS_MSG_ON_SOCK, &rm->m_flags))
			goto unlock_and_drop;

		if (rs != rm->m_rs) {
			if (rs) {
				rds_wake_sk_sleep(rs);
				sock_put(rds_rs_to_sk(rs));
			}
			rs = rm->m_rs;
			sock_hold(rds_rs_to_sk(rs));
		}
		spin_lock(&rs->rs_lock);

		if (test_and_clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags)) {
			struct rds_rdma_op *ro = &rm->rdma.m_rdma_op;
			struct rds_notifier *notifier;

			list_del_init(&rm->m_sock_item);
			rds_send_sndbuf_remove(rs, rm);

			if (ro->r_active && ro->r_notifier &&
			    (ro->r_notify || (ro->r_recverr && status))) {
				notifier = ro->r_notifier;
				list_add_tail(&notifier->n_list,
						&rs->rs_notify_queue);
				if (!notifier->n_status)
					notifier->n_status = status;
				rm->rdma.m_rdma_op.r_notifier = NULL;
			}
			was_on_sock = 1;
			rm->m_rs = NULL;
		}
		spin_unlock(&rs->rs_lock);

unlock_and_drop:
		spin_unlock_irqrestore(&rm->m_rs_lock, flags);
		rds_message_put(rm);
		if (was_on_sock)
			rds_message_put(rm);
	}

	if (rs) {
		rds_wake_sk_sleep(rs);
		sock_put(rds_rs_to_sk(rs));
	}
}

/*
 * Transports call here when they've determined that the receiver queued
 * messages up to, and including, the given sequence number.  Messages are
 * moved to the retrans queue when rds_send_xmit picks them off the send
 * queue. This means that in the TCP case, the message may not have been
 * assigned the m_ack_seq yet - but that's fine as long as tcp_is_acked
 * checks the RDS_MSG_HAS_ACK_SEQ bit.
 *
 * XXX It's not clear to me how this is safely serialized with socket
 * destruction.  Maybe it should bail if it sees SOCK_DEAD.
 */
void rds_send_drop_acked(struct rds_connection *conn, u64 ack,
			 is_acked_func is_acked)
{
	struct rds_message *rm, *tmp;
	unsigned long flags;
	LIST_HEAD(list);

	spin_lock_irqsave(&conn->c_lock, flags);

	list_for_each_entry_safe(rm, tmp, &conn->c_retrans, m_conn_item) {
		if (!rds_send_is_acked(rm, ack, is_acked))
			break;

		list_move(&rm->m_conn_item, &list);
		clear_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	}

	/* order flag updates with spin locks */
	if (!list_empty(&list))
		smp_mb__after_clear_bit();

	spin_unlock_irqrestore(&conn->c_lock, flags);

	/* now remove the messages from the sock list as needed */
	rds_send_remove_from_sock(&list, RDS_RDMA_SUCCESS);
}
EXPORT_SYMBOL_GPL(rds_send_drop_acked);

void rds_send_drop_to(struct rds_sock *rs, struct sockaddr_in *dest)
{
	struct rds_message *rm, *tmp;
	struct rds_connection *conn;
	unsigned long flags;
	LIST_HEAD(list);

	/* get all the messages we're dropping under the rs lock */
	spin_lock_irqsave(&rs->rs_lock, flags);

	list_for_each_entry_safe(rm, tmp, &rs->rs_send_queue, m_sock_item) {
		if (dest && (dest->sin_addr.s_addr != rm->m_daddr ||
			     dest->sin_port != rm->m_inc.i_hdr.h_dport))
			continue;

		list_move(&rm->m_sock_item, &list);
		rds_send_sndbuf_remove(rs, rm);
		clear_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
	}

	/* order flag updates with the rs lock */
	smp_mb__after_clear_bit();

	spin_unlock_irqrestore(&rs->rs_lock, flags);

	if (list_empty(&list))
		return;

	/* Remove the messages from the conn */
	list_for_each_entry(rm, &list, m_sock_item) {

		conn = rm->m_inc.i_conn;

		spin_lock_irqsave(&conn->c_lock, flags);
		/*
		 * Maybe someone else beat us to removing rm from the conn.
		 * If we race with their flag update we'll get the lock and
		 * then really see that the flag has been cleared.
		 */
		if (!test_and_clear_bit(RDS_MSG_ON_CONN, &rm->m_flags)) {
			spin_unlock_irqrestore(&conn->c_lock, flags);
			continue;
		}
		list_del_init(&rm->m_conn_item);
		spin_unlock_irqrestore(&conn->c_lock, flags);

		/*
		 * Couldn't grab m_rs_lock in top loop (lock ordering),
		 * but we can now.
		 */
		spin_lock_irqsave(&rm->m_rs_lock, flags);

		spin_lock(&rs->rs_lock);
		__rds_rdma_send_complete(rs, rm, RDS_RDMA_CANCELED);
		spin_unlock(&rs->rs_lock);

		rm->m_rs = NULL;
		spin_unlock_irqrestore(&rm->m_rs_lock, flags);

		rds_message_put(rm);
	}

	rds_wake_sk_sleep(rs);

	while (!list_empty(&list)) {
		rm = list_entry(list.next, struct rds_message, m_sock_item);
		list_del_init(&rm->m_sock_item);

		rds_message_wait(rm);
		rds_message_put(rm);
	}
}

/*
 * we only want this to fire once so we use the callers 'queued'.  It's
 * possible that another thread can race with us and remove the
 * message from the flow with RDS_CANCEL_SENT_TO.
 */
static int rds_send_queue_rm(struct rds_sock *rs, struct rds_connection *conn,
			     struct rds_message *rm, __be16 sport,
			     __be16 dport, int *queued)
{
	unsigned long flags;
	u32 len;

	if (*queued)
		goto out;

	len = be32_to_cpu(rm->m_inc.i_hdr.h_len);

	/* this is the only place which holds both the socket's rs_lock
	 * and the connection's c_lock */
	spin_lock_irqsave(&rs->rs_lock, flags);

	/*
	 * If there is a little space in sndbuf, we don't queue anything,
	 * and userspace gets -EAGAIN. But poll() indicates there's send
	 * room. This can lead to bad behavior (spinning) if snd_bytes isn't
	 * freed up by incoming acks. So we check the *old* value of
	 * rs_snd_bytes here to allow the last msg to exceed the buffer,
	 * and poll() now knows no more data can be sent.
	 */
	if (rs->rs_snd_bytes < rds_sk_sndbuf(rs)) {
		rs->rs_snd_bytes += len;

		/* let recv side know we are close to send space exhaustion.
		 * This is probably not the optimal way to do it, as this
		 * means we set the flag on *all* messages as soon as our
		 * throughput hits a certain threshold.
		 */
		if (rs->rs_snd_bytes >= rds_sk_sndbuf(rs) / 2)
			__set_bit(RDS_MSG_ACK_REQUIRED, &rm->m_flags);

		list_add_tail(&rm->m_sock_item, &rs->rs_send_queue);
		set_bit(RDS_MSG_ON_SOCK, &rm->m_flags);
		rds_message_addref(rm);
		rm->m_rs = rs;

		/* The code ordering is a little weird, but we're
		   trying to minimize the time we hold c_lock */
		rds_message_populate_header(&rm->m_inc.i_hdr, sport, dport, 0);
		rm->m_inc.i_conn = conn;
		rds_message_addref(rm);

		spin_lock(&conn->c_lock);
		rm->m_inc.i_hdr.h_sequence = cpu_to_be64(conn->c_next_tx_seq++);
		list_add_tail(&rm->m_conn_item, &conn->c_send_queue);
		set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
		spin_unlock(&conn->c_lock);

		rdsdebug("queued msg %p len %d, rs %p bytes %d seq %llu\n",
			 rm, len, rs, rs->rs_snd_bytes,
			 (unsigned long long)be64_to_cpu(rm->m_inc.i_hdr.h_sequence));

		*queued = 1;
	}

	spin_unlock_irqrestore(&rs->rs_lock, flags);
out:
	return *queued;
}

/*
 * rds_message is getting to be quite complicated, and we'd like to allocate
 * it all in one go. This figures out how big it needs to be up front.
 */
static int rds_rm_size(struct msghdr *msg, int data_len)
{
	struct cmsghdr *cmsg;
	int size = 0;
	int retval;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			retval = rds_rdma_extra_size(CMSG_DATA(cmsg));
			if (retval < 0)
				return retval;
			size += retval;
			break;

		case RDS_CMSG_RDMA_DEST:
		case RDS_CMSG_RDMA_MAP:
			/* these are valid but do no add any size */
			break;

		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
			size += sizeof(struct scatterlist);
			break;

		default:
			return -EINVAL;
		}

	}

	size += ceil(data_len, PAGE_SIZE) * sizeof(struct scatterlist);

	return size;
}

static int rds_cmsg_send(struct rds_sock *rs, struct rds_message *rm,
			 struct msghdr *msg, int *allocated_mr)
{
	struct cmsghdr *cmsg;
	int ret = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (!CMSG_OK(msg, cmsg))
			return -EINVAL;

		if (cmsg->cmsg_level != SOL_RDS)
			continue;

		/* As a side effect, RDMA_DEST and RDMA_MAP will set
		 * rm->rdma.m_rdma_cookie and rm->rdma.m_rdma_mr.
		 */
		switch (cmsg->cmsg_type) {
		case RDS_CMSG_RDMA_ARGS:
			ret = rds_cmsg_rdma_args(rs, rm, cmsg);
			break;

		case RDS_CMSG_RDMA_DEST:
			ret = rds_cmsg_rdma_dest(rs, rm, cmsg);
			break;

		case RDS_CMSG_RDMA_MAP:
			ret = rds_cmsg_rdma_map(rs, rm, cmsg);
			if (!ret)
				*allocated_mr = 1;
			break;
		case RDS_CMSG_ATOMIC_CSWP:
		case RDS_CMSG_ATOMIC_FADD:
			ret = rds_cmsg_atomic(rs, rm, cmsg);
			break;

		default:
			return -EINVAL;
		}

		if (ret)
			break;
	}

	return ret;
}

int rds_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
		size_t payload_len)
{
	struct sock *sk = sock->sk;
	struct rds_sock *rs = rds_sk_to_rs(sk);
	struct sockaddr_in *usin = (struct sockaddr_in *)msg->msg_name;
	__be32 daddr;
	__be16 dport;
	struct rds_message *rm = NULL;
	struct rds_connection *conn;
	int ret = 0;
	int queued = 0, allocated_mr = 0;
	int nonblock = msg->msg_flags & MSG_DONTWAIT;
	long timeo = sock_sndtimeo(sk, nonblock);

	/* Mirror Linux UDP mirror of BSD error message compatibility */
	/* XXX: Perhaps MSG_MORE someday */
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_CMSG_COMPAT)) {
		printk(KERN_INFO "msg_flags 0x%08X\n", msg->msg_flags);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (msg->msg_namelen) {
		/* XXX fail non-unicast destination IPs? */
		if (msg->msg_namelen < sizeof(*usin) || usin->sin_family != AF_INET) {
			ret = -EINVAL;
			goto out;
		}
		daddr = usin->sin_addr.s_addr;
		dport = usin->sin_port;
	} else {
		/* We only care about consistency with ->connect() */
		lock_sock(sk);
		daddr = rs->rs_conn_addr;
		dport = rs->rs_conn_port;
		release_sock(sk);
	}

	/* racing with another thread binding seems ok here */
	if (daddr == 0 || rs->rs_bound_addr == 0) {
		ret = -ENOTCONN; /* XXX not a great errno */
		goto out;
	}

	/* size of rm including all sgs */
	ret = rds_rm_size(msg, payload_len);
	if (ret < 0)
		goto out;

	rm = rds_message_alloc(ret, GFP_KERNEL);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	rm->data.m_sg = rds_message_alloc_sgs(rm, ceil(payload_len, PAGE_SIZE));
	/* XXX fix this to not allocate memory */
	ret = rds_message_copy_from_user(rm, msg->msg_iov, payload_len);
	if (ret)
		goto out;

	rm->m_daddr = daddr;

	/* rds_conn_create has a spinlock that runs with IRQ off.
	 * Caching the conn in the socket helps a lot. */
	if (rs->rs_conn && rs->rs_conn->c_faddr == daddr)
		conn = rs->rs_conn;
	else {
		conn = rds_conn_create_outgoing(rs->rs_bound_addr, daddr,
					rs->rs_transport,
					sock->sk->sk_allocation);
		if (IS_ERR(conn)) {
			ret = PTR_ERR(conn);
			goto out;
		}
		rs->rs_conn = conn;
	}

	/* Parse any control messages the user may have included. */
	ret = rds_cmsg_send(rs, rm, msg, &allocated_mr);
	if (ret)
		goto out;

	if ((rm->m_rdma_cookie || rm->rdma.m_rdma_op.r_active) &&
	       !conn->c_trans->xmit_rdma) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "rdma_op %p conn xmit_rdma %p\n",
			       &rm->rdma.m_rdma_op, conn->c_trans->xmit_rdma);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (rm->atomic.op_active && !conn->c_trans->xmit_atomic) {
		if (printk_ratelimit())
			printk(KERN_NOTICE "atomic_op %p conn xmit_atomic %p\n",
			       &rm->atomic, conn->c_trans->xmit_atomic);
		ret = -EOPNOTSUPP;
		goto out;
	}

	/* If the connection is down, trigger a connect. We may
	 * have scheduled a delayed reconnect however - in this case
	 * we should not interfere.
	 */
	if (rds_conn_state(conn) == RDS_CONN_DOWN &&
	    !test_and_set_bit(RDS_RECONNECT_PENDING, &conn->c_flags))
		queue_delayed_work(rds_wq, &conn->c_conn_w, 0);

	ret = rds_cong_wait(conn->c_fcong, dport, nonblock, rs);
	if (ret) {
		rs->rs_seen_congestion = 1;
		goto out;
	}

	while (!rds_send_queue_rm(rs, conn, rm, rs->rs_bound_port,
				  dport, &queued)) {
		rds_stats_inc(s_send_queue_full);
		/* XXX make sure this is reasonable */
		if (payload_len > rds_sk_sndbuf(rs)) {
			ret = -EMSGSIZE;
			goto out;
		}
		if (nonblock) {
			ret = -EAGAIN;
			goto out;
		}

		timeo = wait_event_interruptible_timeout(*sk_sleep(sk),
					rds_send_queue_rm(rs, conn, rm,
							  rs->rs_bound_port,
							  dport,
							  &queued),
					timeo);
		rdsdebug("sendmsg woke queued %d timeo %ld\n", queued, timeo);
		if (timeo > 0 || timeo == MAX_SCHEDULE_TIMEOUT)
			continue;

		ret = timeo;
		if (ret == 0)
			ret = -ETIMEDOUT;
		goto out;
	}

	/*
	 * By now we've committed to the send.  We reuse rds_send_worker()
	 * to retry sends in the rds thread if the transport asks us to.
	 */
	rds_stats_inc(s_send_queued);

	if (!test_bit(RDS_LL_SEND_FULL, &conn->c_flags))
		rds_send_worker(&conn->c_send_w.work);

	rds_message_put(rm);
	return payload_len;

out:
	/* If the user included a RDMA_MAP cmsg, we allocated a MR on the fly.
	 * If the sendmsg goes through, we keep the MR. If it fails with EAGAIN
	 * or in any other way, we need to destroy the MR again */
	if (allocated_mr)
		rds_rdma_unuse(rs, rds_rdma_cookie_key(rm->m_rdma_cookie), 1);

	if (rm)
		rds_message_put(rm);
	return ret;
}

/*
 * Reply to a ping packet.
 */
int
rds_send_pong(struct rds_connection *conn, __be16 dport)
{
	struct rds_message *rm;
	unsigned long flags;
	int ret = 0;

	rm = rds_message_alloc(0, GFP_ATOMIC);
	if (!rm) {
		ret = -ENOMEM;
		goto out;
	}

	rm->m_daddr = conn->c_faddr;

	/* If the connection is down, trigger a connect. We may
	 * have scheduled a delayed reconnect however - in this case
	 * we should not interfere.
	 */
	if (rds_conn_state(conn) == RDS_CONN_DOWN &&
	    !test_and_set_bit(RDS_RECONNECT_PENDING, &conn->c_flags))
		queue_delayed_work(rds_wq, &conn->c_conn_w, 0);

	ret = rds_cong_wait(conn->c_fcong, dport, 1, NULL);
	if (ret)
		goto out;

	spin_lock_irqsave(&conn->c_lock, flags);
	list_add_tail(&rm->m_conn_item, &conn->c_send_queue);
	set_bit(RDS_MSG_ON_CONN, &rm->m_flags);
	rds_message_addref(rm);
	rm->m_inc.i_conn = conn;

	rds_message_populate_header(&rm->m_inc.i_hdr, 0, dport,
				    conn->c_next_tx_seq);
	conn->c_next_tx_seq++;
	spin_unlock_irqrestore(&conn->c_lock, flags);

	rds_stats_inc(s_send_queued);
	rds_stats_inc(s_send_pong);

	queue_delayed_work(rds_wq, &conn->c_send_w, 0);
	rds_message_put(rm);
	return 0;

out:
	if (rm)
		rds_message_put(rm);
	return ret;
}
