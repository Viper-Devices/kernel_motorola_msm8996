/*
 * Copyright (c) 2011 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/usb.h>
#include <linux/vmalloc.h>

#include <brcmu_utils.h>
#include <brcmu_wifi.h>
#include <dhd_bus.h>
#include <dhd_dbg.h>

#include "firmware.h"
#include "usb_rdl.h"
#include "usb.h"

#define IOCTL_RESP_TIMEOUT		2000

#define BRCMF_USB_RESET_GETVER_SPINWAIT	100	/* in unit of ms */
#define BRCMF_USB_RESET_GETVER_LOOP_CNT	10

#define BRCMF_POSTBOOT_ID		0xA123  /* ID to detect if dongle
						   has boot up */
#define BRCMF_USB_NRXQ			50
#define BRCMF_USB_NTXQ			50

#define BRCMF_USB_CBCTL_WRITE		0
#define BRCMF_USB_CBCTL_READ		1
#define BRCMF_USB_MAX_PKT_SIZE		1600

#define BRCMF_USB_43143_FW_NAME		"brcm/brcmfmac43143.bin"
#define BRCMF_USB_43236_FW_NAME		"brcm/brcmfmac43236b.bin"
#define BRCMF_USB_43242_FW_NAME		"brcm/brcmfmac43242a.bin"
#define BRCMF_USB_43569_FW_NAME		"brcm/brcmfmac43569.bin"

struct brcmf_usb_image {
	struct list_head list;
	s8 *fwname;
	u8 *image;
	int image_len;
};

struct brcmf_usbdev_info {
	struct brcmf_usbdev bus_pub; /* MUST BE FIRST */
	spinlock_t qlock;
	struct list_head rx_freeq;
	struct list_head rx_postq;
	struct list_head tx_freeq;
	struct list_head tx_postq;
	uint rx_pipe, tx_pipe;

	int rx_low_watermark;
	int tx_low_watermark;
	int tx_high_watermark;
	int tx_freecount;
	bool tx_flowblock;
	spinlock_t tx_flowblock_lock;

	struct brcmf_usbreq *tx_reqs;
	struct brcmf_usbreq *rx_reqs;

	const u8 *image;	/* buffer for combine fw and nvram */
	int image_len;

	struct usb_device *usbdev;
	struct device *dev;

	int ctl_in_pipe, ctl_out_pipe;
	struct urb *ctl_urb; /* URB for control endpoint */
	struct usb_ctrlrequest ctl_write;
	struct usb_ctrlrequest ctl_read;
	u32 ctl_urb_actual_length;
	int ctl_urb_status;
	int ctl_completed;
	wait_queue_head_t ioctl_resp_wait;
	ulong ctl_op;
	u8 ifnum;

	struct urb *bulk_urb; /* used for FW download */
};

static void brcmf_usb_rx_refill(struct brcmf_usbdev_info *devinfo,
				struct brcmf_usbreq  *req);

static struct brcmf_usbdev *brcmf_usb_get_buspub(struct device *dev)
{
	struct brcmf_bus *bus_if = dev_get_drvdata(dev);
	return bus_if->bus_priv.usb;
}

static struct brcmf_usbdev_info *brcmf_usb_get_businfo(struct device *dev)
{
	return brcmf_usb_get_buspub(dev)->devinfo;
}

static int brcmf_usb_ioctl_resp_wait(struct brcmf_usbdev_info *devinfo)
{
	return wait_event_timeout(devinfo->ioctl_resp_wait,
				  devinfo->ctl_completed,
				  msecs_to_jiffies(IOCTL_RESP_TIMEOUT));
}

static void brcmf_usb_ioctl_resp_wake(struct brcmf_usbdev_info *devinfo)
{
	if (waitqueue_active(&devinfo->ioctl_resp_wait))
		wake_up(&devinfo->ioctl_resp_wait);
}

static void
brcmf_usb_ctl_complete(struct brcmf_usbdev_info *devinfo, int type, int status)
{
	brcmf_dbg(USB, "Enter, status=%d\n", status);

	if (unlikely(devinfo == NULL))
		return;

	if (type == BRCMF_USB_CBCTL_READ) {
		if (status == 0)
			devinfo->bus_pub.stats.rx_ctlpkts++;
		else
			devinfo->bus_pub.stats.rx_ctlerrs++;
	} else if (type == BRCMF_USB_CBCTL_WRITE) {
		if (status == 0)
			devinfo->bus_pub.stats.tx_ctlpkts++;
		else
			devinfo->bus_pub.stats.tx_ctlerrs++;
	}

	devinfo->ctl_urb_status = status;
	devinfo->ctl_completed = true;
	brcmf_usb_ioctl_resp_wake(devinfo);
}

static void
brcmf_usb_ctlread_complete(struct urb *urb)
{
	struct brcmf_usbdev_info *devinfo =
		(struct brcmf_usbdev_info *)urb->context;

	brcmf_dbg(USB, "Enter\n");
	devinfo->ctl_urb_actual_length = urb->actual_length;
	brcmf_usb_ctl_complete(devinfo, BRCMF_USB_CBCTL_READ,
		urb->status);
}

static void
brcmf_usb_ctlwrite_complete(struct urb *urb)
{
	struct brcmf_usbdev_info *devinfo =
		(struct brcmf_usbdev_info *)urb->context;

	brcmf_dbg(USB, "Enter\n");
	brcmf_usb_ctl_complete(devinfo, BRCMF_USB_CBCTL_WRITE,
		urb->status);
}

static int
brcmf_usb_send_ctl(struct brcmf_usbdev_info *devinfo, u8 *buf, int len)
{
	int ret;
	u16 size;

	brcmf_dbg(USB, "Enter\n");
	if (devinfo == NULL || buf == NULL ||
	    len == 0 || devinfo->ctl_urb == NULL)
		return -EINVAL;

	size = len;
	devinfo->ctl_write.wLength = cpu_to_le16p(&size);
	devinfo->ctl_urb->transfer_buffer_length = size;
	devinfo->ctl_urb_status = 0;
	devinfo->ctl_urb_actual_length = 0;

	usb_fill_control_urb(devinfo->ctl_urb,
		devinfo->usbdev,
		devinfo->ctl_out_pipe,
		(unsigned char *) &devinfo->ctl_write,
		buf, size,
		(usb_complete_t)brcmf_usb_ctlwrite_complete,
		devinfo);

	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0)
		brcmf_err("usb_submit_urb failed %d\n", ret);

	return ret;
}

