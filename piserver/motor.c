#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "motor.h"

int sock;
struct sockaddr_in client_addr;
struct sockaddr_in server_addr;

void* thread_motor(void*arg){
	wiringPiSetup();
	pinMode(M11, OUTPUT);
	pinMode(M12, OUTPUT);
	pinMode(M21, OUTPUT);
	pinMode(M22, OUTPUT);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		printf("Error\n");
		return 0;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(6789);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
		printf("bind error\n");
		return 0;
	}

	int client_addr_size;
	int leng = 0;
	char buf[2];

	int i = 0;

	while(1){
		client_addr_size = sizeof(client_addr);
		leng = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &client_addr_size); 
		if(leng > 0){
			printf("%c\n", buf[0]);
			if		(buf[0] == 'F'){
				digitalWrite(M11,LOW);
				digitalWrite(M12,HIGH);
			}
			else if	(buf[0] == 'S') {
				digitalWrite(M11,LOW);
				digitalWrite(M12,LOW);
			}
			else if	(buf[0] == 'B') {
				digitalWrite(M12,LOW);
				digitalWrite(M11,HIGH);
			}

			if		(buf[1] == 'L') {
				digitalWrite(M22, LOW);
				digitalWrite(M21, HIGH);
			}
			else if(buf[1] == 'C') {
				digitalWrite(M22, LOW);
				digitalWrite(M21, LOW);
			}
			else if(buf[1] == 'R') {
				digitalWrite(M21, LOW);
				digitalWrite(M22, HIGH);
			}
		}
		printf("%d\n", i++);
	}


	/*
	digitalWrite(M12, HIGH);
	delay(1000);
	digitalWrite(M12, LOW);


	digitalWrite(M22, HIGH);
	delay(1000);
	digitalWrite(M22, LOW);
	delay(1000);
	digitalWrite(M21, HIGH);
	delay(1000);
	digitalWrite(M21, LOW);
	*/
}




