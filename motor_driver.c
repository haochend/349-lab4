/**
 * @file   motor_driver.c
 *
 * @brief  LKM driver for the RPi LEDs
 *
 * @author LASTONE
 */
#include <linux/init.h>   // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h>     // Header for the Linux file system support
#include <asm/uaccess.h>  // Required for the copy to user function
#include <linux/io.h>     // for iore/unmap()
#include <linux/gpio.h>   // required for the gpio functions
#include <linux/interrupt.h>    // Required for the IRQ code

/** @brief The device will appear at /dev/motor_char using this value*/
#define DEVICE_NAME "motor_char"
/** @brief The device class -- this is a character device driver*/
#define CLASS_NAME "motor" 
/** @brief GPIO pin number for red led */
#define LED_RED_PIN 35
/** @brief GPIO pin number for motor 1 */
#define MOTOR1 21
/** @brief GPIO pin number for motor 2 */
#define MOTOR2 20
/** @brief GPIO pin number for down button */
#define BUTTON 26
/** @brief GPIO pin number for encoder1 channel A */
#define ENC1A  17
/** @brief GPIO pin number for encoder1 channel B */
#define ENC1B  23

/** @brief Module info: license */
MODULE_LICENSE("GPL");
/** @brief Module info: author(s) */
MODULE_AUTHOR("LASTONE");
/** @brief Module info: description */
MODULE_DESCRIPTION("A simple Linux motor driver for the RPi motor");
/** @brief Module info: version */
MODULE_VERSION("0.1");


static int     mydriver_open(struct inode *, struct file *);
static int     mydriver_release(struct inode *, struct file *);
static ssize_t mydriver_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel.
*/
static struct file_operations fops =
{
  .open = mydriver_open,
  .write = mydriver_write,
  .release = mydriver_release,
};

// ****************************************************************************
// 18-349 GPIO library ported to LKM
// ****************************************************************************

/** @brief base of MMIO (physical address) on the pi */
#define MMIO_BASE_PHYSICAL 0x3F000000
/** @brief base of GPIO in memory mapped IO on pi */
#define GPIO_BASE (MMIO_BASE_PHYSICAL + 0x200000)
/** @brief GPIO Pin Output Set 0 */
#define GPIO_REG_GPSET0 7
/** @brief GPIO Pin Output Set 1 */
#define GPIO_REG_GPSET1 8
/** @brief GPIO Pin Output Clear 0 */
#define GPIO_REG_GPCLR0 10
/** @brief GPIO Pin Output Clear 1 */
#define GPIO_REG_GPCLR1 11
/** @brief GPIO Function Select OUTPUT */
#define GPIO_FUN_OUTPUT 1

/** @brief major number of the device */
static int majorNumber;
/** @brief iqr number of the device */
static int irqNumber;
/** @brief the device driver class struct pointer */
static struct class* motorcharclass = NULL;
/** @brief the device driver device struct pointer */
static struct device* motorchardevice = NULL;
/** @brief input number */
static char input[8] = {0};

// ****************************************************************************
// Module interface functions
// ****************************************************************************

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mydriver_open(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "motor_driver: device opened once...\n");
  return 0;
}
/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int mydriver_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "motor_driver: device closed...\n");
  return 0;
}
/** @brief This function is called whenever the device is being written to from user 
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t mydriver_write(struct file *filep, const char *buffer,size_t len, loff_t *offset){
  sprintf(input, buffer);
  printk(KERN_INFO "character received: %s, len of %zu\n", buffer, len);
  switch (input[0]){
    case '0':
      gpio_set_value(MOTOR1, false);
      gpio_set_value(MOTOR2, false);
      break;
    case '1':
      gpio_set_value(MOTOR1, true);
      gpio_set_value(MOTOR2, false);
      break;
    case '2':
      gpio_set_value(MOTOR1, false);
      gpio_set_value(MOTOR2, true);
      break;
  }
  return len;
}

/** @brief the irq handler 
 *  @param irq the irq number with the gpio pin
 *  @param dev_id the device id
 *  @param regs specific register values
 *  @return the handling status
 */
