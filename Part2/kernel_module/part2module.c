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

#define DEVICE_NAME "encoder" // gerir /dev/"DEVICE_NAME"

static int irq_number; // Interrupt ID

static dev_t dev_num; // F. module ID, fyrir kernelinn
static struct cdev gpio_cdev; // linkar dev_num við þessi read() & write()
static struct class *gpio_class = NULL; // segir udev að búa til og eyða /dev/encoder

// Til að lesa pinna:
static struct gpio_desc *enc_a = NULL;
static struct gpio_desc *enc_b = NULL;

static volatile int32_t counter = 0; // counter init


// Þetta er ISR-ið
// irqreturn_t er datatype fyir kernel, til að confirma að interruptið var meðhöndlað
static irqreturn_t encoder_irq_handler(int irq, void *dev_id) {
    // les pinna
    int a = gpiod_get_value(enc_a);
    int b = gpiod_get_value(enc_b);

    // basic direction logic:
    if (a == b)
        counter++;
    else
        counter--;
    // bscly svarar "success":
    return IRQ_HANDLED;
}

// probe, keyrir fyrst eins og init()
static int gpio_irq_probe(struct platform_device *pdev) {
    // initializa gpio pinna
    enc_a = devm_gpiod_get_index(&pdev->dev, "enca", 0, GPIOD_IN);
    enc_b = devm_gpiod_get_index(&pdev->dev, "encb", 0, GPIOD_IN); 

    // sækja IRQ id pinnanna
    irq_number = gpiod_to_irq(enc_a);

    // ISR config, segir
    request_irq(irq_number, encoder_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, // rising & falling
                "gpio_char_irq" // label fyrir cat /proc/interrupt
                , NULL);

    // Create /dev/gpio_module through the kernel device model.
    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);

    cdev_init(&gpio_cdev, &fops);
    cdev_add(&gpio_cdev, dev_num, 1);

    gpio_class = class_create("gpio_class");
    device_create(gpio_class, NULL, dev_num, NULL, DEVICE_NAME);

    printk(KERN_INFO "encoder Platform Driver Probed Successfully.\n");
    return 0;
}

// 2. FILE OPERATIONS: READ
static size_t gpio_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
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
