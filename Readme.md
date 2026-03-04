# Pawan USB Character Driver (Linux 6.x)

Author: Pawan Gupta

## Project Description

This project implements a **custom Linux USB Character Device Driver**
compatible with **Linux Kernel 6.x**.

The driver allows communication between **user-space applications**
and a **USB device** using **bulk IN/OUT transfers**.

It exposes the USB device as a **character device** so that user programs
can read and write data using standard Linux system calls.

Example:

echo "hello" > /dev/pawan_usb
cat /dev/pawan_usb


---

# Features

- USB device detection using **probe()**
- USB device removal using **disconnect()**
- Bulk data transfer
- Character device interface
- Dynamic device node creation
- Compatible with Linux Kernel **6.x**
- Proper memory cleanup

---

# Project Structure

pawan-usb-driver/
│
├── src/
│ └── pawan_usb_driver.c
│
├── include/
│ └── pawan_usb_driver.h
│
├── Makefile
│
└── README.md



---

# Requirements

### Hardware
- Any USB device supporting **Bulk transfer**

Example:
- USB flash device
- USB custom embedded device
- USB serial converter

### Software

- Linux Kernel **6.x**
- Ubuntu / Debian
- GCC
- Kernel headers

Install dependencies:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
Build Driver

Compile the kernel module.

make

Output file:

pawan_usb_driver.ko
Load Driver

Insert the module into kernel.

sudo insmod pawan_usb_driver.ko

Check kernel logs:

dmesg | tail

Expected output:

Pawan USB Driver Loaded Successfully
Verify Driver

Check if module loaded.

lsmod | grep pawan

Expected:

pawan_usb_driver
Check Device Node

After loading driver:

ls /dev/pawan_usb

Example:

/dev/pawan_usb
Connect USB Device

Insert your USB device.

Check kernel logs:

dmesg

Expected:

Pawan USB Device Connected
Test Driver
Write Data
echo "hello USB" > /dev/pawan_usb
Read Data
cat /dev/pawan_usb
Using Python Test Program

Example test script:

f = open("/dev/pawan_usb","rb")
data = f.read(64)
print(data)
f.close()
Remove Driver

Unload module.

sudo rmmod pawan_usb_driver

Check logs:

dmesg | tail

Expected:

Pawan USB Driver Unloaded
Debugging Commands

Show USB devices:

lsusb

Show kernel messages:

dmesg

Show device nodes:

ls /dev

Show module info:

modinfo pawan_usb_driver.ko