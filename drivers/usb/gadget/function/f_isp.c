/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/types.h>
#include <linux/file.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#include <linux/usb.h>
#include <linux/usb/ch9.h>

#include <linux/configfs.h>
#include <linux/usb/composite.h>

#include "configfs.h"
#include "f_isp.h"

#define ISP_BULK_BUFFER_SIZE    512
#define ISP_MAX_INST_NAME_LEN   40

/* String IDs */
#define INTERFACE_STRING_INDEX	0
#define INTERFACE_STRING_SERIAL_IDX		1
#define USB_MFI_SUBCLASS_VENDOR_SPEC 0xf0

/* number of tx and rx requests to allocate */
#define TX_REQ_MAX 4
#define RX_REQ_MAX 2

struct isp_event {
	/* size of the event */
	size_t length;
	/* event data to send */
	void *data;
};

struct isp_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	int state;
	/* set to 1 when we connect */
	unsigned int online:1;
	/* Set to 1 when we disconnect.
	 * Not cleared until our file is closed.
	 */
	int disconnected:1;

	/* set to 1 if we have a pending start request */
	int start_requested;

	int completed;
	int cmd_length;
	int ready;

	/* synchronize access to our device file */
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req[RX_REQ_MAX];
	int rx_done;

	/* delayed work for handling isp start */
	struct delayed_work start_work;

};

unsigned char cmd_buf[8];
extern unsigned char data_buf[8];

static struct usb_interface_descriptor isp_interface_desc = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceProtocol = 0,
};

static struct usb_endpoint_descriptor isp_highspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(512),
};

static struct usb_endpoint_descriptor isp_fullspeed_in_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_DIR_IN,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_isp_descs[] = {
	(struct usb_descriptor_header *)&isp_interface_desc,
	(struct usb_descriptor_header *)&isp_fullspeed_in_desc,
	NULL/*(struct usb_descriptor_header *)&isp_fullspeed_out_desc*/,
	NULL,
};

static struct usb_descriptor_header *hs_isp_descs[] = {
	(struct usb_descriptor_header *)&isp_interface_desc,
	(struct usb_descriptor_header *)&isp_highspeed_in_desc,
	NULL/*(struct usb_descriptor_header *)&isp_highspeed_out_desc*/,
	NULL,
};

static struct usb_string isp_string_defs[] = {
	[INTERFACE_STRING_INDEX].s = "isp interface",
	{},			/* end of list */
};

static struct usb_gadget_strings isp_string_table = {
	.language = 0x0409,	/* en-US */
	.strings = isp_string_defs,
};

static struct usb_gadget_strings *isp_strings[] = {
	&isp_string_table,
	NULL,
};

struct isp_instance {
	struct usb_function_instance func_inst;
	const char *name;
	struct isp_dev *dev;
};

/* temporary variable used between isp_open() and isp_gadget_bind() */
static struct isp_dev *isp_device;
static int isp_release(struct inode *ip, struct file *fp);

static inline struct isp_dev *func_to_isp(struct usb_function *f)
{
	return container_of((f), struct isp_dev, function);
}

static struct usb_request *isp_request_new(struct usb_ep *ep, size_t buffer_size)
{
	struct usb_request *req = NULL;

	req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (req == NULL) {
		return req;
	}

	req->buf = NULL;
	/* now allocate buffers for the requests */
#ifdef DMA_MODE
	req->buf = dma_alloc_coherent(NULL, buffer_size, &req->dma,
			GFP_KERNEL | GFP_DMA);
#else
	req->buf = kmalloc(buffer_size, GFP_KERNEL);
#endif
	if (req->buf == NULL) {
		usb_ep_free_request(ep, req);
		req = NULL;
	}

	return req;
}

static void isp_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
#ifdef DMA_MODE
		dma_free_coherent(NULL, ISP_BULK_BUFFER_SIZE, req->buf,
				  req->dma);
#else
		kfree(req->buf);
#endif
		usb_ep_free_request(ep, req);
	}
}

