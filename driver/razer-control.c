// SPDX-License-Identifier: GPL-2.0-only
/*
 * Razer laptop and keyboard driver
 */

#include <linux/hid.h>
#include <linux/usb/input.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

MODULE_AUTHOR("Ashcon Mohseninia <ashconm@outlook.com>");
MODULE_DESCRIPTION("Razer laptop and keyboard driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

// Razer Vendor ID
#define RAZER_VENDOR_ID 0x1532

// Hardware ID's - TODO Keep organized amongst laptop classes
// 15" laptops
#define BLADE_2016_END 0x0224
#define BLADE_2018_ADV 0x0233
#define BLADE_2018_BASE 0x023B
#define BLADE_2018_MERC 0x0240
#define BLADE_2019_BASE 0x0246
#define BLADE_2019_ADV 0x023A
#define BLADE_2019_MERC 0x0245
#define BLADE_2020_BASE 0x0255
#define BLADE_2020_ADV 0x0253


// Stealth`s
#define BLADE_2017_STEALTH_MID 0x022D
#define BLADE_2017_STEALTH_END 0x0232
#define BLADE_2019_STEALTH 0x0239
#define BLADE_2019_STEALTH_GTX 0x024a
#define BLADE_2020_STEALTH 0x252

// Pro's laptops
#define BLADE_PRO_2019 0x234
#define BLADE_2018_PRO_FHD 0x022F
#define BLADE_2017_PRO 0x0225
#define BLADE_2016_PRO 0x0210

#define BLADE_QHD 0x020F

#define RAZER_USB_REPORT_LEN 0x5A
// Report Responses
#define RAZER_CMD_BUSY          0x01
#define RAZER_CMD_SUCCESSFUL    0x02
#define RAZER_CMD_FAILURE       0x03
#define RAZER_CMD_TIMEOUT       0x04
#define RAZER_CMD_NOT_SUPPORTED 0x05

struct razer_control_device {
	struct mutex comm_lock; // Lock during data transmission and receiving
	struct usb_device *usb_dev; // Talk with this
};


union transaction_id_union {
    unsigned char id;
    struct transaction_parts {
        unsigned char device : 3;
        unsigned char id : 5;
    } parts;
};

union command_id_union {
    unsigned char id;
    struct command_id_parts {
        unsigned char direction : 1;
        unsigned char id : 7;
    } parts;
};

struct razer_packet {
    __u8 status;
    union transaction_id_union transaction_id;
    __u16 remaining_packets;
    __u8 protocol_type; // 0x00
    __u8 data_size;
    __u8 command_class;
    union command_id_union command_id;
    __u8 args[80];
    __u8 crc;
    __u8 reserved; // 0x00
};

static const struct hid_device_id device_table[] = {
	// 15"
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2016_END)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2018_ADV)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2018_BASE)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2018_MERC)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2019_BASE)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2019_ADV)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_PRO_2019)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2019_MERC)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2020_BASE)},
    { HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2020_ADV)},

	// Stealths
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2017_STEALTH_MID)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2017_STEALTH_END)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2019_STEALTH)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2019_STEALTH_GTX)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2020_STEALTH)},

	// Pro's
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2018_PRO_FHD)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2017_PRO)},
	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_2017_PRO)},

	{ HID_USB_DEVICE(RAZER_VENDOR_ID, BLADE_QHD)},
	{ }
};


/**
 * Calculate the checksum for the usb message
 *
 * Checksum byte is stored in the 2nd last byte in the messages payload.
 * The checksum is generated by XORing all the bytes in the report starting
 * at byte number 2 (0 based) and ending at byte 88.
 */
__u8 crc(struct razer_packet *buffer) {
	int i;
    __u8 res = 0;
    __u8 *_report = (__u8*) buffer;
	// Simple CRC. Iterate over all bits from 2-87 and XOR them together
	for (i = 2; i < 88; i++) {
		res ^= _report[i];
	}
    return res;
}