static int
brcmf_usb_recv_ctl(struct brcmf_usbdev_info *devinfo, u8 *buf, int len)
{
	int ret;
	u16 size;

	brcmf_dbg(USB, "Enter\n");
	if ((devinfo == NULL) || (buf == NULL) || (len == 0)
		|| (devinfo->ctl_urb == NULL))
		return -EINVAL;

	size = len;
	devinfo->ctl_read.wLength = cpu_to_le16p(&size);
	devinfo->ctl_urb->transfer_buffer_length = size;

	devinfo->ctl_read.bRequestType = USB_DIR_IN
		| USB_TYPE_CLASS | USB_RECIP_INTERFACE;
	devinfo->ctl_read.bRequest = 1;

	usb_fill_control_urb(devinfo->ctl_urb,
		devinfo->usbdev,
		devinfo->ctl_in_pipe,
		(unsigned char *) &devinfo->ctl_read,
		buf, size,
		(usb_complete_t)brcmf_usb_ctlread_complete,
		devinfo);

	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0)
		brcmf_err("usb_submit_urb failed %d\n", ret);

	return ret;
}

static int brcmf_usb_tx_ctlpkt(struct device *dev, u8 *buf, u32 len)
{
	int err = 0;
	int timeout = 0;
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(dev);

	brcmf_dbg(USB, "Enter\n");
	if (devinfo->bus_pub.state != BRCMFMAC_USB_STATE_UP)
		return -EIO;

	if (test_and_set_bit(0, &devinfo->ctl_op))
		return -EIO;

	devinfo->ctl_completed = false;
	err = brcmf_usb_send_ctl(devinfo, buf, len);
	if (err) {
		brcmf_err("fail %d bytes: %d\n", err, len);
		clear_bit(0, &devinfo->ctl_op);
		return err;
	}
	timeout = brcmf_usb_ioctl_resp_wait(devinfo);
	clear_bit(0, &devinfo->ctl_op);
	if (!timeout) {
		brcmf_err("Txctl wait timed out\n");
		err = -EIO;
	}
	return err;
}

static int brcmf_usb_rx_ctlpkt(struct device *dev, u8 *buf, u32 len)
{
	int err = 0;
	int timeout = 0;
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(dev);

	brcmf_dbg(USB, "Enter\n");
	if (devinfo->bus_pub.state != BRCMFMAC_USB_STATE_UP)
		return -EIO;

	if (test_and_set_bit(0, &devinfo->ctl_op))
		return -EIO;

	devinfo->ctl_completed = false;
	err = brcmf_usb_recv_ctl(devinfo, buf, len);
	if (err) {
		brcmf_err("fail %d bytes: %d\n", err, len);
		clear_bit(0, &devinfo->ctl_op);
		return err;
	}
	timeout = brcmf_usb_ioctl_resp_wait(devinfo);
	err = devinfo->ctl_urb_status;
	clear_bit(0, &devinfo->ctl_op);
	if (!timeout) {
		brcmf_err("rxctl wait timed out\n");
		err = -EIO;
	}
	if (!err)
		return devinfo->ctl_urb_actual_length;
	else
		return err;
}

static struct brcmf_usbreq *brcmf_usb_deq(struct brcmf_usbdev_info *devinfo,
					  struct list_head *q, int *counter)
{
	unsigned long flags;
	struct brcmf_usbreq  *req;
	spin_lock_irqsave(&devinfo->qlock, flags);
	if (list_empty(q)) {
		spin_unlock_irqrestore(&devinfo->qlock, flags);
		return NULL;
	}
	req = list_entry(q->next, struct brcmf_usbreq, list);
	list_del_init(q->next);
	if (counter)
		(*counter)--;
	spin_unlock_irqrestore(&devinfo->qlock, flags);
	return req;

}

static void brcmf_usb_enq(struct brcmf_usbdev_info *devinfo,
			  struct list_head *q, struct brcmf_usbreq *req,
			  int *counter)
{
	unsigned long flags;
	spin_lock_irqsave(&devinfo->qlock, flags);
	list_add_tail(&req->list, q);
	if (counter)
		(*counter)++;
	spin_unlock_irqrestore(&devinfo->qlock, flags);
}

static struct brcmf_usbreq *
brcmf_usbdev_qinit(struct list_head *q, int qsize)
{
	int i;
	struct brcmf_usbreq *req, *reqs;

	reqs = kcalloc(qsize, sizeof(struct brcmf_usbreq), GFP_ATOMIC);
	if (reqs == NULL)
		return NULL;

	req = reqs;

	for (i = 0; i < qsize; i++) {
		req->urb = usb_alloc_urb(0, GFP_ATOMIC);
		if (!req->urb)
			goto fail;

		INIT_LIST_HEAD(&req->list);
		list_add_tail(&req->list, q);
		req++;
	}
	return reqs;
fail:
	brcmf_err("fail!\n");
	while (!list_empty(q)) {
		req = list_entry(q->next, struct brcmf_usbreq, list);
		if (req && req->urb)
			usb_free_urb(req->urb);
		list_del(q->next);
	}
	return NULL;

}

static void brcmf_usb_free_q(struct list_head *q, bool pending)
{
	struct brcmf_usbreq *req, *next;
	int i = 0;
	list_for_each_entry_safe(req, next, q, list) {
		if (!req->urb) {
			brcmf_err("bad req\n");
			break;
		}
		i++;
		if (pending) {
			usb_kill_urb(req->urb);
		} else {
			usb_free_urb(req->urb);
			list_del_init(&req->list);
		}
	}
}

static void brcmf_usb_del_fromq(struct brcmf_usbdev_info *devinfo,
				struct brcmf_usbreq *req)
{
	unsigned long flags;

