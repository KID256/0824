#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/delay.h>  // Include delay library

#define DEVICE_NAME "motion_sensor"
#define SENSOR_GPIO 17
#define LED_GPIO 18
#define LED_ON_TIME_MS 700  // LED on time in milliseconds

static int major_number;
static struct class *motion_sensor_class;
static struct device *motion_sensor_device;
static int motion_detected;
static int user_space_running;  // Track if user-space program is running
static DECLARE_WAIT_QUEUE_HEAD(motion_waitqueue);

static irqreturn_t motion_interrupt(int irq, void *dev_id) {
    if (user_space_running) {  // Only notify user-space program if it is running
        motion_detected = 1;
        wake_up_interruptible(&motion_waitqueue);
        gpio_set_value(LED_GPIO, 1);  // Turn on LED
        mdelay(LED_ON_TIME_MS);  // Wait for 0.7 seconds
        gpio_set_value(LED_GPIO, 0);  // Turn off LED
        pr_info("Motion detected!\n");
    }
    return IRQ_HANDLED;
}

static unsigned int device_poll(struct file *file, poll_table *wait) {
    poll_wait(file, &motion_waitqueue, wait);
    return motion_detected ? (POLLIN | POLLRDNORM) : 0;
}

static int device_open(struct inode *inode, struct file *file) {
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    char message[2] = {motion_detected ? '1' : '0', '\n'};
    motion_detected = 0;  // Reset state

    if (len < 2)
        return -EINVAL;

    if (copy_to_user(buffer, message, 2))
        return -EFAULT;

    return 2;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    if (len != sizeof(int)) {
        return -EINVAL;  // Invalid length
    }

    if (copy_from_user(&user_space_running, buffer, sizeof(user_space_running))) {
        return -EFAULT;  // Failed to copy data from user
    }

    if (user_space_running) {
        gpio_set_value(LED_GPIO, 0);  // Ensure LED is off when user space starts
    } else {
        gpio_set_value(LED_GPIO, 0);  // Turn off LED when user space stops
    }

    return sizeof(user_space_running);
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
    .poll = device_poll,
};

static int __init motion_sensor_init(void) {
    int ret;

    // Register character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("Failed to register character device\n");
        return major_number;
    }

    // Create device class
    motion_sensor_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(motion_sensor_class)) {
        pr_err("Failed to create device class\n");
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(motion_sensor_class);
    }

    // Create device
    motion_sensor_device = device_create(motion_sensor_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(motion_sensor_device)) {
        pr_err("Failed to create the device\n");
        class_destroy(motion_sensor_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(motion_sensor_device);
    }

    // Request GPIOs
    gpio_request_one(SENSOR_GPIO, GPIOF_IN, "motion_sensor");
    gpio_request_one(LED_GPIO, GPIOF_OUT_INIT_LOW, "led");  // Initialize LED to low

    // Request IRQ
    ret = request_irq(gpio_to_irq(SENSOR_GPIO), motion_interrupt, IRQF_TRIGGER_RISING, "motion_sensor", NULL);
    if (ret) {
        pr_err("Unable to request IRQ\n");
        device_destroy(motion_sensor_class, MKDEV(major_number, 0));
        class_destroy(motion_sensor_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    pr_info("Motion sensor module initialized\n");
    return 0;
}

static void __exit motion_sensor_exit(void) {
    free_irq(gpio_to_irq(SENSOR_GPIO), NULL);
    gpio_set_value(LED_GPIO, 0);  // Ensure LED is off on exit
    gpio_free(LED_GPIO);
    gpio_free(SENSOR_GPIO);
    device_destroy(motion_sensor_class, MKDEV(major_number, 0));
    class_destroy(motion_sensor_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("Motion sensor module unloaded\n");
}

module_init(motion_sensor_init);
module_exit(motion_sensor_exit);
MODULE_LICENSE("GPL");