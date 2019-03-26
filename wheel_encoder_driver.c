/**
 * @file   wheel_encoder_driver.c
 *
 * @brief  LKM driver for the RPi wheel enccoder
 *
 * @author David Dong haochend@andrew.cmu.edu
 *         Yanying Zhu yanyingz@andrew.cmu.edu
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

/** @brief define the name of the device */
#define NAME "wheel_encoder"// The device will appear at /dev/motor_char using this value
/** @brief GPIO pin number for red led */
#define LED_RED_PIN 35
/** @brief GPIO pin number for motor 1 */
#define MOTOR1 21
/** @brief GPIO pin number for motor 2 */
#define MOTOR2 20
/** @brief GPIO pin number for down button */
#define BUTTON 26
/** @brief GPIO pin number for encoder0 channel A */
#define ENC0A  6
/** @brief GPIO pin number for encoder0 channel B */
#define ENC0B  7
/** @brief GPIO pin number for encoder1 channel A */
#define ENC1A  17
/** @brief GPIO pin number for encoder1 channel B */
#define ENC1B  23
/** @brief timer interval */
#define TIMER_INTERVAL 1e9
/** @brief wheel steps */
#define WHEEL_COUNTER 1200
/** @brief full round degree num */
#define FULLROUND     360

/** @brief Module info: license */
MODULE_LICENSE("GPL");
/** @brief Module info: author(s) */
MODULE_AUTHOR("LASTONE");
/** @brief Module info: description */
MODULE_DESCRIPTION("A simple Linux motor driver for the RPi motor");
/** @brief Module info: version */
MODULE_VERSION("0.1");


//Prototypes for chracter driver functions;
static int my_driver_open(struct inode *inodep, struct file *filep);
static int my_driver_release(struct inode *inodep, struct file *filep);
static ssize_t my_driver_read(struct file *filep, char *buffer, size_t len,loff_t *offset);
//static ssize_t my_driver_write(struct file *filep, const char *buffer,size_t len, loff_t *offset);

/// Function prototype for the custom IRQ handler function -- see below for the implementation
static irq_handler_t  enc_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

/** @brief Devices are represented as file structure in the kernel.
*/
static struct file_operations fops =
{
  .open = my_driver_open,
  .read = my_driver_read,
  //.write = my_driver_write,
  .release = my_driver_release,
};

// ****************************************************************************
// 18-349 GPIO library ported to LKM
// ****************************************************************************

/** @brief major number of the device */
static int majorNumber;
/** @brief irq number of enc channel A */
static int irqEncNumberA;
/** @brief irq number of enc channel B */
static int irqEncNumberB;
/** @brief the device driver class struct pointer */
static struct class* class = NULL;
/** @brief the device driver device struct pointer */
static struct device* device = NULL;
/** @brief direction of the wheel */
static int dir;
/** @brief angle count */
static int angle;
/** @brief step counts within the current sec */
static int count;
/** @brief current speed */
static int speed;
/** @brief hr timer */
static struct hrtimer hr_timer;
/** @brief ktime struct */
static ktime_t ktime;
/** @brief buffer for read output */
static char output[64] = {0};

// ****************************************************************************
// Module interface functions
// ****************************************************************************

/** @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int my_driver_open(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "encoder: device opened once...\n");
  return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int my_driver_release(struct inode *inodep, struct file *filep){
  printk(KERN_INFO "encoder: device closed...\n");
  return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t my_driver_read(struct file *filep, char *buffer, size_t len,loff_t *offset){
  int error_count = 0;
  int degree;
  degree = angle * FULLROUND / WHEEL_COUNTER;
  snprintf(output, sizeof(output), "%d", degree);
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
  error_count = copy_to_user(buffer, output, sizeof(output));
  return 0;
}

/** @brief The GPIO IRQ Handler function that happens on a step within the encoder.
 *  @param irq    the IRQ number that is associated with the GPIO -- useful for logging.
 *  @param dev_id the *dev_id that is provided -- can be used to identify which device caused the interrupt
 *  Not used in this example as NULL is passed.
 *  @param regs   h/w specific register values -- only really ever used for debugging.
 *  return returns IRQ_HANDLED if successful -- should return IRQ_NONE otherwise.
 */
static irq_handler_t enc_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){
  int valA, valB;
  if (irq == irqEncNumberA){
    valB = gpio_get_value(ENC0B);
    if (valB) dir = 2;
    else dir = 1;
  }
  else if (irq == irqEncNumberB){
    valA = gpio_get_value(ENC0A);
    if (valA) dir = 1;
    else dir = 2;
  }
  count ++;
  if (dir == 1) angle++;
  else if (dir == 2) angle--;
  angle = angle % WHEEL_COUNTER;
  if (angle < 0) angle += WHEEL_COUNTER;
  return (irq_handler_t) IRQ_HANDLED;
}

/** @brief the hr timer callback function that is called when the timer gets finished.
    @param timer is the hr timer struct that is used
    @return a macro
*/
enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer){
  speed = count;
  count = 0;
  if (count == 0) dir = 0;
  hrtimer_forward_now(timer, ktime_set(0, TIMER_INTERVAL));
  return HRTIMER_RESTART;
}  

/** @brief Called when the module is loaded with insmod
 *  @return 0 on failure or a non-zero value on success
 */
static int __init motor_driver_init(void) {
  int result = 0;
  
  majorNumber = register_chrdev(0, NAME, &fops);
  printk(KERN_INFO "encoder: device number is %d.....\n", majorNumber);
  class = class_create(THIS_MODULE, NAME);
  device = device_create(class, NULL, MKDEV(majorNumber, 0), 
			NULL, NAME);
 
  gpio_request(ENC0A, "Encoder0 A");
  gpio_direction_input(ENC0A);
  gpio_export(ENC0A, false);
  gpio_request(ENC0B, "Encoder0 B");
  gpio_direction_input(ENC0B);
  gpio_export(ENC0B, false);

  irqEncNumberA = gpio_to_irq(ENC0A);
  irqEncNumberB = gpio_to_irq(ENC0B);

  result = request_irq(irqEncNumberA,
			(irq_handler_t) enc_irq_handler,
			IRQF_TRIGGER_RISING,
                        "encoder_irq_handler",
			NULL);
  result = request_irq(irqEncNumberB,
                        (irq_handler_t) enc_irq_handler,
                        IRQF_TRIGGER_RISING,
                        "encoder_irq_handler",
                        NULL);

  ktime = ktime_set(0, TIMER_INTERVAL);
  hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = &my_hrtimer_callback;
  hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
 
  // Made it! device was initialized
  printk(KERN_INFO "motor_driver: hello world!\n");
  return result;
}


/** @brief Called when the module is unloaded with rmmod */
static void __exit motor_driver_exit(void) {
  device_destroy(class, MKDEV(majorNumber, 0));
  class_unregister(class);
  class_destroy(class);
  unregister_chrdev(majorNumber, NAME);

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