	spin_lock_irqsave(&devinfo->qlock, flags);
	list_del_init(&req->list);
	spin_unlock_irqrestore(&devinfo->qlock, flags);
}


static void brcmf_usb_tx_complete(struct urb *urb)
{
	struct brcmf_usbreq *req = (struct brcmf_usbreq *)urb->context;
	struct brcmf_usbdev_info *devinfo = req->devinfo;
	unsigned long flags;

	brcmf_dbg(USB, "Enter, urb->status=%d, skb=%p\n", urb->status,
		  req->skb);
	brcmf_usb_del_fromq(devinfo, req);

	brcmf_txcomplete(devinfo->dev, req->skb, urb->status == 0);
	req->skb = NULL;
	brcmf_usb_enq(devinfo, &devinfo->tx_freeq, req, &devinfo->tx_freecount);
	spin_lock_irqsave(&devinfo->tx_flowblock_lock, flags);
	if (devinfo->tx_freecount > devinfo->tx_high_watermark &&
		devinfo->tx_flowblock) {
		brcmf_txflowblock(devinfo->dev, false);
		devinfo->tx_flowblock = false;
	}
	spin_unlock_irqrestore(&devinfo->tx_flowblock_lock, flags);
}

static void brcmf_usb_rx_complete(struct urb *urb)
{
	struct brcmf_usbreq  *req = (struct brcmf_usbreq *)urb->context;
	struct brcmf_usbdev_info *devinfo = req->devinfo;
	struct sk_buff *skb;

	brcmf_dbg(USB, "Enter, urb->status=%d\n", urb->status);
	brcmf_usb_del_fromq(devinfo, req);
	skb = req->skb;
	req->skb = NULL;

	/* zero lenght packets indicate usb "failure". Do not refill */
	if (urb->status != 0 || !urb->actual_length) {
		brcmu_pkt_buf_free_skb(skb);
		brcmf_usb_enq(devinfo, &devinfo->rx_freeq, req, NULL);
		return;
	}

	if (devinfo->bus_pub.state == BRCMFMAC_USB_STATE_UP) {
		skb_put(skb, urb->actual_length);
		brcmf_rx_frame(devinfo->dev, skb);
		brcmf_usb_rx_refill(devinfo, req);
	} else {
		brcmu_pkt_buf_free_skb(skb);
		brcmf_usb_enq(devinfo, &devinfo->rx_freeq, req, NULL);
	}
	return;

}

static void brcmf_usb_rx_refill(struct brcmf_usbdev_info *devinfo,
				struct brcmf_usbreq  *req)
{
	struct sk_buff *skb;
	int ret;

	if (!req || !devinfo)
		return;

	skb = dev_alloc_skb(devinfo->bus_pub.bus_mtu);
	if (!skb) {
		brcmf_usb_enq(devinfo, &devinfo->rx_freeq, req, NULL);
		return;
	}
	req->skb = skb;

	usb_fill_bulk_urb(req->urb, devinfo->usbdev, devinfo->rx_pipe,
			  skb->data, skb_tailroom(skb), brcmf_usb_rx_complete,
			  req);
	req->devinfo = devinfo;
	brcmf_usb_enq(devinfo, &devinfo->rx_postq, req, NULL);

	ret = usb_submit_urb(req->urb, GFP_ATOMIC);
	if (ret) {
		brcmf_usb_del_fromq(devinfo, req);
		brcmu_pkt_buf_free_skb(req->skb);
		req->skb = NULL;
		brcmf_usb_enq(devinfo, &devinfo->rx_freeq, req, NULL);
	}
	return;
}

static void brcmf_usb_rx_fill_all(struct brcmf_usbdev_info *devinfo)
{
	struct brcmf_usbreq *req;

	if (devinfo->bus_pub.state != BRCMFMAC_USB_STATE_UP) {
		brcmf_err("bus is not up=%d\n", devinfo->bus_pub.state);
		return;
	}
	while ((req = brcmf_usb_deq(devinfo, &devinfo->rx_freeq, NULL)) != NULL)
		brcmf_usb_rx_refill(devinfo, req);
}

static void
brcmf_usb_state_change(struct brcmf_usbdev_info *devinfo, int state)
{
	struct brcmf_bus *bcmf_bus = devinfo->bus_pub.bus;
	int old_state;

	brcmf_dbg(USB, "Enter, current state=%d, new state=%d\n",
		  devinfo->bus_pub.state, state);

	if (devinfo->bus_pub.state == state)
		return;

	old_state = devinfo->bus_pub.state;
	devinfo->bus_pub.state = state;

	/* update state of upper layer */
	if (state == BRCMFMAC_USB_STATE_DOWN) {
		brcmf_dbg(USB, "DBUS is down\n");
		brcmf_bus_change_state(bcmf_bus, BRCMF_BUS_DOWN);
	} else if (state == BRCMFMAC_USB_STATE_UP) {
		brcmf_dbg(USB, "DBUS is up\n");
		brcmf_bus_change_state(bcmf_bus, BRCMF_BUS_DATA);
	} else {
		brcmf_dbg(USB, "DBUS current state=%d\n", state);
	}
}

static int brcmf_usb_tx(struct device *dev, struct sk_buff *skb)
{
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(dev);
	struct brcmf_usbreq  *req;
	int ret;
	unsigned long flags;

	brcmf_dbg(USB, "Enter, skb=%p\n", skb);
	if (devinfo->bus_pub.state != BRCMFMAC_USB_STATE_UP) {
		ret = -EIO;
		goto fail;
	}

	req = brcmf_usb_deq(devinfo, &devinfo->tx_freeq,
					&devinfo->tx_freecount);
	if (!req) {
		brcmf_err("no req to send\n");
		ret = -ENOMEM;
		goto fail;
	}

	req->skb = skb;
	req->devinfo = devinfo;
	usb_fill_bulk_urb(req->urb, devinfo->usbdev, devinfo->tx_pipe,
			  skb->data, skb->len, brcmf_usb_tx_complete, req);
	req->urb->transfer_flags |= URB_ZERO_PACKET;
	brcmf_usb_enq(devinfo, &devinfo->tx_postq, req, NULL);
	ret = usb_submit_urb(req->urb, GFP_ATOMIC);
	if (ret) {
		brcmf_err("brcmf_usb_tx usb_submit_urb FAILED\n");
		brcmf_usb_del_fromq(devinfo, req);
		req->skb = NULL;
		brcmf_usb_enq(devinfo, &devinfo->tx_freeq, req,
			      &devinfo->tx_freecount);
		goto fail;
	}

	spin_lock_irqsave(&devinfo->tx_flowblock_lock, flags);
	if (devinfo->tx_freecount < devinfo->tx_low_watermark &&
	    !devinfo->tx_flowblock) {
		brcmf_txflowblock(dev, true);
		devinfo->tx_flowblock = true;
	}
	spin_unlock_irqrestore(&devinfo->tx_flowblock_lock, flags);
	return 0;

fail:
	return ret;
}


