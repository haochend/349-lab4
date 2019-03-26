/**
 * @file   PID_control.c
 *
 * @brief  the user program that controls the motor using PID
 *
 * @author David Dong haochend@andrew.cmu.edu
 *         Yanying Zhu yanyingz@andrew.cmu.edu
 */

#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

/** @brief define clockwise direction */
#define CLOCKWISE 1
/** @brief define counterclockwise direction */
#define COUNTERCLOCK 2
/** @brief define speed max */
#define SPEED 50
/** @brief define write buffer length */
#define writeLen 4
/** @brief define read buffer length */
#define readLen 64
/** @brief define the time interval */
#define FREQ    5000000
/** @brief define the mox speed */
#define SHIGH   70
/** @brief define the low speed upper bound */
#define SLOW1   20
/** @brief define the low speed lower bound */
#define SLOW2   5
/** @brief define the full round degree */
#define FULLROUND 360
/** @brief define the half round degree */
#define HALFROUND 180

/** @brief write buffer */
static char writeString[writeLen];
/** @brief read buffer */
static char readString[readLen] = {0};
/** @brief define Kp */
static float Kp = 0.23;
/** @brief define Kd */
static float Kd = 0.1;
/** @brief define ki */
static float Ki = 0.0001;

/** @brief prototype for device write function 
    @param fd is the file descripture of the target device
    @param num is the number of bits write
*/
void writeToDevice(int fd, int num);

/** @brief main function runs in a loop, continuously checking encoder outputs and set 
     motor positions according to that 
*/
int main() {
	int fd_motor, fd_pwm, fd_wheel_encoder, fd_rotary_encoder, rotary_pos, motor_pos, err, speed, i;
	int err_sum = 0, last_err = 0, dir = 0;

	fd_motor = open("/dev/motor_char", O_RDWR);
	fd_pwm = open("/dev/motor_pwm", O_RDWR);
	fd_wheel_encoder = open("/dev/wheel_encoder", O_RDWR);
	fd_rotary_encoder = open("/dev/rot_encoder", O_RDWR);

	while(1) {
		rotary_pos = readEncoder(fd_rotary_encoder);
		motor_pos = readEncoder(fd_wheel_encoder);

		printf("rotary_pos: %d\n", rotary_pos);
		printf("motor_pos: %d\n", motor_pos);


		err = rotary_pos-motor_pos;

		printf("Difference before: %d\n", err);

		if (err < 0 && err >= -HALFROUND) {
			dir = COUNTERCLOCK;
		} else if (err < -HALFROUND) {
			dir = CLOCKWISE;
			err = -FULLROUND-err;
		} else if (err >= 0 && err < HALFROUND) {
			dir = CLOCKWISE;
		} else {
			dir = COUNTERCLOCK;
			err = FULLROUND-err;
		}

		printf("Difference after: %d\n", err);

		speed = (int)(Kp*err+Kd*(err-last_err)+Ki*err_sum);

		
		if (speed < 0) speed = -speed;
		if (speed > SHIGH) speed = SHIGH;
                else if (speed < SLOW1 && speed >= SLOW2) speed = SLOW1;
		
		last_err = err;
		err_sum += err;

		writeToDevice(fd_motor, dir);
		writeToDevice(fd_pwm, speed);

		
		for (i = 0; i < FREQ; i++);
	}

}

void writeToDevice(int fd, int num) {
	memset(writeString, 0, writeLen);
	sprintf(writeString, "%d", num);
	printf("writeString: %s\n", writeString);
	write(fd, writeString, strlen(writeString)+1);
}

/** @brief readEncoder reads results from a encoder
    @param fd is the file descriptor of the device
    @return number of bytes read
*/
int readEncoder(int fd) {
	memset(readString, 0, readLen);
	read(fd, readString, readLen);
	return atoi(readString);
}
