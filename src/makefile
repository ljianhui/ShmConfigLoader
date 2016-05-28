CC := g++
FLAGS := -g -Wall -O

SRCS := ${wildcard *.cpp}
OBJS := $(SRCS:.cpp=.o)
TARGET := ../lib/libshmconfigloader.a

all: $(OBJS)
	ar -r $(TARGET) $(OBJS)

$(OBJS): %.o: %.cpp
	$(CC) -c $< -o $@ $(FLAGS)

clean:
	-rm -f $(TARGET) *.o
