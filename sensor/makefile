CC = gcc
OBJS = wiringdht.o ultrasonic.o
TARGET = ultrasonic

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) -lwiringPi -lpthread

clean :
	rm -f $(OBJS) $(TARGET)