/* add a request to the tail of a list */
static void isp_req_put(struct isp_dev *dev, struct list_head *head,
			struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request
*isp_req_get(struct isp_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void isp_set_disconnected(struct isp_dev *dev)
{
	DBG(dev->cdev, "%s\n", __func__);
	dev->online = 0;
	dev->completed = 0;
	dev->disconnected = 1;
}

static void isp_complete_in_ctrl(struct usb_ep *ep, struct usb_request *req)
{
	struct isp_dev *dev = isp_device;

	if (req->status != 0) {
		isp_set_disconnected(dev);
	}

	wake_up(&dev->write_wq);
}

static void isp_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct isp_dev *dev = isp_device;

	if (req->status != 0) {
		isp_set_disconnected(dev);
	}

	isp_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static int create_isp_bulk_endpoints(struct isp_dev *dev,
				    struct usb_endpoint_descriptor *in_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;
	int ret = 0;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for isp ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for isp ep_in got %s\n", ep->name);
	ep->driver_data = dev;	/* claim the endpoint */
	dev->ep_in = ep;

	/* now allocate requests for our endpoints */
	for (i = 0; i < TX_REQ_MAX; i++) {
		req = isp_request_new(dev->ep_in, ISP_BULK_BUFFER_SIZE);
		if (req == NULL) {
			ret = -ENOMEM;
			break;
		}
		req->complete = isp_complete_in;
		isp_req_put(dev, &dev->tx_idle, req);
	}
	if (ret < 0) {
		pr_err("[ERROR][USB] isp_bind() could not allocate requests\n");
	}
	return ret;
}

static ssize_t isp_read(struct file *fp, char __user *buf,
						size_t count, loff_t *pos)
{
	struct isp_dev *dev = (struct isp_dev*)fp->private_data;
	int r = 0;
	int ret = 0;
	unsigned xfer;
	unsigned long flags;

	if (dev->disconnected) {
		return -ENODEV;
	}

	/* we will block until we're online */
	ret = wait_event_interruptible(dev->read_wq, (dev->online != 0));
	if (ret < 0) {
		r = ret;
		goto done;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (isp_device->completed == 1) {
		DBG(dev->cdev, "%s: copy cmd to user(cmd)\n", __func__);
		isp_device->completed = 0;
		dev->completed = 0;
		xfer = dev->cmd_length;
		r = xfer;

		if (copy_to_user(buf, &cmd_buf, xfer)) {
			r = -EFAULT;
		} else {
			(void)memset(&cmd_buf, 0x0, sizeof(cmd_buf));
		}
	} else if (isp_device->ready == 1) {
		DBG(dev->cdev, "%s: copy cmd to user(data=%d)\n", __func__, dev->cmd_length);
		isp_device->ready = 0;
		xfer = dev->cmd_length;
		r = xfer;
		if (copy_to_user(buf, &data_buf, xfer)) {
			r = -EFAULT;
		} else {
			memset(&data_buf, 0x0, sizeof(data_buf)); // Ignore return value

		}
	}
	spin_unlock_irqrestore(&dev->lock, flags);

done:
	DBG(dev->cdev, "%s ret=%d\n", __func__, r);
	return r;
}

static ssize_t isp_write(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	struct isp_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	unsigned long flags;
	int r = count;
	unsigned xfer;
	int ret;

	DBG(dev->cdev, "%s(0x%ld)\n", __func__, count);

	if (!dev->online || (dev->disconnected != 0)) {
		return -ENODEV;
	}

	if ((count == 8u) || (count == 4u)) {
		spin_lock_irqsave(&dev->lock, flags);
		ret = copy_from_user(dev->cdev->req->buf, buf, (unsigned long)count);
		if(ret < 0) {
			pr_err("%s:failed to copy from user\n", __func__);
			r = -EIO;
		} else {
			dev->cdev->req->length = count;
			dev->cdev->req->zero = 0;
			dev->cdev->req->complete = isp_complete_in_ctrl;
			ret = usb_ep_queue(dev->cdev->gadget->ep0, dev->cdev->req, GFP_ATOMIC);
			if (ret < 0) {
				DBG(dev->cdev, "%s: xfer error %d\n", __func__, ret);
				r = -EIO;
			}
			isp_device->ready=0;
		}
		spin_unlock_irqrestore(&dev->lock, flags);
	}
	else {
		DBG(dev->cdev, "%s : Bulk IN(0x%ld)\n", __func__, count);

		while (count > 0) {
			if (!dev->online) {
				DBG(dev->cdev, "%s dev->error\n", __func__);
				r = -EIO;
				break;
			}

			/* get an idle tx request to use */
			req = 0;
			ret = wait_event_interruptible(dev->write_wq,
					((req = isp_req_get(dev, &dev->tx_idle))
					 || !dev->online));
			if (ret != 0) {
				r = ret;
				break;
			}

			if (count > ISP_BULK_BUFFER_SIZE) {
				xfer = ISP_BULK_BUFFER_SIZE;
			} else {
				xfer = count;
			}
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}
			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_KERNEL);
			if (ret < 0) {
				DBG(dev->cdev, "%s: xfer error %d\n", __func__, ret);
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}

	if (req) {
		isp_req_put(dev, &dev->tx_idle, req);
	}

	DBG(dev->cdev, "%s returning %d\n", __func__, r);

	return r;
}

static int isp_open(struct inode *ip, struct file *fp)
{
	pr_info("[INFO][USB] %s\n", __func__);

	if (atomic_xchg(&isp_device->open_excl, 1)) {
		return -EBUSY;
	}

	if (isp_device->online == 0) {
		atomic_xchg(&isp_device->open_excl, 0);
		return -EIO;
	}

	isp_device->disconnected = 0;
	fp->private_data = isp_device;

	pr_info("[INFO][USB] %s -end\n", __func__);
	return 0;
}

static int isp_release(struct inode *ip, struct file *fp)
{
	pr_info("[INFO][USB] %s\n", __func__);

	atomic_xchg(&isp_device->open_excl, 0);
	isp_device->disconnected = 0;

	return 0;
}

/* file operations for /dev/usb_accessory */
static const struct file_operations isp_fops = {
	.owner = THIS_MODULE,
	.read = isp_read,
	.write = isp_write,
	.open = isp_open,
	.release = isp_release,
};

static struct miscdevice f_isp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "isp",
	.fops = &isp_fops,
};

static void isp_control_complete(struct usb_ep *ep, struct usb_request *req)
{
	DBG(isp_device->cdev, "%s : len=%d\n", __func__, req->length);
	if (req->length == 8u) {
		;
	}
	else if (req->length == 4u) {
		memcpy(&data_buf, req->buf, 4);
		DBG(isp_device->cdev, "%s : out_buf = %02x %02x %02x %02x \n", __func__,
				data_buf[0],data_buf[1],data_buf[2],data_buf[3]);
		isp_device->cmd_length = 4;
		isp_device->completed = 0;
		isp_device->ready = 1;
	} else {
		printk(KERN_DEBUG "%s : Unknown length(%d)\n", __func__, req->length);
	}
}

int isp_ctrlrequest(struct usb_composite_dev *composite_dev,
		    const struct usb_ctrlrequest *ctrl)
{
	int value = -EOPNOTSUPP;

	isp_device->completed = 0;
	isp_device->ready = 0;
	/* respond with data transfer or status phase? */
	if (ctrl->bRequestType == 0xC0u) {
		memcpy(&cmd_buf, ctrl, sizeof(struct usb_ctrlrequest));
		DBG(composite_dev, "%s IN : cmd_buf = %02x %02x %02x %02x %02x %02x %02x %02x\n", __func__,
			cmd_buf[0],cmd_buf[1],cmd_buf[2],cmd_buf[3],
			cmd_buf[4],cmd_buf[5],cmd_buf[6],cmd_buf[7]);
		isp_device->cmd_length = 8;
		composite_dev->req->length = 8;
		isp_device->completed = 1;

		return 0;
	} else if (ctrl->bRequestType == 0x40u) {
		memcpy(&cmd_buf, ctrl, sizeof(struct usb_ctrlrequest));
		DBG(composite_dev, "%s OUT : cmd_buf = %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
			cmd_buf[0],cmd_buf[1],cmd_buf[2],cmd_buf[3],
			cmd_buf[4],cmd_buf[5],cmd_buf[6],cmd_buf[7]);
		isp_device->cmd_length = 8;
		isp_device->completed = 1;
		composite_dev->req->zero = 0;
		composite_dev->req->length = 4;
		composite_dev->req->complete = isp_control_complete;
		value = usb_ep_queue(composite_dev->gadget->ep0, composite_dev->req, GFP_ATOMIC);
		if (value < 0) {
			pr_err("[INFO][USB] %s: send response error\n",
					__func__);
		}
	}

	if (value == -EOPNOTSUPP) {
		DBG(composite_dev,
		    "unknown class-specific control req %02x.%02x v%04x i%04x l%u\n",
		    ctrl->bRequestType, ctrl->bRequest,
		    ctrl->wIndex, ctrl->wValue, ctrl->wLength);
	}

	return value;
}
EXPORT_SYMBOL_GPL(isp_ctrlrequest);

static int
isp_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct isp_dev *dev = func_to_isp(f);
	struct usb_string *us;
	int id;
	int ret;

	dev->cdev = cdev;
	DBG(cdev, "%s dev: %p\n", __func__, dev);

	dev->start_requested = 0;

	/* allocate a string ID for our interface */
	us = usb_gstrings_attach(cdev, isp_strings,
				 ARRAY_SIZE(isp_string_defs));
	if (IS_ERR(us)) {
		return PTR_ERR(us);
	}

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0) {
		return id;
	}
	isp_interface_desc.bInterfaceNumber = id;
	c->iConfiguration = 0;

	/* allocate endpoints */
	ret = create_isp_bulk_endpoints(dev, &isp_fullspeed_in_desc);
	if (ret) {
		return ret;
	}

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		isp_highspeed_in_desc.bEndpointAddress =
		    isp_fullspeed_in_desc.bEndpointAddress;
	}

	DBG(cdev, "ISP %s speed %s: IN/%s, OUT/NULL\n",
	    gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
	    f->name, dev->ep_in->name/*, dev->ep_out->name*/);
	return 0;
}