static int brcmf_usb_up(struct device *dev)
{
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(dev);

	brcmf_dbg(USB, "Enter\n");
	if (devinfo->bus_pub.state == BRCMFMAC_USB_STATE_UP)
		return 0;

	/* Success, indicate devinfo is fully up */
	brcmf_usb_state_change(devinfo, BRCMFMAC_USB_STATE_UP);

	if (devinfo->ctl_urb) {
		devinfo->ctl_in_pipe = usb_rcvctrlpipe(devinfo->usbdev, 0);
		devinfo->ctl_out_pipe = usb_sndctrlpipe(devinfo->usbdev, 0);

		/* CTL Write */
		devinfo->ctl_write.bRequestType =
			USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		devinfo->ctl_write.bRequest = 0;
		devinfo->ctl_write.wValue = cpu_to_le16(0);
		devinfo->ctl_write.wIndex = cpu_to_le16(devinfo->ifnum);

		/* CTL Read */
		devinfo->ctl_read.bRequestType =
			USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE;
		devinfo->ctl_read.bRequest = 1;
		devinfo->ctl_read.wValue = cpu_to_le16(0);
		devinfo->ctl_read.wIndex = cpu_to_le16(devinfo->ifnum);
	}
	brcmf_usb_rx_fill_all(devinfo);
	return 0;
}

static void brcmf_usb_down(struct device *dev)
{
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(dev);

	brcmf_dbg(USB, "Enter\n");
	if (devinfo == NULL)
		return;

	if (devinfo->bus_pub.state == BRCMFMAC_USB_STATE_DOWN)
		return;

	brcmf_usb_state_change(devinfo, BRCMFMAC_USB_STATE_DOWN);

	if (devinfo->ctl_urb)
		usb_kill_urb(devinfo->ctl_urb);

	if (devinfo->bulk_urb)
		usb_kill_urb(devinfo->bulk_urb);
	brcmf_usb_free_q(&devinfo->tx_postq, true);

	brcmf_usb_free_q(&devinfo->rx_postq, true);
}

static void
brcmf_usb_sync_complete(struct urb *urb)
{
	struct brcmf_usbdev_info *devinfo =
			(struct brcmf_usbdev_info *)urb->context;

	devinfo->ctl_completed = true;
	brcmf_usb_ioctl_resp_wake(devinfo);
}

static int brcmf_usb_dl_cmd(struct brcmf_usbdev_info *devinfo, u8 cmd,
			    void *buffer, int buflen)
{
	int ret;
	char *tmpbuf;
	u16 size;

	if ((!devinfo) || (devinfo->ctl_urb == NULL))
		return -EINVAL;

	tmpbuf = kmalloc(buflen, GFP_ATOMIC);
	if (!tmpbuf)
		return -ENOMEM;

	size = buflen;
	devinfo->ctl_urb->transfer_buffer_length = size;

	devinfo->ctl_read.wLength = cpu_to_le16p(&size);
	devinfo->ctl_read.bRequestType = USB_DIR_IN | USB_TYPE_VENDOR |
		USB_RECIP_INTERFACE;
	devinfo->ctl_read.bRequest = cmd;

	usb_fill_control_urb(devinfo->ctl_urb,
		devinfo->usbdev,
		usb_rcvctrlpipe(devinfo->usbdev, 0),
		(unsigned char *) &devinfo->ctl_read,
		(void *) tmpbuf, size,
		(usb_complete_t)brcmf_usb_sync_complete, devinfo);

	devinfo->ctl_completed = false;
	ret = usb_submit_urb(devinfo->ctl_urb, GFP_ATOMIC);
	if (ret < 0) {
		brcmf_err("usb_submit_urb failed %d\n", ret);
		goto finalize;
	}

	if (!brcmf_usb_ioctl_resp_wait(devinfo))
		ret = -ETIMEDOUT;
	else
		memcpy(buffer, tmpbuf, buflen);

finalize:
	kfree(tmpbuf);
	return ret;
}

static bool
brcmf_usb_dlneeded(struct brcmf_usbdev_info *devinfo)
{
	struct bootrom_id_le id;
	u32 chipid, chiprev;

	brcmf_dbg(USB, "Enter\n");

	if (devinfo == NULL)
		return false;

	/* Check if firmware downloaded already by querying runtime ID */
	id.chip = cpu_to_le32(0xDEAD);
	brcmf_usb_dl_cmd(devinfo, DL_GETVER, &id, sizeof(id));

	chipid = le32_to_cpu(id.chip);
	chiprev = le32_to_cpu(id.chiprev);

	if ((chipid & 0x4300) == 0x4300)
		brcmf_dbg(USB, "chip %x rev 0x%x\n", chipid, chiprev);
	else
		brcmf_dbg(USB, "chip %d rev 0x%x\n", chipid, chiprev);
	if (chipid == BRCMF_POSTBOOT_ID) {
		brcmf_dbg(USB, "firmware already downloaded\n");
		brcmf_usb_dl_cmd(devinfo, DL_RESETCFG, &id, sizeof(id));
		return false;
	} else {
		devinfo->bus_pub.devid = chipid;
		devinfo->bus_pub.chiprev = chiprev;
	}
	return true;
}

