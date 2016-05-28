CC := g++
INCS := -I ../src
LIBS := -L../lib -lshmconfigloader
CFLAG := -g -Wall -O

SRCS := ${wildcard *.cpp}
DEPS := $(SRCS:.cpp=.d)
OBJS := $(SRCS:.cpp=.o)
EXES := ${basename $(SRCS)}

all: $(OBJS) $(DEPS) $(EXES)

$(EXES):%:%.o
	$(CC) -o $@ $< $(INCS) $(LIBS) $(CFLAG) 

$(OBJS):%.o:%.cpp
	$(CC) -c $< -o $@ $(INCS) $(CFLAG)

$(DEPS):%.d:%.cpp
	rm -f $@;
	$(CC) -MM $< $(INCS) > $@

-include $(DEPS)

clean:
	-rm -fr  $(EXES) *.o *.d
