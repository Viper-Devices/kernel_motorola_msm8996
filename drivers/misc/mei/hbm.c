/*
 *
 * Intel Management Engine Interface (Intel MEI) Linux driver
 * Copyright (c) 2003-2012, Intel Corporation.
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
 */

#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mei.h>

#include "mei_dev.h"
#include "interface.h"

/**
 * mei_hbm_cl_hdr - construct client hbm header
 * @cl: - client
 * @hbm_cmd: host bus message command
 * @buf: buffer for cl header
 * @len: buffer length
 */
static inline
void mei_hbm_cl_hdr(struct mei_cl *cl, u8 hbm_cmd, void *buf, size_t len)
{
	struct mei_hbm_cl_cmd *cmd = buf;

	memset(cmd, 0, len);

	cmd->hbm_cmd = hbm_cmd;
	cmd->host_addr = cl->host_client_id;
	cmd->me_addr = cl->me_client_id;
}

/**
 * same_disconn_addr - tells if they have the same address
 *
 * @file: private data of the file object.
 * @disconn: disconnection request.
 *
 * returns true if addres are same
 */
static inline
bool mei_hbm_cl_addr_equal(struct mei_cl *cl, void *buf)
{
	struct mei_hbm_cl_cmd *cmd = buf;
	return cl->host_client_id == cmd->host_addr &&
		cl->me_client_id == cmd->me_addr;
}


/**
 * host_start_message - mei host sends start message.
 *
 * @dev: the device structure
 *
 * returns none.
 */
void mei_host_start_message(struct mei_device *dev)
{
	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	struct hbm_host_version_request *start_req;
	const size_t len = sizeof(struct hbm_host_version_request);

	mei_hbm_hdr(mei_hdr, len);

	/* host start message */
	start_req = (struct hbm_host_version_request *)dev->wr_msg.data;
	memset(start_req, 0, len);
	start_req->hbm_cmd = HOST_START_REQ_CMD;
	start_req->host_version.major_version = HBM_MAJOR_VERSION;
	start_req->host_version.minor_version = HBM_MINOR_VERSION;

	dev->recvd_msg = false;
	if (mei_write_message(dev, mei_hdr, dev->wr_msg.data)) {
		dev_dbg(&dev->pdev->dev, "write send version message to FW fail.\n");
		dev->dev_state = MEI_DEV_RESETING;
		mei_reset(dev, 1);
	}
	dev->init_clients_state = MEI_START_MESSAGE;
	dev->init_clients_timer = MEI_CLIENTS_INIT_TIMEOUT;
	return ;
}

/**
 * host_enum_clients_message - host sends enumeration client request message.
 *
 * @dev: the device structure
 *
 * returns none.
 */
void mei_host_enum_clients_message(struct mei_device *dev)
{
	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	struct hbm_host_enum_request *enum_req;
	const size_t len = sizeof(struct hbm_host_enum_request);
	/* enumerate clients */
	mei_hbm_hdr(mei_hdr, len);

	enum_req = (struct hbm_host_enum_request *)dev->wr_msg.data;
	memset(enum_req, 0, len);
	enum_req->hbm_cmd = HOST_ENUM_REQ_CMD;

	if (mei_write_message(dev, mei_hdr, dev->wr_msg.data)) {
		dev->dev_state = MEI_DEV_RESETING;
		dev_dbg(&dev->pdev->dev, "write send enumeration request message to FW fail.\n");
		mei_reset(dev, 1);
	}
	dev->init_clients_state = MEI_ENUM_CLIENTS_MESSAGE;
	dev->init_clients_timer = MEI_CLIENTS_INIT_TIMEOUT;
	return;
}


int mei_host_client_enumerate(struct mei_device *dev)
{

	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	struct hbm_props_request *prop_req;
	const size_t len = sizeof(struct hbm_props_request);
	unsigned long next_client_index;
	u8 client_num;


	client_num = dev->me_client_presentation_num;

	next_client_index = find_next_bit(dev->me_clients_map, MEI_CLIENTS_MAX,
					  dev->me_client_index);

	/* We got all client properties */
	if (next_client_index == MEI_CLIENTS_MAX) {
		schedule_work(&dev->init_work);

		return 0;
	}

	dev->me_clients[client_num].client_id = next_client_index;
	dev->me_clients[client_num].mei_flow_ctrl_creds = 0;

	mei_hbm_hdr(mei_hdr, len);
	prop_req = (struct hbm_props_request *)dev->wr_msg.data;

	memset(prop_req, 0, sizeof(struct hbm_props_request));


	prop_req->hbm_cmd = HOST_CLIENT_PROPERTIES_REQ_CMD;
	prop_req->address = next_client_index;

	if (mei_write_message(dev, mei_hdr, dev->wr_msg.data)) {
		dev->dev_state = MEI_DEV_RESETING;
		dev_err(&dev->pdev->dev, "Properties request command failed\n");
		mei_reset(dev, 1);

		return -EIO;
	}

	dev->init_clients_timer = MEI_CLIENTS_INIT_TIMEOUT;
	dev->me_client_index = next_client_index;

	return 0;
}

