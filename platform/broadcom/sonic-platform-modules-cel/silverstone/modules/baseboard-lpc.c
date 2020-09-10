/*
 * baseboard-lpc.c - The CPLD driver for the Base Board of Silverstone
 * The driver implement sysfs to access CPLD register on the baseboard of Silverstone via LPC bus.
 * Copyright (C) 2018 Celestica Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/dmi.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <uapi/linux/stat.h>
#include <linux/string.h>

#define DRIVER_NAME "baseboard-lpc"
/**
 * CPLD register address for read and write.
 */
#define VERSION_ADDR        0xA100
#define SCRATCH_ADDR        0xA101
#define BLT_MONTH_ADDR      0xA102
#define BLT_DATE_ADDR       0xA103
#define REBOOT_CAUSE        0xA106
#define SYS_LED_ADDR        0xA162
#define CPLD_REGISTER_SIZE  0x93

/* System reboot cause recorded in CPLD */
static const struct {
        const char *reason;
        u8 reset_code;
} reboot_causes[] = {
        {"POR",           0x11},
        {"soft-warm-rst", 0x22},
        {"soft-cold-rst", 0x33},
        {"warm-rst",      0x44},
        {"cold-rst",      0x55},
        {"wdt-rst",       0x66},
        {"power-cycle",   0x77}
};

struct cpld_b_data {
    struct mutex       cpld_lock;
    uint16_t           read_addr;
};

struct cpld_b_data *cpld_data;
 
static ssize_t scratch_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
    unsigned char data = 0;
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SCRATCH_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return sprintf(buf,"0x%2.2x\n", data);
}

static ssize_t scratch_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    unsigned long data;
    char *last;

    mutex_lock(&cpld_data->cpld_lock);
    data = (uint16_t)strtoul(buf,&last,16);
    if(data == 0 && buf == last){
        mutex_unlock(&cpld_data->cpld_lock);
        return -EINVAL;
    }
    outb(data, SCRATCH_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_RW(scratch);


/* CPLD version attributes */
static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8 version;
    mutex_lock(&cpld_data->cpld_lock);
    version = inb(VERSION_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return sprintf(buf, "%d.%d\n", version >> 4, version & 0x0F);
}
static DEVICE_ATTR_RO(version);

/* CPLD version attributes */
static ssize_t build_date_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8 month, day_of_month;
    mutex_lock(&cpld_data->cpld_lock);
    day_of_month = inb(BLT_DATE_ADDR);
    month = inb(BLT_MONTH_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return sprintf(buf, "%x/%x\n", day_of_month, month);
}
static DEVICE_ATTR_RO(build_date);


static ssize_t getreg_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    uint16_t addr;
    char *last;

    addr = (uint16_t)strtoul(buf,&last,16);
    if(addr == 0 && buf == last){
        return -EINVAL;
    }
    cpld_data->read_addr = addr;
    return count;
}

static ssize_t getreg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int len = 0;

    mutex_lock(&cpld_data->cpld_lock);
    len = sprintf(buf, "0x%2.2x\n",inb(cpld_data->read_addr));
    mutex_unlock(&cpld_data->cpld_lock);
    return len;
}
static DEVICE_ATTR_RW(getreg);

static ssize_t setreg_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    uint16_t addr;
    uint8_t value;
    char *tok;
    char clone[count];
    char *pclone = clone;
    char *last;

    strcpy(clone, buf);

    mutex_lock(&cpld_data->cpld_lock);
    tok = strsep((char**)&pclone, " ");
    if(tok == NULL){
        mutex_unlock(&cpld_data->cpld_lock);
        return -EINVAL;
    }
    addr = (uint16_t)strtoul(tok,&last,16);
    if(addr == 0 && tok == last){
        mutex_unlock(&cpld_data->cpld_lock);
        return -EINVAL;
    }

    tok = strsep((char**)&pclone, " ");
    if(tok == NULL){
        mutex_unlock(&cpld_data->cpld_lock);
        return -EINVAL;
    }
    value = (uint8_t)strtoul(tok,&last,16);
    if(value == 0 && tok == last){
        mutex_unlock(&cpld_data->cpld_lock);
        return -EINVAL;
    }

    outb(value,addr);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_WO(setreg);

/**
 * Read all CPLD register in binary mode.
 */
static ssize_t dump_read(struct file *filp, struct kobject *kobj,
                struct bin_attribute *attr, char *buf,
                loff_t off, size_t count)
{
    unsigned long i=0;
    ssize_t status;

    mutex_lock(&cpld_data->cpld_lock);
begin:
    if(i < count){
        buf[i++] = inb(VERSION_ADDR + off);
        off++;
        msleep(1);
        goto begin;
    }
    status = count;

    mutex_unlock(&cpld_data->cpld_lock);
    return status;
}
static BIN_ATTR_RO(dump, CPLD_REGISTER_SIZE);

/**
 * Show system led status - on/off/1hz/4hz
 * @return         Hex string read from scratch register.
 */
static ssize_t sys_led_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
    unsigned char data = 0;
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SYS_LED_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    data = data & 0x3;
    return sprintf(buf, "%s\n",
            data == 0x03 ? "off" : data == 0x02 ? "4hz" : data ==0x01 ? "1hz": "on");
}

/**
 * Set the status of system led - on/off/1hz/4hz
 */