/**
 * Print report to syslog
 */
void print_erroneous_report(struct razer_packet* report, char* message) {
    printk(KERN_WARNING "Razer control: %s. Start Marker: %02x id: %02x Num Params: %02x Reserved: %02x Command: %02x Params: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x .\n",
           message,
           report->status,
           report->transaction_id.id,
           report->data_size,
           report->command_class,
           report->command_id.id,
           report->args[0],  report->args[1],  report->args[2],  report->args[3], report->args[4],  report->args[5],
           report->args[6],  report->args[7],  report->args[8],  report->args[9], report->args[10], report->args[11],
           report->args[12], report->args[13], report->args[14], report->args[15]);
}


/**
 * Sends payload to the EC controller
 *
 * @param usb_device EC Controller USB device struct
 * @param buffer Payload buffer
 * @param minWait Minimum time to wait in us after sending the payload
 * @param maxWait Maximum time to wait in us after sending the payload
 */
int send_control_message(struct usb_device *usb_dev, void const *buffer, unsigned long minWait, unsigned long maxWait)
{
    uint request = HID_REQ_SET_REPORT; // 0x09
    uint request_type = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT; // 0x21
    uint value = 0x300;
    uint report_index = 0x02;
    uint size = RAZER_USB_REPORT_LEN;
	char *buf;
	int len;

	/* crc(buffer); // Generate checksum for payload */
	buf = kmemdup(buffer, size, GFP_KERNEL);

    if(buf == NULL)
    {
        return -ENOMEM;
    }

	len = usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0),
			      request,
			      request_type,
			      value,
			      report_index,
			      buf,
			      size,
			      USB_CTRL_SET_TIMEOUT);
	// Sleep for a specified number of us. If we send packets too quickly,
	// the EC will ignore them
	usleep_range(minWait, maxWait);

	kfree(buf);
    if(len != size)
    {
        printk(KERN_WARNING "Razer control: Device data transfer failed.");
    }

	return ((len < 0) ? len : ((len != size) ? -EIO : 0));
}

/**
 * Sends packet in req_buffer and puts device response
 * int resp_buffer.
 *
 * TODO - Work out device calls for all querys (Fan RPM, Matrix brightness and Power mode).
 * Should be possible tracing packets when Synapse first loads up.
 */
int get_usb_responce(struct usb_device *usb_dev, struct razer_packet* req_buffer, struct razer_packet* resp_buffer, unsigned long minWait, unsigned long maxWait)
{
	uint request = HID_REQ_GET_REPORT; // 0x01
	uint request_type = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN; // 0xA1
    uint value = 0x300;
    uint response_index = 0x02;
    uint size = RAZER_USB_REPORT_LEN;
	uint len;
    int result = 0;
	char *buf;

	buf = kzalloc(sizeof(char[90]), GFP_KERNEL);
	if (buf == NULL) {
		return -ENOMEM;
	}

    // Send request to device
	send_control_message(usb_dev, req_buffer, minWait, maxWait);
	len = usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0),
			request,
			request_type,
			value,
			response_index,
			buf,
			size,
			USB_CTRL_SET_TIMEOUT);

	memcpy(resp_buffer, buf, RAZER_USB_REPORT_LEN);
	kfree(buf);

	if (len != 90) {
		dev_warn(&usb_dev->dev, "Razer laptop control: USB Response invalid. Got %d bytes. Expected 90.", len);
        result = 1;
	}

	return result;
}

struct razer_packet send_payload(struct usb_device *usb_dev, struct razer_packet *request_report)
{
    int retval = -1;
    struct razer_packet response_report = {0};

    request_report->crc = crc(request_report);

    retval = get_usb_responce(usb_dev, request_report, &response_report, 600, 800); //min max as parameters ? in openrazer ther are not