/**
 * mei_hbm_stop_req_prepare - perpare stop request message
 *
 * @dev - mei device
 * @mei_hdr - mei message header
 * @data - hbm message body buffer
 */
static void mei_hbm_stop_req_prepare(struct mei_device *dev,
		struct mei_msg_hdr *mei_hdr, unsigned char *data)
{
	struct hbm_host_stop_request *req =
			(struct hbm_host_stop_request *)data;
	const size_t len = sizeof(struct hbm_host_stop_request);

	mei_hbm_hdr(mei_hdr, len);

	memset(req, 0, len);
	req->hbm_cmd = HOST_STOP_REQ_CMD;
	req->reason = DRIVER_STOP_REQUEST;
}

/**
 * mei_send_flow_control - sends flow control to fw.
 *
 * @dev: the device structure
 * @cl: private data of the file object
 *
 * This function returns -EIO on write failure
 */
int mei_send_flow_control(struct mei_device *dev, struct mei_cl *cl)
{
	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	const size_t len = sizeof(struct hbm_flow_control);

	mei_hbm_hdr(mei_hdr, len);
	mei_hbm_cl_hdr(cl, MEI_FLOW_CONTROL_CMD, dev->wr_msg.data, len);

	dev_dbg(&dev->pdev->dev, "sending flow control host client = %d, ME client = %d\n",
		cl->host_client_id, cl->me_client_id);

	return mei_write_message(dev, mei_hdr, dev->wr_msg.data);
}

/**
 * mei_disconnect - sends disconnect message to fw.
 *
 * @dev: the device structure
 * @cl: private data of the file object
 *
 * This function returns -EIO on write failure
 */
int mei_disconnect(struct mei_device *dev, struct mei_cl *cl)
{
	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	const size_t len = sizeof(struct hbm_client_connect_request);

	mei_hbm_hdr(mei_hdr, len);
	mei_hbm_cl_hdr(cl, CLIENT_DISCONNECT_REQ_CMD, dev->wr_msg.data, len);

	return mei_write_message(dev, mei_hdr, dev->wr_msg.data);
}

/**
 * mei_connect - sends connect message to fw.
 *
 * @dev: the device structure
 * @cl: private data of the file object
 *
 * This function returns -EIO on write failure
 */
int mei_connect(struct mei_device *dev, struct mei_cl *cl)
{
	struct mei_msg_hdr *mei_hdr = &dev->wr_msg.hdr;
	const size_t len = sizeof(struct hbm_client_connect_request);

	mei_hbm_hdr(mei_hdr, len);
	mei_hbm_cl_hdr(cl, CLIENT_CONNECT_REQ_CMD, dev->wr_msg.data, len);

	return mei_write_message(dev, mei_hdr,  dev->wr_msg.data);
}

/**
 * mei_client_disconnect_request - disconnects from request irq routine
 *
 * @dev: the device structure.
 * @disconnect_req: disconnect request bus message.
 */
static void mei_client_disconnect_request(struct mei_device *dev,
		struct hbm_client_connect_request *disconnect_req)
{
	struct mei_cl *cl, *next;
	const size_t len = sizeof(struct hbm_client_connect_response);

	list_for_each_entry_safe(cl, next, &dev->file_list, link) {
		if (mei_hbm_cl_addr_equal(cl, disconnect_req)) {
			dev_dbg(&dev->pdev->dev, "disconnect request host client %d ME client %d.\n",
					disconnect_req->host_addr,
					disconnect_req->me_addr);
			cl->state = MEI_FILE_DISCONNECTED;
			cl->timer_count = 0;
			if (cl == &dev->wd_cl)
				dev->wd_pending = false;
			else if (cl == &dev->iamthif_cl)
				dev->iamthif_timer = 0;

			/* prepare disconnect response */
			mei_hbm_hdr(&dev->wr_ext_msg.hdr, len);
			mei_hbm_cl_hdr(cl, CLIENT_DISCONNECT_RES_CMD,
					 dev->wr_ext_msg.data, len);
			break;
		}
	}
}


/**
 * mei_hbm_dispatch - bottom half read routine after ISR to
 * handle the read bus message cmd processing.
 *
 * @dev: the device structure
 * @mei_hdr: header of bus message
 */