static void
isp_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct isp_dev *dev = func_to_isp(f);
	struct usb_request *req;

	DBG(dev->cdev, "%s\n", __func__);

	while ((req = isp_req_get(dev, &dev->tx_idle))) {
		isp_request_free(req, dev->ep_in);
	}
}

static void isp_start_work(struct work_struct *data)
{
	char *envp[2] = { "ISP=START", NULL };

	pr_info("[INFO][USB] %s\n", __func__);

	kobject_uevent_env(&f_isp_device.this_device->kobj, KOBJ_CHANGE, envp);
}

static int isp_function_set_alt(struct usb_function *f,
				unsigned int intf, unsigned int alt)
{
	struct isp_dev *dev = func_to_isp(f);
	struct usb_composite_dev *composite_dev = f->config->cdev;
	int ret;

	DBG(composite_dev, "%s: %d alt: %d\n", __func__, intf, alt);

	ret = config_ep_by_speed(composite_dev->gadget, f, dev->ep_in);
	if (ret < 0) {
		return ret;
	}

	ret = usb_ep_enable(dev->ep_in);
	if (ret < 0) {
		return ret;
	}
	dev->online = 1;

	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);
	return 0;
}

static void isp_function_disable(struct usb_function *f)
{
	struct isp_dev *dev = func_to_isp(f);

	DBG(dev->cdev, "%s\n", __func__);

	isp_set_disconnected(dev);
	(void)usb_ep_disable(dev->ep_in);
	/* readers may be blocked waiting for us to go online */
	wake_up(&dev->read_wq);

	VDBG(dev->cdev, "%s disabled\n", dev->function.name);
}

