CC = gcc
OBJS = motor.o piserver.o
TARGET = piserver

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) -lwiringPi -lpthread

clean :
	rm -f $(OBJS) $(TARGET)
