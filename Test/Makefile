TARGET_NAME=LWMessageQueueTest
CXX=g++
CXXFLAGS=-Wall -Werror -I../ -std=c++11 -O3
LDFLAGS=-lpthread

DEPS = \
	../LWMessageQueue.h

OBJ = LWMessageQueueTest.o


$(ODIR)/%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

$(TARGET_NAME): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

all: $(TARGET_NAME)

.PHONY: clean

clean:
	rm -f *.o $(TARGET_NAME)