int isp_bind_config(struct usb_configuration *c)
{
	struct isp_dev *dev = isp_device;

	pr_info("[INFO][USB] %s\n", __func__);

	dev->cdev = c->cdev;
	dev->function.name = "isp";
	dev->function.fs_descriptors = fs_isp_descs;
	dev->function.hs_descriptors = hs_isp_descs;
	dev->function.bind = isp_function_bind;
	dev->function.unbind = isp_function_unbind;
	dev->function.set_alt = isp_function_set_alt;
	dev->function.disable = isp_function_disable;

	return usb_add_function(c, &dev->function);
}
EXPORT_SYMBOL_GPL(isp_bind_config);

static int __isp_setup(struct isp_instance *fi_isp)
{
	struct isp_dev *dev;
	int ret;

	pr_info("[INFO][USB] %s\n", __func__);

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	if (fi_isp != NULL) {
		fi_isp->dev = dev;
	}

	spin_lock_init(&dev->lock);
	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);
	atomic_set(&dev->open_excl, 0);
	INIT_LIST_HEAD(&dev->tx_idle);
	INIT_DELAYED_WORK(&dev->start_work, isp_start_work);

	isp_device = dev;

	ret = misc_register(&f_isp_device);
	if (ret) {
		goto err;
	}

	return 0;

err:
	isp_device = NULL;
	misc_deregister(&f_isp_device);

	kfree(dev);
	pr_err(
	       "[ERROR][USB] isp gadget driver failed to initialize\n");
	return ret;
}

int isp_setup(void)
{
	return __isp_setup(NULL);
}
EXPORT_SYMBOL_GPL(isp_setup);

static int isp_setup_configfs(struct isp_instance *fi_isp)
{
	return __isp_setup(fi_isp);
}