static int
brcmf_usb_resetcfg(struct brcmf_usbdev_info *devinfo)
{
	struct bootrom_id_le id;
	u32 loop_cnt;
	int err;

	brcmf_dbg(USB, "Enter\n");

	loop_cnt = 0;
	do {
		mdelay(BRCMF_USB_RESET_GETVER_SPINWAIT);
		loop_cnt++;
		id.chip = cpu_to_le32(0xDEAD);       /* Get the ID */
		err = brcmf_usb_dl_cmd(devinfo, DL_GETVER, &id, sizeof(id));
		if ((err) && (err != -ETIMEDOUT))
			return err;
		if (id.chip == cpu_to_le32(BRCMF_POSTBOOT_ID))
			break;
	} while (loop_cnt < BRCMF_USB_RESET_GETVER_LOOP_CNT);

	if (id.chip == cpu_to_le32(BRCMF_POSTBOOT_ID)) {
		brcmf_dbg(USB, "postboot chip 0x%x/rev 0x%x\n",
			  le32_to_cpu(id.chip), le32_to_cpu(id.chiprev));

		brcmf_usb_dl_cmd(devinfo, DL_RESETCFG, &id, sizeof(id));
		return 0;
	} else {
		brcmf_err("Cannot talk to Dongle. Firmware is not UP, %d ms\n",
			  BRCMF_USB_RESET_GETVER_SPINWAIT * loop_cnt);
		return -EINVAL;
	}
}


static int
brcmf_usb_dl_send_bulk(struct brcmf_usbdev_info *devinfo, void *buffer, int len)
{
	int ret;

	if ((devinfo == NULL) || (devinfo->bulk_urb == NULL))
		return -EINVAL;

	/* Prepare the URB */
	usb_fill_bulk_urb(devinfo->bulk_urb, devinfo->usbdev,
			  devinfo->tx_pipe, buffer, len,
			  (usb_complete_t)brcmf_usb_sync_complete, devinfo);

	devinfo->bulk_urb->transfer_flags |= URB_ZERO_PACKET;

	devinfo->ctl_completed = false;
	ret = usb_submit_urb(devinfo->bulk_urb, GFP_ATOMIC);
	if (ret) {
		brcmf_err("usb_submit_urb failed %d\n", ret);
		return ret;
	}
	ret = brcmf_usb_ioctl_resp_wait(devinfo);
	return (ret == 0);
}

