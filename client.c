/**
 * @file   client.c
 *
 * @brief  the user program that runs the client rpi
 *
 * @author David Dong haochend@andrew.cmu.edu
 *         Yanying Zhu yanyingz@andrew.cmu.edu
 */

#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>

/** @brief port number for network */
#define PORT 5000
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
/** @brief define the buffer size */
#define BUFFSIZE 32

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
/** @brief global varible for received target position */
static int target_pos = 0;
/** @brief global varible for sending motor position */
static int motor_pos = 0;
/** @brief ip address */
static const char *host = "127.0.0.1";

/** @brief prototype for device write function 
    @param fd is the file descripture of the target device
    @param num is the number of bits write
*/
void writeToDevice(int fd, int num) {
	memset(writeString, 0, writeLen);
	sprintf(writeString, "%d", num);
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

/** @brief the client function that is being run on one thread
           receive and send motor position over network
*/
void *clientFun() {
	int sockfd, i;
	char sendBuffer[BUFFSIZE] = {0}, receiveBuffer[BUFFSIZE] = {0};

	struct sockaddr_in client_addr, server_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, host, &server_addr.sin_addr);
	server_addr.sin_port = htons(PORT);

	connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	while(1) {
		memset(sendBuffer, 0, BUFFSIZE);
		memset(receiveBuffer, 0 , BUFFSIZE);
		sprintf(sendBuffer, "%d", motor_pos);
		send(sockfd, sendBuffer, strlen(sendBuffer)+1, 0);
		read(sockfd, receiveBuffer, BUFFSIZE);
		target_pos = atoi(receiveBuffer);

		for (i = 0; i < FREQ*2; i++);
	}
}


/** @brief the motor function that is simply the same as PID control
           on one thread
*/
void *motorFun() {
	int fd_motor, fd_pwm, fd_wheel_encoder, i, err, speed;
	int err_sum = 0, last_err = 0, dir = 0;

	fd_motor = open("/dev/motor_char", O_RDWR);
	fd_pwm = open("/dev/motor_pwm", O_RDWR);
	fd_wheel_encoder = open("/dev/wheel_encoder", O_RDWR);

	while (1) {
		motor_pos = readEncoder(fd_wheel_encoder);
		
		err = target_pos-motor_pos;

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

/** @brief creates two threads and run the client function
           and motor function concurrently
*/
int main() {
	pthread_t tid1, tid2;

	pthread_create(&tid1, NULL, clientFun, NULL);
	pthread_create(&tid2, NULL, motorFun, NULL);

	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);


	return 0;
}
