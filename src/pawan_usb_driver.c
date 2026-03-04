/*
 * File: pawan_usb_driver.c
 * Author: Pawan Gupta
 * Description:
 *   Custom Linux USB Character Driver
 *   Compatible with Linux Kernel 6.x
 *
 * Features:
 *   - USB probe/disconnect
 *   - Bulk IN/OUT transfer
 *   - Character device interface
 *   - Proper cleanup
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "../include/pawan_usb_driver.h"

/* ============================
   Device Structure
   ============================ */

struct pawan_usb_device {
    struct usb_device *udev;
    struct usb_interface *interface;

    unsigned char bulk_in_endpointAddr;
    unsigned char bulk_out_endpointAddr;

    size_t bulk_in_size;
    unsigned char *bulk_in_buffer;
};

static struct pawan_usb_device *global_dev;

/* Character device variables */
static dev_t dev_number;
static struct cdev pawan_cdev;
static struct class *pawan_class;

/* ============================
   Character File Operations
   ============================ */

static int pawan_open(struct inode *inode, struct file *file)
{
    if (!global_dev)
        return -ENODEV;

    file->private_data = global_dev;
    pr_info("USB Device Opened\n");
    return 0;
}

static int pawan_release(struct inode *inode, struct file *file)
{
    pr_info("USB Device Closed\n");
    return 0;
}

static ssize_t pawan_read(struct file *file,
                          char __user *buffer,
                          size_t count,
                          loff_t *ppos)
{
    struct pawan_usb_device *dev = file->private_data;
    int retval;
    int read_cnt;

    if (!dev)
        return -ENODEV;

    retval = usb_bulk_msg(dev->udev,
                          usb_rcvbulkpipe(dev->udev,
                          dev->bulk_in_endpointAddr),
                          dev->bulk_in_buffer,
                          min(dev->bulk_in_size, count),
                          &read_cnt,
                          5000);

    if (retval)
        return retval;

    if (copy_to_user(buffer, dev->bulk_in_buffer, read_cnt))
        return -EFAULT;

    return read_cnt;
}

static ssize_t pawan_write(struct file *file,
                           const char __user *user_buffer,
                           size_t count,
                           loff_t *ppos)
{
    struct pawan_usb_device *dev = file->private_data;
    int retval;
    int wrote_cnt;
    char *buf;

    if (!dev)
        return -ENODEV;

    buf = kmalloc(count, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, user_buffer, count)) {
        kfree(buf);
        return -EFAULT;
    }

    retval = usb_bulk_msg(dev->udev,
                          usb_sndbulkpipe(dev->udev,
                          dev->bulk_out_endpointAddr),
                          buf,
                          count,
                          &wrote_cnt,
                          5000);

    kfree(buf);

    if (retval)
        return retval;

    return wrote_cnt;
}

static struct file_operations pawan_fops = {
    .owner = THIS_MODULE,
    .open = pawan_open,
    .release = pawan_release,
    .read = pawan_read,
    .write = pawan_write,
};

/* ============================
   USB Probe Function
   ============================ */

static int pawan_probe(struct usb_interface *interface,
                       const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct pawan_usb_device *dev;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->udev = usb_get_dev(udev);
    dev->interface = interface;

    iface_desc = interface->cur_altsetting;

    for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
        endpoint = &iface_desc->endpoint[i].desc;

        if (usb_endpoint_is_bulk_in(endpoint)) {
            dev->bulk_in_size = usb_endpoint_maxp(endpoint);
            dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
            dev->bulk_in_buffer =
                kmalloc(dev->bulk_in_size, GFP_KERNEL);
        }

        if (usb_endpoint_is_bulk_out(endpoint)) {
            dev->bulk_out_endpointAddr =
                endpoint->bEndpointAddress;
        }
    }

    if (!(dev->bulk_in_endpointAddr &&
          dev->bulk_out_endpointAddr)) {
        kfree(dev);
        return -ENODEV;
    }

    global_dev = dev;

    pr_info("Pawan USB Device Connected\n");
    return 0;
}

static void pawan_disconnect(struct usb_interface *interface)
{
    if (global_dev) {
        kfree(global_dev->bulk_in_buffer);
        usb_put_dev(global_dev->udev);
        kfree(global_dev);
        global_dev = NULL;
    }

    pr_info("Pawan USB Device Disconnected\n");
}

/* ============================
   USB Device ID Table
   ============================ */

static const struct usb_device_id pawan_table[] = {
    { USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
    {}
};
MODULE_DEVICE_TABLE(usb, pawan_table);

/* ============================
   USB Driver Structure
   ============================ */

static struct usb_driver pawan_usb_driver = {
    .name = DRIVER_NAME,
    .probe = pawan_probe,
    .disconnect = pawan_disconnect,
    .id_table = pawan_table,
};

/* ============================
   Module Init / Exit
   ============================ */

static int __init pawan_init(void)
{
    int result;

    /* Allocate char device */
    result = alloc_chrdev_region(&dev_number, 0, 1, DRIVER_NAME);
    if (result < 0)
        return result;

    cdev_init(&pawan_cdev, &pawan_fops);
    result = cdev_add(&pawan_cdev, dev_number, 1);
    if (result < 0)
        goto unregister_chrdev;

    /* Linux 6.x class_create API */
    pawan_class = class_create(CLASS_NAME);
    if (IS_ERR(pawan_class)) {
        result = PTR_ERR(pawan_class);
        goto del_cdev;
    }

    device_create(pawan_class, NULL,
                  dev_number, NULL, DEVICE_NAME);

    /* Register USB driver */
    result = usb_register(&pawan_usb_driver);
    if (result)
        goto destroy_device;

    pr_info("Pawan USB Driver Loaded Successfully\n");
    return 0;

destroy_device:
    device_destroy(pawan_class, dev_number);
    class_destroy(pawan_class);
del_cdev:
    cdev_del(&pawan_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev_number, 1);
    return result;
}

static void __exit pawan_exit(void)
{
    usb_deregister(&pawan_usb_driver);

    device_destroy(pawan_class, dev_number);
    class_destroy(pawan_class);
    cdev_del(&pawan_cdev);
    unregister_chrdev_region(dev_number, 1);

    pr_info("Pawan USB Driver Unloaded\n");
}

module_init(pawan_init);
module_exit(pawan_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pawan Gupta");
MODULE_DESCRIPTION("Custom USB Character Driver (Linux 6.x Compatible)");
MODULE_VERSION("1.0");