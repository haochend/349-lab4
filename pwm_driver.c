/**
 * @file   pwm_driver.c
 *
 * @brief  LKM driver for the RPi motor pwm
 *
 * @author David Dong haochend@andrew.cmu.edu
 */
#include <linux/init.h>   // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h>     // Header for the Linux file system support
#include <asm/uaccess.h>  // Required for the copy to user function
#include <linux/io.h>     // for iore/unmap()
#include <linux/gpio.h> // Required for the GPIO functions
#include <linux/interrupt.h> // Required for the IRQ code
#include <linux/hrtimer.h> // Required for hrtimer
#include <linux/ktime.h>  // Required for ktime

/** @brief the pwm pin number */
#define gpioPWM 12
/** @brief the name of the device */
#define NAME "motor_pwm"
/** @brief Module info: license */
MODULE_LICENSE("GPL");
/** @brief Module info: author(s) */
MODULE_AUTHOR("LASTONE");
/** @brief Module info: description */
MODULE_DESCRIPTION("A simple Linux motor driver for the RPi motor");
/** @brief Module info: version */
MODULE_VERSION("0.1");

/** @brief the hr timer struct */
static struct hrtimer hr_timer;
/** @brief the ktime struct */
static ktime_t ktime;
/** @brief time interval */
unsigned long timer_interval_ns = 1e6;
/** @brief time interval for off */
static unsigned long off;
/** @brief time interval for duty cycle on */
static unsigned long on;
/** @brief device major number */
static int major_number;
/** @brief number of cycles */
static int cycle = 0;
/** @brief on/off indicator of duty cycle */
static bool onOrOff = false;
/** @brief class struct pointer */
static struct class*  this_class  = NULL; 
/** @brief device struct pointer */
static struct device* this_device = NULL;


static int driver_open(struct inode *inodep, struct file *filep);
static int driver_release(struct inode *inodep, struct file *filep);
static ssize_t driver_read(struct file *filep, char *buffer, size_t len, loff_t *offset);
static ssize_t driver_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

/** @brief The file operations structure lists the
 *         callback functions that you can associate with the file operations.
 */
static struct file_operations fops ={
  .open = driver_open,
  .read = driver_read,
  .write = driver_write,
  .release = driver_release,
};

/** @brief this function iscalled when a hr timer reaches 0 
    @param timer is the hrtimer that is currently used 
*/
enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer){
  ktime_t waitTime, now;
  now = ktime_get();

  if (onOrOff) {
    waitTime = ktime_set(0, on);
    gpio_set_value(gpioPWM, onOrOff);
    onOrOff = !onOrOff;
  } else {
    waitTime = ktime_set(0, off);
    gpio_set_value(gpioPWM, onOrOff);
    onOrOff = !onOrOff;
  }
  hrtimer_forward(timer, now, waitTime);
  return HRTIMER_RESTART;
}

/** @brief Called when the module is loaded with insmod
 *  @return 0 on failure or a non-zero value on success
 */
static int __init pwm_init(void) {

  major_number = register_chrdev(0, NAME, &fops);

  this_class = class_create(THIS_MODULE, NAME);

  this_device = device_create(this_class, NULL, MKDEV(major_number, 0), NULL, NAME);

  gpio_request(gpioPWM, "gpioPWM");
  gpio_direction_output(gpioPWM, false);

  ktime = ktime_set(0, timer_interval_ns);
  hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = &my_hrtimer_callback;
  hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);
  printk(KERN_INFO "sucessfully inited! \n");
  return 0;
}

/** @brief Called when the module is unloaded with rmmod */
static void __exit pwm_exit(void) {
  device_destroy(this_class, MKDEV(major_number,0));
  class_unregister(this_class);
  class_destroy(this_class);
  unregister_chrdev(major_number, NAME);
  hrtimer_cancel(&hr_timer);
}
/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int driver_open(struct inode *inodep, struct file *filep) {
  return 0;
}
/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int driver_release(struct inode *inodep, struct file *filep) {
  return 0;
}
/** @brief This function is called whenever device is being read from user space
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t driver_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
  return 0;
}
/** @brief This function parses an interger from the input char
 *  @param s the string
 *  @param len the length of desired number
 */
static int parseInt(const char* s, int len) {
  int i, result;
  result = 0;
  for (i = 0; i < len; i++) {
    result = result*10+(s[i]-'0');
  }
  return result;
}
 
/** @brief This function is called whenever the device is being written to from user space
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t driver_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
  hrtimer_cancel(&hr_timer);

  cycle = parseInt(buffer, len-1);

  printk(KERN_INFO "pwn driver: the duty cycle is: %d", cycle);
  
  off = (100-cycle)*timer_interval_ns/100;
  on = cycle*timer_interval_ns/100;

  onOrOff = false;
  ktime = ktime_set(0, on);
  hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  hr_timer.function = &my_hrtimer_callback;
  hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

  return len;
}

/** @brief Registering the module init function with the kernel
 *
 *  direction_init: The function that holds the module's init routine
 */
module_init(pwm_init);


/** @brief Registering the module exit function with the kernel
 *
 *  direction_exit: The function that holds the module's exit routine
 */
module_exit(pwm_exit);