static ssize_t sys_led_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    unsigned char led_status,data;
    if(sysfs_streq(buf, "off")){
        led_status = 0x03;
    }else if(sysfs_streq(buf, "4hz")){
        led_status = 0x02;
    }else if(sysfs_streq(buf, "1hz")){
        led_status = 0x01;
    }else if(sysfs_streq(buf, "on")){
        led_status = 0x00;
    }else{
        count = -EINVAL;
        return count;
    }
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SYS_LED_ADDR);
    data = data & ~(0x3);
    data = data | led_status;
    outb(data, SYS_LED_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_RW(sys_led);

/**
 * Show system led color - both/green/yellow/none
 * @return         Current led color.
 */
static ssize_t sys_led_color_show(struct device *dev, struct device_attribute *devattr,
                char *buf)
{
    unsigned char data = 0;
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SYS_LED_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    data = (data >> 4) & 0x3;
    return sprintf(buf, "%s\n",
            data == 0x03 ? "off" : data == 0x02 ? "yellow" : data ==0x01 ? "green": "both");
}

/**
 * Set the color of system led - both/green/yellow/none
 */
static ssize_t sys_led_color_store(struct device *dev, struct device_attribute *devattr,
                const char *buf, size_t count)
{
    unsigned char led_status,data;
    if(sysfs_streq(buf, "off")){
        led_status = 0x03;
    }else if(sysfs_streq(buf, "yellow")){
        led_status = 0x02;
    }else if(sysfs_streq(buf, "green")){
        led_status = 0x01;
    }else if(sysfs_streq(buf, "both")){
        led_status = 0x00;
    }else{
        count = -EINVAL;
        return count;
    }
    mutex_lock(&cpld_data->cpld_lock);
    data = inb(SYS_LED_ADDR);
    data = data & ~( 0x3 << 4);
    data = data | (led_status << 4);
    outb(data, SYS_LED_ADDR);
    mutex_unlock(&cpld_data->cpld_lock);
    return count;
}
static DEVICE_ATTR_RW(sys_led_color);

static ssize_t reboot_cause_show(struct device *dev, 
                                 struct device_attribute *attr, 
                                 char *buf)
{
        ssize_t status;
        u8 reg;
        int i;

        mutex_lock(&cpld_data->cpld_lock);
        reg = inb(REBOOT_CAUSE);
        mutex_unlock(&cpld_data->cpld_lock);

        status = 0;
        dev_dbg(dev,"reboot: 0x%x\n", (u8)reg);
        for(i = 0; i < ARRAY_SIZE(reboot_causes); i++){
                if((u8)reg == reboot_causes[i].reset_code){
                        status = sprintf(buf, "%s\n", 
                                         reboot_causes[i].reason);
                        break;
                }
        }
        return status;
}
DEVICE_ATTR_RO(reboot_cause);

static struct attribute *cpld_b_attrs[] = {
    &dev_attr_version.attr,
    &dev_attr_build_date.attr,
    &dev_attr_scratch.attr,
    &dev_attr_getreg.attr,
    &dev_attr_setreg.attr,
    &dev_attr_sys_led.attr,
    &dev_attr_sys_led_color.attr,
    &dev_attr_reboot_cause.attr,
    NULL,
};

static struct bin_attribute *cpld_b_bin_attrs[] = {
    &bin_attr_dump,
    NULL,
};

static struct attribute_group cpld_b_attrs_grp = {
    .attrs = cpld_b_attrs,
    .bin_attrs = cpld_b_bin_attrs,
};

static struct resource cpld_b_resources[] = {
    {
        .start  = 0xA100,
        .end    = 0xA192,
        .flags  = IORESOURCE_IO,
    },
};

static void cpld_b_dev_release( struct device * dev)
{
    return;
}

static struct platform_device cpld_b_dev = {
    .name           = DRIVER_NAME,
    .id             = -1,
    .num_resources  = ARRAY_SIZE(cpld_b_resources),
    .resource       = cpld_b_resources,
    .dev = {
        .release = cpld_b_dev_release,
    }
};

static int cpld_b_drv_probe(struct platform_device *pdev)
{
    struct resource *res;
    int err = 0;

    cpld_data = devm_kzalloc(&pdev->dev, sizeof(struct cpld_b_data),
        GFP_KERNEL);
    if (!cpld_data)
        return -ENOMEM;

    mutex_init(&cpld_data->cpld_lock);

    cpld_data->read_addr = VERSION_ADDR;

    res = platform_get_resource(pdev, IORESOURCE_IO, 0);
    if (unlikely(!res)) {
        printk(KERN_ERR "Specified Resource Not Available...\n");
        return -ENODEV;
    }

    err = sysfs_create_group(&pdev->dev.kobj, &cpld_b_attrs_grp);
    if (err) {
        printk(KERN_ERR "Cannot create sysfs for baseboard CPLD\n");
        return err;
    }
    return 0;
}

static int cpld_b_drv_remove(struct platform_device *pdev)
{
    sysfs_remove_group(&pdev->dev.kobj, &cpld_b_attrs_grp);
    return 0;
}

static struct platform_driver cpld_b_drv = {
    .probe  = cpld_b_drv_probe,
    .remove = __exit_p(cpld_b_drv_remove),
    .driver = {
        .name   = DRIVER_NAME,
    },
};

int cpld_b_init(void)
{
    // Register platform device and platform driver
    platform_device_register(&cpld_b_dev);
    platform_driver_register(&cpld_b_drv);
    return 0;
}

void cpld_b_exit(void)
{
    // Unregister platform device and platform driver
    platform_driver_unregister(&cpld_b_drv);
    platform_device_unregister(&cpld_b_dev);
}

module_init(cpld_b_init);
module_exit(cpld_b_exit);


MODULE_AUTHOR("Celestica Inc.");
MODULE_DESCRIPTION("Celestica Silverstone CPLD baseboard driver");
MODULE_VERSION("0.2.0");
MODULE_LICENSE("GPL");