/**
 * @file   led_driver.c
 *
 * @brief  LKM driver for the RPi LEDs
 *
 * @author ???
 */
#include <linux/init.h>   // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h> // Core header for loading LKMs into the kernel
#include <linux/device.h> // Header to support the kernel Driver Model
#include <linux/kernel.h> // Contains types, macros, functions for the kernel
#include <linux/fs.h>     // Header for the Linux file system support
#include <asm/uaccess.h>  // Required for the copy to user function
#include <linux/io.h>     // for iore/unmap()

/** @brief GPIO pin number for red led */
#define LED_RED_PIN 35

/** @brief Module info: license */
MODULE_LICENSE("GPL");
/** @brief Module info: author(s) */
MODULE_AUTHOR("YOUR TEAM NAMES HERE");
/** @brief Module info: description */
MODULE_DESCRIPTION("A simple Linux char driver for the RPi LEDs");
/** @brief Module info: version */
MODULE_VERSION("0.1");

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

/** @brief GPIO register set */
static volatile unsigned int *gpio = NULL;

/** @brief configures a pin for a given functionality.
 *
 *  See BCM2835 peripherals pg 102 - 103 for various alternate
 *  functions of GPIO pins.
 *
 *  @param pin the pin number to configure (0 to 53 on pi)
 *  @param fun the function type (see defines)
 */
static void gpio_config(unsigned char pin, unsigned char fun) {
  // get the offset into MM GPIO
  unsigned int reg = pin / 10;
  // get contents of correct GPIO_REG_GPFSEL register
  unsigned int config = gpio[reg];
  // get the bit offset into the GPIO_REG_GPFSEL register
  unsigned int offset = (pin % 10) * 3;
  config &= ~(0x7 << offset);
  config |= (fun << offset);
  gpio[reg] = config;
}

/** @brief sets a given GPIO pin high.
 *
 *  @param pin the pin number to set (0 to 53 on pi)
 */
static void gpio_set(unsigned char pin) {
  if (pin > 31) {
    gpio[GPIO_REG_GPSET1] = (1 << (pin - 32));
  } else {
    gpio[GPIO_REG_GPSET0] = (1 << pin);
  }
}

/** @brief sets a given GPIO pin low.
 *
 *  @param pin the pin number to clear (0 to 53 on pi)
 */
static void gpio_clr(unsigned char pin) {
  if (pin > 31) {
    gpio[GPIO_REG_GPCLR1] = (1 << (pin - 32));
  } else {
    gpio[GPIO_REG_GPCLR0] = (1 << pin);
  }
}

// ****************************************************************************
// Module interface functions
// ****************************************************************************

/** @brief Called when the module is loaded with insmod
 *  @return 0 on failure or a non-zero value on success
 */
static int __init LED_driver_init(void) {
  // set up the MMIO for LED
  gpio = ioremap(GPIO_BASE, 20); // 20 bytes since we only need gpio[11] max
  gpio_config(LED_RED_PIN, GPIO_FUN_OUTPUT);

  // Made it! device was initialized
  printk(KERN_INFO "LED_driver: hello world!\n");
  return 0;
}


/** @brief Called when the module is unloaded with rmmod */
static void __exit LED_driver_exit(void) {
  iounmap(gpio); // unmmap GPIO virtual memory
  printk(KERN_INFO "LED_driver: Goodbye from the LKM!\n");
}


/** @brief Registering the module init function with the kernel
 *
 *  LED_driver_init: The function that holds the module's init routine
 */
module_init(LED_driver_init);


/** @brief Registering the module exit function with the kernel
 *
 *  LED_driver_exit: The function that holds the module's exit routine
 */
module_exit(LED_driver_exit);