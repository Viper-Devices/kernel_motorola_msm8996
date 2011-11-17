/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HIF_OPS_H
#define HIF_OPS_H

#include "hif.h"
#include "debug.h"

static inline int hif_read_write_sync(struct ath6kl *ar, u32 addr, u8 *buf,
				      u32 len, u32 request)
{
	ath6kl_dbg(ATH6KL_DBG_HIF,
		   "hif %s sync addr 0x%x buf 0x%p len %d request 0x%x\n",
		   (request & HIF_WRITE) ? "write" : "read",
		   addr, buf, len, request);

	return ar->hif_ops->read_write_sync(ar, addr, buf, len, request);
}

static inline int hif_write_async(struct ath6kl *ar, u32 address, u8 *buffer,
				  u32 length, u32 request,
				  struct htc_packet *packet)
{
	ath6kl_dbg(ATH6KL_DBG_HIF,
		   "hif write async addr 0x%x buf 0x%p len %d request 0x%x\n",
		   address, buffer, length, request);

	return ar->hif_ops->write_async(ar, address, buffer, length,
					request, packet);
}
static inline void ath6kl_hif_irq_enable(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif irq enable\n");

	return ar->hif_ops->irq_enable(ar);
}

static inline void ath6kl_hif_irq_disable(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif irq disable\n");

	return ar->hif_ops->irq_disable(ar);
}

static inline struct hif_scatter_req *hif_scatter_req_get(struct ath6kl *ar)
{
	return ar->hif_ops->scatter_req_get(ar);
}

static inline void hif_scatter_req_add(struct ath6kl *ar,
				       struct hif_scatter_req *s_req)
{
	return ar->hif_ops->scatter_req_add(ar, s_req);
}

static inline int ath6kl_hif_enable_scatter(struct ath6kl *ar)
{
	return ar->hif_ops->enable_scatter(ar);
}

static inline int ath6kl_hif_scat_req_rw(struct ath6kl *ar,
					 struct hif_scatter_req *scat_req)
{
	return ar->hif_ops->scat_req_rw(ar, scat_req);
}

static inline void ath6kl_hif_cleanup_scatter(struct ath6kl *ar)
{
	return ar->hif_ops->cleanup_scatter(ar);
}

static inline int ath6kl_hif_suspend(struct ath6kl *ar,
				     struct cfg80211_wowlan *wow)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif suspend\n");

	return ar->hif_ops->suspend(ar, wow);
}

static inline int ath6kl_hif_resume(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif resume\n");

	return ar->hif_ops->resume(ar);
}

static inline int ath6kl_hif_power_on(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif power on\n");

	return ar->hif_ops->power_on(ar);
}

static inline int ath6kl_hif_power_off(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif power off\n");

	return ar->hif_ops->power_off(ar);
}

static inline void ath6kl_hif_stop(struct ath6kl *ar)
{
	ath6kl_dbg(ATH6KL_DBG_HIF, "hif stop\n");

	ar->hif_ops->stop(ar);
}

#endif