void mei_hbm_dispatch(struct mei_device *dev, struct mei_msg_hdr *hdr)
{
	struct mei_bus_message *mei_msg;
	struct mei_me_client *me_client;
	struct hbm_host_version_response *version_res;
	struct hbm_client_connect_response *connect_res;
	struct hbm_client_connect_response *disconnect_res;
	struct hbm_client_connect_request *disconnect_req;
	struct hbm_flow_control *flow_control;
	struct hbm_props_response *props_res;
	struct hbm_host_enum_response *enum_res;

	/* read the message to our buffer */
	BUG_ON(hdr->length >= sizeof(dev->rd_msg_buf));
	mei_read_slots(dev, dev->rd_msg_buf, hdr->length);
	mei_msg = (struct mei_bus_message *)dev->rd_msg_buf;

	switch (mei_msg->hbm_cmd) {
	case HOST_START_RES_CMD:
		version_res = (struct hbm_host_version_response *)mei_msg;
		if (!version_res->host_version_supported) {
			dev->version = version_res->me_max_version;
			dev_dbg(&dev->pdev->dev, "version mismatch.\n");

			mei_hbm_stop_req_prepare(dev, &dev->wr_msg.hdr,
						dev->wr_msg.data);
			mei_write_message(dev, &dev->wr_msg.hdr,
					dev->wr_msg.data);
			return;
		}

		dev->version.major_version = HBM_MAJOR_VERSION;
		dev->version.minor_version = HBM_MINOR_VERSION;
		if (dev->dev_state == MEI_DEV_INIT_CLIENTS &&
		    dev->init_clients_state == MEI_START_MESSAGE) {
			dev->init_clients_timer = 0;
			mei_host_enum_clients_message(dev);
		} else {
			dev->recvd_msg = false;
			dev_dbg(&dev->pdev->dev, "reset due to received hbm: host start\n");
			mei_reset(dev, 1);
			return;
		}

		dev->recvd_msg = true;
		dev_dbg(&dev->pdev->dev, "host start response message received.\n");
		break;

	case CLIENT_CONNECT_RES_CMD:
		connect_res = (struct hbm_client_connect_response *) mei_msg;
		mei_client_connect_response(dev, connect_res);
		dev_dbg(&dev->pdev->dev, "client connect response message received.\n");
		wake_up(&dev->wait_recvd_msg);
		break;

	case CLIENT_DISCONNECT_RES_CMD:
		disconnect_res = (struct hbm_client_connect_response *) mei_msg;
		mei_client_disconnect_response(dev, disconnect_res);
		dev_dbg(&dev->pdev->dev, "client disconnect response message received.\n");
		wake_up(&dev->wait_recvd_msg);
		break;

	case MEI_FLOW_CONTROL_CMD:
		flow_control = (struct hbm_flow_control *) mei_msg;
		mei_client_flow_control_response(dev, flow_control);
		dev_dbg(&dev->pdev->dev, "client flow control response message received.\n");
		break;

	case HOST_CLIENT_PROPERTIES_RES_CMD:
		props_res = (struct hbm_props_response *)mei_msg;
		me_client = &dev->me_clients[dev->me_client_presentation_num];

		if (props_res->status || !dev->me_clients) {
			dev_dbg(&dev->pdev->dev, "reset due to received host client properties response bus message wrong status.\n");
			mei_reset(dev, 1);
			return;
		}

		if (me_client->client_id != props_res->address) {
			dev_err(&dev->pdev->dev,
				"Host client properties reply mismatch\n");
			mei_reset(dev, 1);

			return;
		}

		if (dev->dev_state != MEI_DEV_INIT_CLIENTS ||
		    dev->init_clients_state != MEI_CLIENT_PROPERTIES_MESSAGE) {
			dev_err(&dev->pdev->dev,
				"Unexpected client properties reply\n");
			mei_reset(dev, 1);

			return;
		}

		me_client->props = props_res->client_properties;
		dev->me_client_index++;
		dev->me_client_presentation_num++;

		mei_host_client_enumerate(dev);

		break;

	case HOST_ENUM_RES_CMD:
		enum_res = (struct hbm_host_enum_response *) mei_msg;
		memcpy(dev->me_clients_map, enum_res->valid_addresses, 32);
		if (dev->dev_state == MEI_DEV_INIT_CLIENTS &&
		    dev->init_clients_state == MEI_ENUM_CLIENTS_MESSAGE) {
				dev->init_clients_timer = 0;
				dev->me_client_presentation_num = 0;
				dev->me_client_index = 0;
				mei_allocate_me_clients_storage(dev);
				dev->init_clients_state =
					MEI_CLIENT_PROPERTIES_MESSAGE;

				mei_host_client_enumerate(dev);
		} else {
			dev_dbg(&dev->pdev->dev, "reset due to received host enumeration clients response bus message.\n");
			mei_reset(dev, 1);
			return;
		}
		break;

	case HOST_STOP_RES_CMD:
		dev->dev_state = MEI_DEV_DISABLED;
		dev_dbg(&dev->pdev->dev, "resetting because of FW stop response.\n");
		mei_reset(dev, 1);
		break;

	case CLIENT_DISCONNECT_REQ_CMD:
		/* search for client */
		disconnect_req = (struct hbm_client_connect_request *)mei_msg;
		mei_client_disconnect_request(dev, disconnect_req);
		break;

	case ME_STOP_REQ_CMD:

		mei_hbm_stop_req_prepare(dev, &dev->wr_ext_msg.hdr,
					dev->wr_ext_msg.data);
		break;
	default:
		BUG();
		break;

	}
}

