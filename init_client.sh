#! /bin/bash
echo "Running init script"
echo "raspberry" | sudo -S su
sleep 0.1
sudo insmod /home/pi/motor_driver.ko
sudo insmod /home/pi/rot_encoder_driver.ko
sudo insmod /home/pi/wheel_encoder_driver.ko
sudo insmod /home/pi/pwm_driver.ko
sudo ./client
echo "client rpi Running"
exit 0