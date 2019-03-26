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

#define CLOCKWISE 1
#define COUNTERCLOCK 2

#define SPEED 50
#define writeLen 4
#define readLen 64

static char writeString[writeLen];
static char readString[readLen] = {0};

void writeToDevice(int fd, int num);


int main() {
	int fd_motor, fd_pwm, fd_wheel_encoder, fd_rotary_encoder, rotary_pos, motor_pos, err, speed;
	int err_sum = 0, last_err = 0, dir = 0;

	//fd_motor = open("/dev/motor_char", O_RDWR);
	//fd_pwm = open("/dev/motor_pwm", O_RDWR);
	//fd_wheel_encoder = open("/dev/wheel_encoder", O_RDWR);
	//fd_rotary_encoder = open("/dev/rot_encoder", O_RDWR);

	printf("Finish opening devices\n");


	while(1) {
		rotary_pos = readEncoder(1);
		motor_pos = readEncoder(2);

		printf("rotary_pos: %d\n", rotary_pos);
		printf("motor_pos: %d\n", motor_pos);


		err = rotary_pos-motor_pos;

		printf("Difference before: %d\n", err);

		if (err < 0 && err >= -180) {
			dir = CLOCKWISE;
		} else if (err < -180) {
			dir = COUNTERCLOCK;
			err = -360-err;
		} else if (err >= 0 && err < 180) {
			dir = COUNTERCLOCK;
		} else {
			dir = CLOCKWISE;
			err = 360-err;
		}

		printf("Difference after: %d\n", err);

        if (err < 10 && err > -10) speed = SPEED;
        else speed = 0;
		// last_err = err;
		// err_sum += err;

		writeToDevice(1, dir);
		writeToDevice(2, 50);
		int i;
                for (i = 0; i < 100000000; i++);
	}

}

void writeToDevice(int i, int num) {
	int fd;
	memset(writeString, 0, writeLen);
	sprintf(writeString, "%d", num);
	printf("writeString: %s\n", writeString);
        if (i ==1) fd = open("/dev/motor_char", O_RDWR);
        else if (i == 2) fd = open("/dev/motor_pwm", O_RDWR);
	write(fd, writeString, strlen(writeString));
}

int readEncoder(int in) {
	int fd;
	memset(readString, 0, readLen);
        if (in==2)fd = open("/dev/wheel_encoder", O_RDWR);
	else if (in==1)fd = open("/dev/rot_encoder", O_RDWR);
	read(fd, readString, readLen);
	return atoi(readString);
}
