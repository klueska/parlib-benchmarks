EXECS = frequency-test fsbase-test

all: $(EXECS)

$(EXECS): %: %.c
	$(CC) -o $(@) -std=gnu99 $(^) -lparlib

clean:
	rm -rf $(EXECS)
