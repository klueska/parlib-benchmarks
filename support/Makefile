C_EXECS = frequency-test fsbase-test
CXX_EXECS = blast
UPTHREAD_C_EXECS = pi

EXECS = $(C_EXECS) $(CXX_EXECS) $(UPTHREAD_C_EXECS)

all: $(EXECS)

$(CXX_EXECS): %: %.cc
	g++ -g -std=c++11 -O2 -o $@ $^ -pthread

$(C_EXECS): %: %.c
	$(CC) -g -o $(@) -O2 -std=gnu99 $(^) -lparlib -lpthread

$(UPTHREAD_C_EXECS): %: %.c
	$(CC) -g -o $(@) -O2 -std=gnu99 -DUSE_UPTHREAD $(^) -lm -lupthread -lparlib

clean:
	rm -rf $(EXECS)
