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


static int razer_control_probe(struct hid_device *hdev, const struct hid_device_id *id) {
    return 0;
}

static void razer_control_remove(struct hid_device *hdev) {
    hid_hw_stop(hdev);
}


MODULE_DEVICE_TABLE(hid, device_table);
static struct hid_driver razer_driver = {
	.name = "Razer laptop System control driver",
	.probe = razer_control_probe,
	.remove = razer_control_remove,
	.id_table = device_table,
};
module_hid_driver(razer_driver);