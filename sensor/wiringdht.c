#include <wiringPi.h>
#include <stdio.h>
#include <pthread.h>
#include "wiringdht.h"
#define MAX_COUNT 85
#define DHT_PIN 1

pthread_mutex_t dhtmutex= PTHREAD_MUTEX_INITIALIZER;
char dhtdata[6] = {0,};
int dhtVal[5] = {0,0,0,0,0};

void* thread_dht(void* arg) {
	unsigned short state = HIGH;
	unsigned short counter = 0; unsigned short j = 0, i;
	float farenheit;

	while(1) {
		state = HIGH;
		counter = j = 0; 
		for(i=0; i<5; i++) 
			dhtVal[i] = 0;

		pinMode(DHT_PIN, OUTPUT);
		digitalWrite(DHT_PIN, LOW);
		delay(18);
		digitalWrite(DHT_PIN,HIGH);
		delayMicroseconds(40);
		pinMode(DHT_PIN,INPUT);

		for(i=0; i<MAX_COUNT; i++) {
			counter = 0;

			while(digitalRead(DHT_PIN) == state) {
				counter++;
				delayMicroseconds(1);
				if (counter == 255) break;
			}

			state =  digitalRead(DHT_PIN);
			if(counter == 255)break;

			/* 상위 3개 변화량은 무시 */
			if ((i>=4) && (i%2) == 0) {
				dhtVal[j/8] <<=1;
				if(counter > 16) dhtVal[j/8] |=1;
				j++;
			}
		}

		/*체크섬 검사 측정 값 출력 */
		if (( j>= 40) && (dhtVal[4] == ((dhtVal[0] + dhtVal[1] + dhtVal[2] +
							dhtVal[3]) &  0xFF))) {
			farenheit = dhtVal[2]*9./5.+32;

			pthread_mutex_lock(&dhtmutex);
			sprintf(dhtdata,"%d%d%d%d",dhtVal[0],dhtVal[1],dhtVal[2],dhtVal[3]);
			pthread_mutex_unlock(&dhtmutex);

//			printf("Humidity = %d.%d %% Temperature = %d.%d *C (%.1f *F\n", dhtVal[0], dhtVal[1], dhtVal[2], dhtVal[3], farenheit);
		}
		//else printf("Invalid Data!!\n");
		delay(3000);
	}
}
