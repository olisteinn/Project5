#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Konráð Björn Ketilsson");

#define DEVICE_NAME "gpio_module"

static int irq_number;
static int current_val = 0;

static dev_t dev_num;
static struct cdev gpio_cdev;
static struct class *gpio_class = NULL;

static struct gpio_desc *in_desc = NULL;
static struct gpio_desc *out_desc = NULL;

// 1. INTERRUPT SERVICE ROUTINE - GPIO coppying
static irqreturn_t gpio_irq_handler(int irq, void *dev_id) {
    if (in_desc && out_desc) {
        current_val = gpiod_get_value(in_desc);
        gpiod_set_value(out_desc, current_val);
    }
    return IRQ_HANDLED;
}

// 2. FILE OPERATIONS: READ
static ssize_t gpio_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    char tmp[4];
    int len;

    if (*offset > 0)
        return 0;

    len = snprintf(tmp, sizeof(tmp), "%d\n", current_val);
    if (copy_to_user(buf, tmp, len))
        return -EFAULT;

    *offset += len;
    return len;
}

// 3. FILE OPERATIONS: WRITE
static ssize_t gpio_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    char value;

    if (!count)
        return 0;

    if (copy_from_user(&value, buf, 1))
        return -EFAULT;

    if (out_desc) {
        if (value == '1')
            gpiod_set_value(out_desc, 1);
        else if (value == '0')
            gpiod_set_value(out_desc, 0);
    }

    return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = gpio_read,
    .write = gpio_write,
};

// 4. PLATFORM DRIVER PROBE (Executes when DT node matches)
static int gpio_irq_probe(struct platform_device *pdev) {
    // Grab descriptors safely using the device-managed API bound to the Device Tree
    in_desc = devm_gpiod_get(&pdev->dev, "input", GPIOD_IN);
    out_desc = devm_gpiod_get(&pdev->dev, "output", GPIOD_OUT_LOW);

    // Map input descriptor to its hardware IRQ line
    irq_number = gpiod_to_irq(in_desc);

    request_irq(irq_number, gpio_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                "gpio_char_irq", NULL);

    // Create /dev/gpio_module through the kernel device model.
    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);

    cdev_init(&gpio_cdev, &fops);
    cdev_add(&gpio_cdev, dev_num, 1);

    gpio_class = class_create("gpio_class");
    device_create(gpio_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk(KERN_INFO "GPIO Platform Driver Probed Successfully.\n");
    return 0;
}

// 5. PLATFORM DRIVER REMOVE
static void gpio_irq_remove(struct platform_device *pdev) {
    if (irq_number > 0)
        free_irq(irq_number, NULL);
    device_destroy(gpio_class, dev_num);
    class_destroy(gpio_class);
    cdev_del(&gpio_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "GPIO Platform Driver Removed.\n");
}

// Device tree match table
static const struct of_device_id gpio_irq_of_match[] = {
    { .compatible = "custom,gpio-irq-module" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gpio_irq_of_match); // Macro that helps with module autoloading based on Device Tree compatibility

// Map the functions to the platform driver structure
static struct platform_driver gpio_irq_driver = { 
    .probe = gpio_irq_probe,
    .remove = gpio_irq_remove,
    .driver = {
        .name = "gpio_irq_module",
        .of_match_table = gpio_irq_of_match,
    }
};

module_platform_driver(gpio_irq_driver); // This macro handles module init and exit functions