    if(retval == 0) {
        // Check the packet number, class and command are the same
        if(response_report.remaining_packets != request_report->remaining_packets ||
           response_report.command_class != request_report->command_class ||
           response_report.command_id.id != request_report->command_id.id) {
            print_erroneous_report(&response_report, "Response doesn't match request");
//		} else if (response_report.status == RAZER_CMD_BUSY) {
//			print_erroneous_report(&response_report, "razerkbd", "Device is busy");
        } else if (response_report.status == RAZER_CMD_FAILURE) {
            print_erroneous_report(&response_report, "Command failed");
        } else if (response_report.status == RAZER_CMD_NOT_SUPPORTED) {
            print_erroneous_report(&response_report, "Command not supported");
        } else if (response_report.status == RAZER_CMD_TIMEOUT) {
            print_erroneous_report(&response_report, "Command timed out");
        }
    } else {
        print_erroneous_report(&response_report, "Invalid Report Length");
    }

    return response_report;
}

/**
 * Get initialised razer report
 */
struct razer_packet get_razer_report(unsigned char command_class, unsigned char command_id, unsigned char data_size)
{
    struct razer_packet new_report = {0};
    memset(&new_report, 0, sizeof(struct razer_packet));

    new_report.status = 0x00;
    new_report.transaction_id.id = 0x1F;
    new_report.remaining_packets = 0x00;
    new_report.protocol_type = 0x00;
    new_report.command_class = command_class;
    new_report.command_id.id = command_id;
    new_report.data_size = data_size;

    return new_report;
}

// SYSFS FUNCTIONS

static ssize_t test_read_show(struct device *dev, struct device_attribute *attr, char *buf) {
	struct razer_control_device *device = dev_get_drvdata(dev);
	struct razer_packet req = {0};
    struct razer_packet resp = {0};
	// 
	req = get_razer_report(0x03, 0x83, 0x03);
	req.args[0] = 0x01;
    req.args[1] = 0x05;
    req.args[2] = 0x00;
	resp = send_payload(device->usb_dev, &req);
	return sprintf(buf, "%02X %02X %02X %02X %02X %02X\n", 
		resp.args[0],
		resp.args[1],
		resp.args[2],
		resp.args[3],
		resp.args[4],
		resp.args[5]
	);
}



static DEVICE_ATTR_RO(test_read);


// LOAD AND UNLOAD FUNCTIONS


static int razer_control_probe(struct hid_device *hdev, const struct hid_device_id *id) {
	struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct razer_control_device *dev = NULL;



	dev = kzalloc(sizeof(struct razer_control_device), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(&intf->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}
	hid_set_drvdata(hdev, dev);
	dev_set_drvdata(&hdev->dev, dev);

	mutex_init(&dev->comm_lock);
	dev->usb_dev = usb_dev;

	device_create_file(&hdev->dev, &dev_attr_test_read);


	if (hid_parse(hdev)) {
		hid_err(hdev, "HID Parse failed\n");
		kfree(dev);
		return 0;
	}

	if (hid_hw_start(hdev, HID_CONNECT_DEFAULT)) {
		hid_err(hdev, "HID HW Start failed");
		kfree(dev);
		return 0;
	}
    return 0;
}

static void razer_control_remove(struct hid_device *hdev) {
	struct razer_control_device *dev;
    struct usb_interface *intf = to_usb_interface(hdev->dev.parent);
    struct usb_device *usb_dev = interface_to_usbdev(intf);

	device_remove_file(&hdev->dev, &dev_attr_test_read);

    dev = hid_get_drvdata(hdev);
    hid_hw_stop(hdev);
	kfree(dev);
	dev_info(&intf->dev, "Razer-control device disconnected\n");
}


MODULE_DEVICE_TABLE(hid, device_table);
static struct hid_driver razer_driver = {
	.name = "Razer laptop System control driver",
	.probe = razer_control_probe,
	.remove = razer_control_remove,
	.id_table = device_table,
};
module_hid_driver(razer_driver);