static irq_handler_t my_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
   int cur1,cur2;
   cur1 = gpio_get_value(MOTOR1);
   cur2 = gpio_get_value(MOTOR2);
   gpio_set_value(MOTOR1, !cur1);          // Set the physical LED accordingly
   gpio_set_value(MOTOR2, !cur2);
   return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
}

/** @brief Called when the module is loaded with insmod
 *  @return 0 on failure or a non-zero value on success
 */
static int __init motor_driver_init(void) {
  int result = 0;
  
  majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
  printk(KERN_INFO "motor_driver: device number is %d.....\n", majorNumber);
  motorcharclass = class_create(THIS_MODULE, CLASS_NAME);
  printk(KERN_INFO "motor_driver: device class registered!\n");
  motorchardevice = device_create(motorcharclass, NULL, MKDEV(majorNumber, 0), 
			NULL, DEVICE_NAME);
  printk(KERN_INFO "motor_driver: device class created!\n");
  
  gpio_request(MOTOR1, "gpioMotor1"); // gpio for Motor A requested
  gpio_direction_output(MOTOR1, false); // Set the gpio to be in output mode and on
  gpio_export(MOTOR1, false); // Causes gpio20 to appear in /sys/class/gpio

  gpio_request(MOTOR2, "gpioMotor2"); // gpio for Motor A requested
  gpio_direction_output(MOTOR2, false); // Set the gpio to be in output mode and on
  gpio_export(MOTOR2, false); // Causes gpio20 to appear in /sys/class/gpio

  gpio_request(BUTTON, "DOWNBUTTON");
  gpio_direction_input(BUTTON);
  gpio_set_debounce(BUTTON, 200);      // Debounce the button with a delay of 200ms
  gpio_export(BUTTON, false);          // Causes gpio115 to appear in /sys/class/gpio
  
  irqNumber = gpio_to_irq(BUTTON);
  printk(KERN_INFO "GPIO: The button is mapped to IRQ: %d\n", irqNumber);
  // This next call requests an interrupt line
  result = request_irq(irqNumber,        // The interrupt number requested
             (irq_handler_t) my_irq_handler, // The pointer to the handler function below
              IRQF_TRIGGER_RISING,   // Interrupt on rising edge (button press, not release)
              "my_irq_handler",     // Used in /proc/interrupts to identify the owner
              NULL);                 // The *dev_id for shared interrupt lines, NULL is okay

  // Made it! device was initialized
  printk(KERN_INFO "motor_driver: hello world!\n");
  return result;
}


/** @brief Called when the module is unloaded with rmmod */
static void __exit motor_driver_exit(void) {
  gpio_unexport(MOTOR1);                  // Unexport the LED GPIO
  gpio_unexport(MOTOR2);
  free_irq(irqNumber, NULL);               // Free the IRQ number, no *dev_id required in this case
  gpio_unexport(BUTTON);               // Unexport the Button GPIO
  gpio_free(MOTOR1);                      // Free the LED GPIO
  gpio_free(MOTOR2);                   // Free the Button GPIO
  gpio_free(BUTTON);
  device_destroy(motorcharclass, MKDEV(majorNumber, 0));
  class_unregister(motorcharclass);
  class_destroy(motorcharclass);
  unregister_chrdev(majorNumber, DEVICE_NAME);

  printk(KERN_INFO "motor_driver: Goodbye from the LKM!\n");
}


/** @brief Registering the module init function with the kernel
 *
 *  LED_driver_init: The function that holds the module's init routine
 */
module_init(motor_driver_init);


/** @brief Registering the module exit function with the kernel
 *
 *  LED_driver_exit: The function that holds the module's exit routine
 */
module_exit(motor_driver_exit);