static int
brcmf_usb_dl_writeimage(struct brcmf_usbdev_info *devinfo, u8 *fw, int fwlen)
{
	unsigned int sendlen, sent, dllen;
	char *bulkchunk = NULL, *dlpos;
	struct rdl_state_le state;
	u32 rdlstate, rdlbytes;
	int err = 0;

	brcmf_dbg(USB, "Enter, fw %p, len %d\n", fw, fwlen);

	bulkchunk = kmalloc(RDL_CHUNK, GFP_ATOMIC);
	if (bulkchunk == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	/* 1) Prepare USB boot loader for runtime image */
	brcmf_usb_dl_cmd(devinfo, DL_START, &state, sizeof(state));

	rdlstate = le32_to_cpu(state.state);
	rdlbytes = le32_to_cpu(state.bytes);

	/* 2) Check we are in the Waiting state */
	if (rdlstate != DL_WAITING) {
		brcmf_err("Failed to DL_START\n");
		err = -EINVAL;
		goto fail;
	}
	sent = 0;
	dlpos = fw;
	dllen = fwlen;

	/* Get chip id and rev */
	while (rdlbytes != dllen) {
		/* Wait until the usb device reports it received all
		 * the bytes we sent */
		if ((rdlbytes == sent) && (rdlbytes != dllen)) {
			if ((dllen-sent) < RDL_CHUNK)
				sendlen = dllen-sent;
			else
				sendlen = RDL_CHUNK;

			/* simply avoid having to send a ZLP by ensuring we
			 * never have an even
			 * multiple of 64
			 */
			if (!(sendlen % 64))
				sendlen -= 4;

			/* send data */
			memcpy(bulkchunk, dlpos, sendlen);
			if (brcmf_usb_dl_send_bulk(devinfo, bulkchunk,
						   sendlen)) {
				brcmf_err("send_bulk failed\n");
				err = -EINVAL;
				goto fail;
			}

			dlpos += sendlen;
			sent += sendlen;
		}
		err = brcmf_usb_dl_cmd(devinfo, DL_GETSTATE, &state,
				       sizeof(state));
		if (err) {
			brcmf_err("DL_GETSTATE Failed\n");
			goto fail;
		}

		rdlstate = le32_to_cpu(state.state);
		rdlbytes = le32_to_cpu(state.bytes);

		/* restart if an error is reported */
		if (rdlstate == DL_BAD_HDR || rdlstate == DL_BAD_CRC) {
			brcmf_err("Bad Hdr or Bad CRC state %d\n",
				  rdlstate);
			err = -EINVAL;
			goto fail;
		}
	}

fail:
	kfree(bulkchunk);
	brcmf_dbg(USB, "Exit, err=%d\n", err);
	return err;
}

static int brcmf_usb_dlstart(struct brcmf_usbdev_info *devinfo, u8 *fw, int len)
{
	int err;

	brcmf_dbg(USB, "Enter\n");

	if (devinfo == NULL)
		return -EINVAL;

	if (devinfo->bus_pub.devid == 0xDEAD)
		return -EINVAL;

	err = brcmf_usb_dl_writeimage(devinfo, fw, len);
	if (err == 0)
		devinfo->bus_pub.state = BRCMFMAC_USB_STATE_DL_DONE;
	else
		devinfo->bus_pub.state = BRCMFMAC_USB_STATE_DL_FAIL;
	brcmf_dbg(USB, "Exit, err=%d\n", err);

	return err;
}

static int brcmf_usb_dlrun(struct brcmf_usbdev_info *devinfo)
{
	struct rdl_state_le state;

	brcmf_dbg(USB, "Enter\n");
	if (!devinfo)
		return -EINVAL;

	if (devinfo->bus_pub.devid == 0xDEAD)
		return -EINVAL;

	/* Check we are runnable */
	state.state = 0;
	brcmf_usb_dl_cmd(devinfo, DL_GETSTATE, &state, sizeof(state));

	/* Start the image */
	if (state.state == cpu_to_le32(DL_RUNNABLE)) {
		if (brcmf_usb_dl_cmd(devinfo, DL_GO, &state, sizeof(state)))
			return -ENODEV;
		if (brcmf_usb_resetcfg(devinfo))
			return -ENODEV;
		/* The Dongle may go for re-enumeration. */
	} else {
		brcmf_err("Dongle not runnable\n");
		return -EINVAL;
	}
	brcmf_dbg(USB, "Exit\n");
	return 0;
}

static bool brcmf_usb_chip_support(int chipid, int chiprev)
{
	switch(chipid) {
	case 43143:
		return true;
	case 43235:
	case 43236:
	case 43238:
		return (chiprev == 3);
	case 43242:
		return true;
	case 43566:
	case 43569:
		return true;
	default:
		break;
	}
	return false;
}

static int
brcmf_usb_fw_download(struct brcmf_usbdev_info *devinfo)
{
	int devid, chiprev;
	int err;

	brcmf_dbg(USB, "Enter\n");
	if (devinfo == NULL)
		return -ENODEV;

	devid = devinfo->bus_pub.devid;
	chiprev = devinfo->bus_pub.chiprev;

	if (!brcmf_usb_chip_support(devid, chiprev)) {
		brcmf_err("unsupported chip %d rev %d\n",
			  devid, chiprev);
		return -EINVAL;
	}

	if (!devinfo->image) {
		brcmf_err("No firmware!\n");
		return -ENOENT;
	}

	err = brcmf_usb_dlstart(devinfo,
		(u8 *)devinfo->image, devinfo->image_len);
	if (err == 0)
		err = brcmf_usb_dlrun(devinfo);
	return err;
}


static void brcmf_usb_detach(struct brcmf_usbdev_info *devinfo)
{
	brcmf_dbg(USB, "Enter, devinfo %p\n", devinfo);

	/* free the URBS */
	brcmf_usb_free_q(&devinfo->rx_freeq, false);
	brcmf_usb_free_q(&devinfo->tx_freeq, false);

	usb_free_urb(devinfo->ctl_urb);
	usb_free_urb(devinfo->bulk_urb);

	kfree(devinfo->tx_reqs);
	kfree(devinfo->rx_reqs);
}

#define TRX_MAGIC       0x30524448      /* "HDR0" */
#define TRX_VERSION     1               /* Version 1 */
#define TRX_MAX_LEN     0x3B0000        /* Max length */
#define TRX_NO_HEADER   1               /* Do not write TRX header */
#define TRX_MAX_OFFSET  3               /* Max number of individual files */
#define TRX_UNCOMP_IMAGE        0x20    /* Trx contains uncompressed image */

struct trx_header_le {
	__le32 magic;		/* "HDR0" */
	__le32 len;		/* Length of file including header */
	__le32 crc32;		/* CRC from flag_version to end of file */
	__le32 flag_version;	/* 0:15 flags, 16:31 version */
	__le32 offsets[TRX_MAX_OFFSET]; /* Offsets of partitions from start of
					 * header */
};

static int check_file(const u8 *headers)
{
	struct trx_header_le *trx;
	int actual_len = -1;

	brcmf_dbg(USB, "Enter\n");
	/* Extract trx header */
	trx = (struct trx_header_le *) headers;
	if (trx->magic != cpu_to_le32(TRX_MAGIC))
		return -1;

	headers += sizeof(struct trx_header_le);

	if (le32_to_cpu(trx->flag_version) & TRX_UNCOMP_IMAGE) {
		actual_len = le32_to_cpu(trx->offsets[TRX_OFFSETS_DLFWLEN_IDX]);
		return actual_len + sizeof(struct trx_header_le);
	}
	return -1;
}

static const char *brcmf_usb_get_fwname(struct brcmf_usbdev_info *devinfo)
{
	switch (devinfo->bus_pub.devid) {
	case 43143:
		return BRCMF_USB_43143_FW_NAME;
	case 43235:
	case 43236:
	case 43238:
		return BRCMF_USB_43236_FW_NAME;
	case 43242:
		return BRCMF_USB_43242_FW_NAME;
	case 43566:
	case 43569:
		return BRCMF_USB_43569_FW_NAME;
	default:
		return NULL;
	}
}


static
struct brcmf_usbdev *brcmf_usb_attach(struct brcmf_usbdev_info *devinfo,
				      int nrxq, int ntxq)
{
	brcmf_dbg(USB, "Enter\n");

	devinfo->bus_pub.nrxq = nrxq;
	devinfo->rx_low_watermark = nrxq / 2;
	devinfo->bus_pub.devinfo = devinfo;
	devinfo->bus_pub.ntxq = ntxq;
	devinfo->bus_pub.state = BRCMFMAC_USB_STATE_DOWN;

	/* flow control when too many tx urbs posted */
	devinfo->tx_low_watermark = ntxq / 4;
	devinfo->tx_high_watermark = devinfo->tx_low_watermark * 3;
	devinfo->bus_pub.bus_mtu = BRCMF_USB_MAX_PKT_SIZE;

	/* Initialize other structure content */
	init_waitqueue_head(&devinfo->ioctl_resp_wait);

	/* Initialize the spinlocks */
	spin_lock_init(&devinfo->qlock);
	spin_lock_init(&devinfo->tx_flowblock_lock);

	INIT_LIST_HEAD(&devinfo->rx_freeq);
	INIT_LIST_HEAD(&devinfo->rx_postq);

	INIT_LIST_HEAD(&devinfo->tx_freeq);
	INIT_LIST_HEAD(&devinfo->tx_postq);

	devinfo->tx_flowblock = false;

	devinfo->rx_reqs = brcmf_usbdev_qinit(&devinfo->rx_freeq, nrxq);
	if (!devinfo->rx_reqs)
		goto error;

	devinfo->tx_reqs = brcmf_usbdev_qinit(&devinfo->tx_freeq, ntxq);
	if (!devinfo->tx_reqs)
		goto error;
	devinfo->tx_freecount = ntxq;

	devinfo->ctl_urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!devinfo->ctl_urb) {
		brcmf_err("usb_alloc_urb (ctl) failed\n");
		goto error;
	}
	devinfo->bulk_urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!devinfo->bulk_urb) {
		brcmf_err("usb_alloc_urb (bulk) failed\n");
		goto error;
	}

	return &devinfo->bus_pub;

