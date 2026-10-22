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
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Modern Device-Tree Bound GPIO Interrupt Driver");
MODULE_VERSION("3.0");

#define DEVICE_NAME "gpio_module"

static int irq_number;
static int current_val = 0;

static dev_t dev_num;
static struct cdev gpio_cdev;
static struct class *gpio_class = NULL;

static struct gpio_desc *in_desc = NULL;
static struct gpio_desc *out_desc = NULL;

// 1. INTERRUPT SERVICE ROUTINE (ISR) - Instant Mirroring
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
    char kernel_buf[10];

    if (count > sizeof(kernel_buf) - 1)
        count = sizeof(kernel_buf) - 1;

    if (copy_from_user(kernel_buf, buf, count))
        return -EFAULT;

    kernel_buf[count] = '\0';

    if (out_desc) {
        if (kernel_buf[0] == '1') {
            gpiod_set_value(out_desc, 1);
        } else if (kernel_buf[0] == '0') {
            gpiod_set_value(out_desc, 0);
        }
    }
    return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = gpio_read,
    .write = gpio_write,
};

// 4. PLATFORM DRIVER PROBE (Executes safely when DT node matches)
static int gpio_irq_probe(struct platform_device *pdev) {
    int ret;
    printk(KERN_INFO "GPIO Platform Driver: Probing device...\n");

    // Grab descriptors safely using the device-managed API bound to the Device Tree
    in_desc = devm_gpiod_get(&pdev->dev, "input", GPIOD_IN);
    if (IS_ERR(in_desc)) {
        printk(KERN_ERR "Failed to get input GPIO descriptor\n");
        return PTR_ERR(in_desc);
    }

    out_desc = devm_gpiod_get(&pdev->dev, "output", GPIOD_OUT_LOW);
    if (IS_ERR(out_desc)) {
        printk(KERN_ERR "Failed to get output GPIO descriptor\n");
        return PTR_ERR(out_desc);
    }

    // Map input descriptor to its hardware IRQ line
    irq_number = gpiod_to_irq(in_desc);
    if (irq_number < 0) {
        printk(KERN_ERR "Failed to map descriptor to IRQ (%d)\n", irq_number);
        return irq_number;
    }

    ret = request_irq(irq_number, (irq_handler_t)gpio_irq_handler,
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                      "gpio_char_irq", NULL);
    if (ret) {
        printk(KERN_ERR "Failed to request IRQ line (%d)\n", ret);
        return ret;
    }

    // Setup Character Device Node (/dev/gpio_module)
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) goto free_irq_line;

    cdev_init(&gpio_cdev, &fops);
    ret = cdev_add(&gpio_cdev, dev_num, 1);
    if (ret < 0) goto unregister_region;

    gpio_class = class_create("gpio_class");
    if (IS_ERR(gpio_class)) {
        ret = PTR_ERR(gpio_class);
        goto delete_cdev;
    }
    device_create(gpio_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk(KERN_INFO "GPIO Platform Driver Probed Successfully. IRQ: %d\n", irq_number);
    return 0;

delete_cdev:
    cdev_del(&gpio_cdev);
unregister_region:
    unregister_chrdev_region(dev_num, 1);
free_irq_line:
    free_irq(irq_number, NULL);
    return ret;
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

static const struct of_device_id gpio_irq_of_match[] = {
    { .compatible = "custom,gpio-irq-module" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gpio_irq_of_match);

static struct platform_driver gpio_irq_driver = {
    .probe = gpio_irq_probe,
    .remove = gpio_irq_remove,
    .driver = {
        .name = "gpio_irq_module",
        .of_match_table = gpio_irq_of_match,
    },
};

module_platform_driver(gpio_irq_driver);