void isp_cleanup(void)
{
	struct isp_dev *dev = isp_device;

	if (dev == NULL) {
		return;
	}

	DBG(dev->cdev, "%s\n", __func__);

	misc_deregister(&f_isp_device);
	isp_device = NULL;
	kfree(dev);
}
EXPORT_SYMBOL_GPL(isp_cleanup);

static struct isp_instance *to_isp_instance(struct config_item *item)
{
	return container_of(to_config_group(item), struct isp_instance,
			    func_inst.group);
}

static void isp_attr_release(struct config_item *item)
{
	struct isp_instance *fi_isp = to_isp_instance(item);

	usb_put_function_instance(&fi_isp->func_inst);
}

static struct configfs_item_operations isp_item_ops = {
	.release = isp_attr_release,
};

static struct config_item_type isp_func_type = {
	.ct_item_ops = &isp_item_ops,
	.ct_owner = THIS_MODULE,
};

static struct isp_instance *to_fi_isp(struct usb_function_instance *fi)
{
	return container_of((fi), struct isp_instance, func_inst);
}

static int isp_set_inst_name(struct usb_function_instance *fi, const char *name)
{
	struct isp_instance *fi_isp;
	char *ptr;
	size_t name_len;

	name_len = strlen(name) + 1u;
	if (name_len > ISP_MAX_INST_NAME_LEN) {
		return -ENAMETOOLONG;
	}

	ptr = kstrndup(name, name_len, GFP_KERNEL);
	if (ptr == NULL) {
		return -ENOMEM;
	}

	fi_isp = to_fi_isp(fi);
	fi_isp->name = ptr;

	return 0;
}

static void isp_free_inst(struct usb_function_instance *fi)
{
	struct isp_instance *fi_isp;

	fi_isp = to_fi_isp(fi);
	kfree(fi_isp->name);
	isp_cleanup();
	kfree(fi_isp);
}

static struct usb_function_instance *isp_alloc_inst(void)
{
	struct isp_instance *fi_isp;
	int ret = 0;

	fi_isp = kzalloc(sizeof(*fi_isp), (gfp_t)GFP_KERNEL);
	if (fi_isp == NULL) {
		return ERR_PTR(-ENOMEM);
	}
	fi_isp->func_inst.set_inst_name = isp_set_inst_name;
	fi_isp->func_inst.free_func_inst = isp_free_inst;

	ret = isp_setup_configfs(fi_isp);
	if (ret) {
		kfree(fi_isp);
		pr_err("[ERROR][USB] Error setting iAP2\n");
		return ERR_PTR(ret);
	}

	config_group_init_type_name(&fi_isp->func_inst.group,
				    "", &isp_func_type);

	return &fi_isp->func_inst;
}

static int isp_ctrlreq_configfs(struct usb_function *f,
				const struct usb_ctrlrequest *ctrl)
{
	return isp_ctrlrequest(f->config->cdev, ctrl);
}

static void isp_free(struct usb_function *f)
{
	/*NO-OP: no function specific resource allocation in isp_alloc */
}

struct usb_function *function_alloc_isp(struct usb_function_instance *fi)
{
	struct isp_instance *fi_isp = to_fi_isp(fi);
	struct isp_dev *dev;

	if (fi_isp->dev == NULL) {
		pr_err("[ERROR][USB] Error: Create isp function before linking isp function with a gadget configuration\n");
		pr_err
		    ("[ERROR][USB] \t1: Delete existing isp function if any\n");
		pr_err("[ERROR][USB] \t2: Create isp function\n");
		pr_err("[ERROR][USB] \t3: Create and symlink isp function with a gadget configuration\n");
		return NULL;
	}

	dev = fi_isp->dev;
	dev->function.name = "isp";
	dev->function.strings = isp_strings;
	dev->function.fs_descriptors = fs_isp_descs;
	dev->function.hs_descriptors = hs_isp_descs;
	dev->function.bind = isp_function_bind;
	dev->function.unbind = isp_function_unbind;
	dev->function.set_alt = isp_function_set_alt;
	dev->function.disable = isp_function_disable;
	dev->function.setup = isp_ctrlreq_configfs;
	dev->function.free_func = isp_free;

	fi->f = &dev->function;

	return &dev->function;
}
EXPORT_SYMBOL_GPL(function_alloc_isp);

static struct usb_function *isp_alloc(struct usb_function_instance *fi)
{
	return function_alloc_isp(fi);
}

DECLARE_USB_FUNCTION_INIT(isp, (isp_alloc_inst), isp_alloc);
MODULE_LICENSE("GPL");