error:
	brcmf_err("failed!\n");
	brcmf_usb_detach(devinfo);
	return NULL;
}

static struct brcmf_bus_ops brcmf_usb_bus_ops = {
	.txdata = brcmf_usb_tx,
	.stop = brcmf_usb_down,
	.txctl = brcmf_usb_tx_ctlpkt,
	.rxctl = brcmf_usb_rx_ctlpkt,
};

static int brcmf_usb_bus_setup(struct brcmf_usbdev_info *devinfo)
{
	int ret;

	/* Attach to the common driver interface */
	ret = brcmf_attach(devinfo->dev);
	if (ret) {
		brcmf_err("brcmf_attach failed\n");
		return ret;
	}

	ret = brcmf_usb_up(devinfo->dev);
	if (ret)
		goto fail;

	ret = brcmf_bus_start(devinfo->dev);
	if (ret)
		goto fail;

	return 0;
fail:
	brcmf_detach(devinfo->dev);
	return ret;
}

static void brcmf_usb_probe_phase2(struct device *dev,
				   const struct firmware *fw,
				   void *nvram, u32 nvlen)
{
	struct brcmf_bus *bus = dev_get_drvdata(dev);
	struct brcmf_usbdev_info *devinfo;
	int ret;

	brcmf_dbg(USB, "Start fw downloading\n");
	ret = check_file(fw->data);
	if (ret < 0) {
		brcmf_err("invalid firmware\n");
		release_firmware(fw);
		goto error;
	}

	devinfo = bus->bus_priv.usb->devinfo;
	devinfo->image = fw->data;
	devinfo->image_len = fw->size;

	ret = brcmf_usb_fw_download(devinfo);
	release_firmware(fw);
	if (ret)
		goto error;

	ret = brcmf_usb_bus_setup(devinfo);
	if (ret)
		goto error;

	return;
error:
	brcmf_dbg(TRACE, "failed: dev=%s, err=%d\n", dev_name(dev), ret);
	device_release_driver(dev);
}

static int brcmf_usb_probe_cb(struct brcmf_usbdev_info *devinfo)
{
	struct brcmf_bus *bus = NULL;
	struct brcmf_usbdev *bus_pub = NULL;
	struct device *dev = devinfo->dev;
	int ret;

	brcmf_dbg(USB, "Enter\n");
	bus_pub = brcmf_usb_attach(devinfo, BRCMF_USB_NRXQ, BRCMF_USB_NTXQ);
	if (!bus_pub)
		return -ENODEV;

	bus = kzalloc(sizeof(struct brcmf_bus), GFP_ATOMIC);
	if (!bus) {
		ret = -ENOMEM;
		goto fail;
	}

	bus->dev = dev;
	bus_pub->bus = bus;
	bus->bus_priv.usb = bus_pub;
	dev_set_drvdata(dev, bus);
	bus->ops = &brcmf_usb_bus_ops;
	bus->chip = bus_pub->devid;
	bus->chiprev = bus_pub->chiprev;
	bus->proto_type = BRCMF_PROTO_BCDC;
	bus->always_use_fws_queue = true;

	if (!brcmf_usb_dlneeded(devinfo)) {
		ret = brcmf_usb_bus_setup(devinfo);
		if (ret)
			goto fail;
	}
	/* request firmware here */
	brcmf_fw_get_firmwares(dev, 0, brcmf_usb_get_fwname(devinfo), NULL,
			       brcmf_usb_probe_phase2);
	return 0;

fail:
	/* Release resources in reverse order */
	kfree(bus);
	brcmf_usb_detach(devinfo);
	return ret;
}

static void
brcmf_usb_disconnect_cb(struct brcmf_usbdev_info *devinfo)
{
	if (!devinfo)
		return;
	brcmf_dbg(USB, "Enter, bus_pub %p\n", devinfo);

	brcmf_detach(devinfo->dev);
	kfree(devinfo->bus_pub.bus);
	brcmf_usb_detach(devinfo);
}

static int
brcmf_usb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct brcmf_usbdev_info *devinfo;
	struct usb_interface_descriptor	*desc;
	struct usb_endpoint_descriptor *endpoint;
	int ret = 0;
	u32 num_of_eps;
	u8 endpoint_num, ep;

	brcmf_dbg(USB, "Enter 0x%04x:0x%04x\n", id->idVendor, id->idProduct);

	devinfo = kzalloc(sizeof(*devinfo), GFP_ATOMIC);
	if (devinfo == NULL)
		return -ENOMEM;

	devinfo->usbdev = usb;
	devinfo->dev = &usb->dev;
	usb_set_intfdata(intf, devinfo);

	/* Check that the device supports only one configuration */
	if (usb->descriptor.bNumConfigurations != 1) {
		brcmf_err("Number of configurations: %d not supported\n",
			  usb->descriptor.bNumConfigurations);
		ret = -ENODEV;
		goto fail;
	}

	if ((usb->descriptor.bDeviceClass != USB_CLASS_VENDOR_SPEC) &&
	    (usb->descriptor.bDeviceClass != USB_CLASS_MISC) &&
	    (usb->descriptor.bDeviceClass != USB_CLASS_WIRELESS_CONTROLLER)) {
		brcmf_err("Device class: 0x%x not supported\n",
			  usb->descriptor.bDeviceClass);
		ret = -ENODEV;
		goto fail;
	}

	desc = &intf->altsetting[0].desc;
	if ((desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
	    (desc->bInterfaceSubClass != 2) ||
	    (desc->bInterfaceProtocol != 0xff)) {
		brcmf_err("non WLAN interface %d: 0x%x:0x%x:0x%x\n",
			  desc->bInterfaceNumber, desc->bInterfaceClass,
			  desc->bInterfaceSubClass, desc->bInterfaceProtocol);
		ret = -ENODEV;
		goto fail;
	}

	num_of_eps = desc->bNumEndpoints;
	for (ep = 0; ep < num_of_eps; ep++) {
		endpoint = &intf->altsetting[0].endpoint[ep].desc;
		endpoint_num = usb_endpoint_num(endpoint);
		if (!usb_endpoint_xfer_bulk(endpoint))
			continue;
		if (usb_endpoint_dir_in(endpoint)) {
			if (!devinfo->rx_pipe)
				devinfo->rx_pipe =
					usb_rcvbulkpipe(usb, endpoint_num);
		} else {
			if (!devinfo->tx_pipe)
				devinfo->tx_pipe =
					usb_sndbulkpipe(usb, endpoint_num);
		}
	}
	if (devinfo->rx_pipe == 0) {
		brcmf_err("No RX (in) Bulk EP found\n");
		ret = -ENODEV;
		goto fail;
	}
	if (devinfo->tx_pipe == 0) {
		brcmf_err("No TX (out) Bulk EP found\n");
		ret = -ENODEV;
		goto fail;
	}

	devinfo->ifnum = desc->bInterfaceNumber;

	if (usb->speed == USB_SPEED_SUPER)
		brcmf_dbg(USB, "Broadcom super speed USB WLAN interface detected\n");
	else if (usb->speed == USB_SPEED_HIGH)
		brcmf_dbg(USB, "Broadcom high speed USB WLAN interface detected\n");
	else
		brcmf_dbg(USB, "Broadcom full speed USB WLAN interface detected\n");

	ret = brcmf_usb_probe_cb(devinfo);
	if (ret)
		goto fail;

	/* Success */
	return 0;

fail:
	kfree(devinfo);
	usb_set_intfdata(intf, NULL);
	return ret;
}

