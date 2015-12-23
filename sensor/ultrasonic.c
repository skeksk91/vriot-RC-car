#include <stdio.h>
#include <wiringPi.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "wiringdht.h"

#define trigPin1 8
#define echoPin1 9
#define trigPin2 15
#define echoPin2 16
#define trigPin3 4
#define echoPin3 5

//network
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#define OUTPUT_DIST_LEN 12
#define OUTPUT_DHT_LEN 6

extern pthread_mutex_t dhtmutex;
extern char dhtdate[6];

int getDistance(int num);
void decToStr(int dec, char* str);

//network
int sock;
struct sockaddr_in clnt_addr;
struct sockaddr_in serv_addr;


pthread_t dhttid;

int main(void)
{
	if(wiringPiSetup () == -1)
		return 1;
	pinMode (trigPin1, OUTPUT);
	pinMode (echoPin1, INPUT);
	pinMode (trigPin2, OUTPUT);
	pinMode (echoPin2, INPUT);
	pinMode (trigPin3, OUTPUT);
	pinMode (echoPin3, INPUT);

	//network
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		printf("Error\n");
		return 0;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(9080);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1){
		printf("bind error\n");
		return 0;
	}
	int client_addr_size;
	int leng = 0;
	char buf[1]; // 1,2,3,4 sensor
	char output[12];


	pthread_create(&dhttid, NULL, thread_dht, NULL);

	client_addr_size = sizeof(clnt_addr);

	while(1){
		//printf("recv wait...\n");
		leng = recvfrom(sock, buf, sizeof(buf), 0,
			   	(struct sockaddr*)&clnt_addr, 
				&client_addr_size); 

		//printf("recved \n");
		if(leng > 0){
		//	printf("%c\n", buf[0]); 
			if	(buf[0] == 'D'){
				decToStr(getDistance(0),output);
				decToStr(getDistance(1),output+4);
				decToStr(getDistance(2),output+8);
				if(sendto(sock, output, OUTPUT_DIST_LEN,  0,
							(struct sockaddr *)&clnt_addr, client_addr_size) < 0) {
					perror("D sendto fail");
					exit(0);
				}
			}
			else if	(buf[0] == 'T') { //temperatur and huminity
				pthread_mutex_lock(&dhtmutex);
				if(sendto(sock, dhtdata, OUTPUT_DHT_LEN,  0,
							(struct sockaddr *)&clnt_addr, client_addr_size) < 0) {
					perror("D sendto fail");
					exit(0);
				}
				pthread_mutex_unlock(&dhtmutex);
			}
		}
	}
	pthread_join(dhttid, NULL);
}
int getDistance(int num) {
	int trigPin,echoPin;
	int distance=0;
	int pulse = 0;
	if(num == 0) { 
		trigPin = trigPin1;
		echoPin = echoPin1;
	}
	else if(num == 1) {
		trigPin = trigPin2;
		echoPin = echoPin2;
	}
	else if(num == 2) {
		trigPin = trigPin3;
		echoPin = echoPin3;
	}
	else {
		printf("thread_sonic init error\n");
		return 0;
	}
	digitalWrite (trigPin, LOW);
	usleep(2);
	digitalWrite (trigPin, HIGH);
	usleep(20);
	digitalWrite (trigPin, LOW);

	while(digitalRead(echoPin) == LOW);

	long startTime = micros();
	while(digitalRead(echoPin) == HIGH);
	long travelTime = micros() - startTime;

	distance = travelTime / 58;

	//printf("%d sonic, Distance: %dcm\n", num,distance);
	return distance;
}
void decToStr(int dec, char* str) {
	int i;
	for(i=3; i>=0; i--) {
		str[i] = dec%10 + '0';
		dec/=10;
	}
}
