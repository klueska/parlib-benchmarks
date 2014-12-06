C_EXECS = frequency-test fsbase-test
CXX_EXECS = blast

EXECS = $(C_EXECS) $(CXX_EXECS)

all: $(EXECS)

$(CXX_EXECS): %: %.cc
	g++ -std=c++11 -O2 -o $@ $^ -pthread

$(C_EXECS): %: %.c
	$(CC) -g -o $(@) -O2 -std=gnu99 $(^) -lparlib -lpthread

clean:
	rm -rf $(EXECS)
