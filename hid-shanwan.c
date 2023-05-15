// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Force feedback support for Shanwan USB WirelessGamepad
 *
 * Copyright (c) 2022-2023	Huseyin BIYIK	<huseyinbiyik@hotmail.com>
 * Copyright (c) 2023	Ahmad Hasan Mubashshir	<ahmubashshir@gmail.com>
 *
 */

#include <linux/input.h>
#include <linux/slab.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

#define USB_VENDOR_ID_SHANWAN 0x2563
#define USB_PRODUCT_ID_SHANWAN_USB_WIRELESSGAMEPAD 0x0575

#define SHANWAN_PAYLOAD_LEN(x) (sizeof(s32) * x)
#define CONFIG_SHANWAN_FF

static bool swap_motors;
module_param_named(swap, swap_motors, bool, 0);
MODULE_PARM_DESC(swap, "Swap Weak/Strong Feedback motors");

#ifdef CONFIG_SHANWAN_FF
static int shanwan_play_effect(struct input_dev *dev, void *data, struct ff_effect *effect)
{
	struct hid_device *hid    = input_get_drvdata(dev);
	struct hid_report *report = hid_get_drvdata(hid);
	struct hid_field  *field0 = report->field[0];
	s32 payload_template[] = {
		0x02,  // 2 = rumble effect message
		0x08, // reserved value, always 8
		0x00, // rumble value
		0x00, // rumble value
		0xff  // duration 0-254 (255 = nonstop)
	};

	if (effect->type != FF_RUMBLE)
		return 0;

	memcpy_and_pad(field0->value, SHANWAN_PAYLOAD_LEN(8), payload_template, SHANWAN_PAYLOAD_LEN(4), 0x00);

	if (swap_motors) {
		/* weak rumble / strong rumble */
		field0->value[2] = effect->u.rumble.strong_magnitude / 256;
		field0->value[3] = effect->u.rumble.weak_magnitude / 256;
	} else {
		/* strong rumble / weak rumble */
		field0->value[2] = effect->u.rumble.weak_magnitude / 256;
		field0->value[3] = effect->u.rumble.strong_magnitude / 256;
	}

	hid_hw_request(hid, report, HID_REQ_SET_REPORT);

	return 0;
}

static int shanwan_init_ff(struct hid_device *hid)
{
	struct hid_report *report;
	struct hid_input *hidinput;
	struct list_head *report_list = &hid->report_enum[HID_OUTPUT_REPORT].report_list;
	struct input_dev *dev;

	if (list_empty(&hid->inputs)) {
		hid_err(hid, "no inputs found\n");
		return -ENODEV;
	}
	hidinput = list_first_entry(&hid->inputs, struct hid_input, list);
	dev = hidinput->input;

	if (list_empty(report_list)) {
		hid_err(hid, "no output reports found\n");
		return -ENODEV;
	}

	report = list_first_entry(report_list, struct hid_report, list);
	hid_set_drvdata(hid, report);

	set_bit(FF_RUMBLE, dev->ffbit);
	if (input_ff_create_memless(dev, NULL, shanwan_play_effect))
		return -ENODEV;

	return 0;
}
#else
static int shanwan_init_ff(struct hid_device *hid)
{
	return 0;
}
#endif

static int shanwan_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT & ~HID_CONNECT_FF);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	ret = shanwan_init_ff(hdev);
	if (ret)
		hid_warn(hdev, "Failed to enable force feedback support, error: %d\n", ret);

	ret = hid_hw_open(hdev);
	if (ret) {
		dev_err(&hdev->dev, "hw open failed\n");
		hid_hw_stop(hdev);
		return ret;
	}

	hid_hw_close(hdev);
	return ret;
}

static const struct hid_device_id shanwan_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_SHANWAN, USB_PRODUCT_ID_SHANWAN_USB_WIRELESSGAMEPAD) },
	{ }
};
MODULE_DEVICE_TABLE(hid, shanwan_devices);

static struct hid_driver shanwan_driver = {
	.name			= "shanwan",
	.id_table		= shanwan_devices,
	.probe			= shanwan_probe,
};
module_hid_driver(shanwan_driver);

MODULE_AUTHOR("Huseyin BIYIK <huseyinbiyik@hotmail.com>");
MODULE_AUTHOR("Ahmad Hasan Mubashshir <ahmubashshir@gmail.com>");
MODULE_DESCRIPTION("Force feedback support for Shanwan USB WirelessGamepad");
MODULE_LICENSE("GPL");
// vim: ts=8:noet
