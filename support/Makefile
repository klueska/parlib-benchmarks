EXECS = frequency-test fsbase-test

all: $(EXECS)

$(EXECS): %: %.c
	$(CC) -g -o $(@) -O2 -std=gnu99 $(^) -lparlib -lpthread

clean:
	rm -rf $(EXECS)