static void
brcmf_usb_disconnect(struct usb_interface *intf)
{
	struct brcmf_usbdev_info *devinfo;

	brcmf_dbg(USB, "Enter\n");
	devinfo = (struct brcmf_usbdev_info *)usb_get_intfdata(intf);
	brcmf_usb_disconnect_cb(devinfo);
	kfree(devinfo);
	brcmf_dbg(USB, "Exit\n");
}

/*
 * only need to signal the bus being down and update the state.
 */
static int brcmf_usb_suspend(struct usb_interface *intf, pm_message_t state)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(&usb->dev);

	brcmf_dbg(USB, "Enter\n");
	devinfo->bus_pub.state = BRCMFMAC_USB_STATE_SLEEP;
	brcmf_detach(&usb->dev);
	return 0;
}

/*
 * (re-) start the bus.
 */
static int brcmf_usb_resume(struct usb_interface *intf)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(&usb->dev);

	brcmf_dbg(USB, "Enter\n");
	return brcmf_usb_bus_setup(devinfo);
}

static int brcmf_usb_reset_resume(struct usb_interface *intf)
{
	struct usb_device *usb = interface_to_usbdev(intf);
	struct brcmf_usbdev_info *devinfo = brcmf_usb_get_businfo(&usb->dev);

	brcmf_dbg(USB, "Enter\n");

	return brcmf_fw_get_firmwares(&usb->dev, 0,
				      brcmf_usb_get_fwname(devinfo), NULL,
				      brcmf_usb_probe_phase2);
}

#define BRCMF_USB_VENDOR_ID_BROADCOM	0x0a5c
#define BRCMF_USB_DEVICE_ID_43143	0xbd1e
#define BRCMF_USB_DEVICE_ID_43236	0xbd17
#define BRCMF_USB_DEVICE_ID_43242	0xbd1f
#define BRCMF_USB_DEVICE_ID_43569	0xbd27
#define BRCMF_USB_DEVICE_ID_BCMFW	0x0bdc

static struct usb_device_id brcmf_usb_devid_table[] = {
	{ USB_DEVICE(BRCMF_USB_VENDOR_ID_BROADCOM, BRCMF_USB_DEVICE_ID_43143) },
	{ USB_DEVICE(BRCMF_USB_VENDOR_ID_BROADCOM, BRCMF_USB_DEVICE_ID_43236) },
	{ USB_DEVICE(BRCMF_USB_VENDOR_ID_BROADCOM, BRCMF_USB_DEVICE_ID_43242) },
	{ USB_DEVICE(BRCMF_USB_VENDOR_ID_BROADCOM, BRCMF_USB_DEVICE_ID_43569) },
	/* special entry for device with firmware loaded and running */
	{ USB_DEVICE(BRCMF_USB_VENDOR_ID_BROADCOM, BRCMF_USB_DEVICE_ID_BCMFW) },
	{ }
};

MODULE_DEVICE_TABLE(usb, brcmf_usb_devid_table);
MODULE_FIRMWARE(BRCMF_USB_43143_FW_NAME);
MODULE_FIRMWARE(BRCMF_USB_43236_FW_NAME);
MODULE_FIRMWARE(BRCMF_USB_43242_FW_NAME);
MODULE_FIRMWARE(BRCMF_USB_43569_FW_NAME);

static struct usb_driver brcmf_usbdrvr = {
	.name = KBUILD_MODNAME,
	.probe = brcmf_usb_probe,
	.disconnect = brcmf_usb_disconnect,
	.id_table = brcmf_usb_devid_table,
	.suspend = brcmf_usb_suspend,
	.resume = brcmf_usb_resume,
	.reset_resume = brcmf_usb_reset_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

static int brcmf_usb_reset_device(struct device *dev, void *notused)
{
	/* device past is the usb interface so we
	 * need to use parent here.
	 */
	brcmf_dev_reset(dev->parent);
	return 0;
}

void brcmf_usb_exit(void)
{
	struct device_driver *drv = &brcmf_usbdrvr.drvwrap.driver;
	int ret;

	brcmf_dbg(USB, "Enter\n");
	ret = driver_for_each_device(drv, NULL, NULL,
				     brcmf_usb_reset_device);
	usb_deregister(&brcmf_usbdrvr);
}

void brcmf_usb_register(void)
{
	brcmf_dbg(USB, "Enter\n");
	usb_register(&brcmf_usbdrvr);
}
