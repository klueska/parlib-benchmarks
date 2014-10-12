EXECS = frequency-test

all: $(EXECS)

frequency-test: %: %.c
	$(CC) -o $(@) $(^)

clean:
	rm -rf $(EXECS)
