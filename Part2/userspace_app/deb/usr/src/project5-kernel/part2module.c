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
#include <linux/ktime.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ólafur Steinn Jónsson");

#define DEVICE_NAME "encoder" // gerir /dev/"DEVICE_NAME"

static int irq_number; // Interrupt ID

static dev_t dev_num; // F. module ID, fyrir kernelinn
static struct cdev encoder_cdev; // linkar dev_num við read()
static struct class *encoder_class = NULL; // segir udev að búa til og eyða /dev/encoder
static struct file_operations fops; // forward declaration svo probe geti verið fyrir ofan read()

// Til að lesa pinna:
static struct gpio_desc *enc_a = NULL;
static struct gpio_desc *enc_b = NULL;

volatile int32_t counter; // bara position nema loopar ekki
const int16_t rev_res = 1050; // Steps í einum hring á mótor
volatile int16_t ext_counter; // position sem loopar
volatile int a_prevstate;
volatile int dir; // true fyrir áfram false fyrir afturábak. Fer samt eftir hvernig maður tengir hvort áttin er hvað
struct History {
    // Þetta er til að reikna hraðann, hægt að averagea yfir seinustu X mörg counter steps
    volatile int8_t counter_mem;
    volatile uint32_t timestamps;
};
// Þurfti að breyta þessum tveim utaf þetta er c ekki c++:
#define history_length 40
struct History enc_memory[history_length];

volatile uint8_t history_head = 0; 

static inline uint32_t time_mus(void) { // ktime wrapper
    return (uint32_t)ktime_to_us(ktime_get());
}

// Þetta er ISR-ið
// irqreturn_t er datatype fyir kernel, til að confirma að interruptið var meðhöndlað
static irqreturn_t encoder_irq_handler(int irq, void *dev_id) {
    // les pinna
    int a = gpiod_get_value(enc_a);
    int b = gpiod_get_value(enc_b);

    if (a == 1) {
        if (a != a_prevstate) {
            if (a != b) {
                counter++;
            } else {
                counter--;
            }
            a_prevstate = a;
            history_head++;
            if (history_head >= history_length) {
                history_head=0;
            }
            enc_memory[history_head].timestamps = time_mus();
            enc_memory[history_head].counter_mem = (int8_t)counter;
            if (ext_counter >= rev_res || -rev_res >= ext_counter) {
                ext_counter = 0;
            }
        }
    } else {
        a_prevstate = a;
    }

    return IRQ_HANDLED;
}

// probe, keyrir fyrst eins og init()
static int encoder_probe(struct platform_device *pdev) {
    counter = 0;
    ext_counter = 0;
    history_head = 0;

    memset(enc_memory, 0, sizeof(enc_memory));
    // initializa gpio pinna
    enc_a = devm_gpiod_get_index(&pdev->dev, "enca", 0, GPIOD_IN);
    enc_b = devm_gpiod_get_index(&pdev->dev, "encb", 0, GPIOD_IN); 

    // sækja IRQ id pinnanna
    irq_number = gpiod_to_irq(enc_a);

    // ISR config, segir
    request_irq(irq_number, encoder_irq_handler,
                IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, // rising & falling
                "encoder_irq", // label fyrir cat /proc/interrupts
                NULL);

    // Láta kernel gefa okkur module ID
    alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    
    // Tengja read() við gefið module ID
    cdev_init(&encoder_cdev, &fops);
    cdev_add(&encoder_cdev, dev_num, 1);
    
    // Segja kernel að búa til /dev/encoder
    encoder_class = class_create("encoder_class");
    device_create(encoder_class, NULL, dev_num, NULL, DEVICE_NAME);
    
    // Printar í kernel log
    printk(KERN_INFO "Encoder Platform Driver Probed Successfully.\n");
    return 0;
}

// read(), skilar history struct með bara head & tail gildunum
static ssize_t encoder_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    uint8_t head = history_head;
    uint8_t tail = head + 1;
    if (tail >= history_length) {
        tail = 0;
    }
    struct History out[2]; // output struct
    out[0] = enc_memory[head];
    out[1] = enc_memory[tail];
    if (copy_to_user(buf, out, sizeof(out)))
        return -EFAULT;
    return sizeof(out);
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = encoder_read,
};

// remove, keyrir þegar module er tekið út (rmmod)
static void encoder_remove(struct platform_device *pdev) {
    if (irq_number > 0)
        free_irq(irq_number, NULL);
    device_destroy(encoder_class, dev_num);
    class_destroy(encoder_class);
    cdev_del(&encoder_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "Encoder Platform Driver Removed.\n");
}

// Device tree match table
static const struct of_device_id encoder_of_match[] = {
    { .compatible = "custom,encoder-module" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, encoder_of_match); // Macro that helps with module autoloading based on Device Tree compatibility

// Map the functions to the platform driver structure
static struct platform_driver encoder_driver = { 
    .probe = encoder_probe,
    .remove = encoder_remove,
    .driver = {
        .name = "encoder_module",
        .of_match_table = encoder_of_match,
    }
};

module_platform_driver(encoder_driver); // This macro handles module init and exit functions
