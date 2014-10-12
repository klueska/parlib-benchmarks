BENCHMARKS = fixedwork
LIBS = native-pthread upthread upthread-juggle
CFLAGS += -std=gnu99 
#LDFLAGS += -Wl,--no-as-needed -lprofiler -Wl,--as-needed
LDFLAGS += 
NATIVE_EXTRAS = ../native-timing.c

CFILES = $(patsubst %, %.c, $(BENCHMARKS))
PTHREAD_EXECS = $(foreach b, $(BENCHMARKS), native-pthread-$b)
UPTHREAD_EXECS = $(foreach b, $(BENCHMARKS), upthread-$b)
UPTHREAD_YIELD_EXECS = $(foreach b, $(BENCHMARKS), upthread-yield-$b)
UPTHREAD_JUGGLE_EXECS = $(foreach b, $(BENCHMARKS), upthread-juggle-$b)

EXECS = $(PTHREAD_EXECS) $(UPTHREAD_EXECS) \
        $(UPTHREAD_YIELD_EXECS) $(UPTHREAD_JUGGLE_EXECS)

# Helper functions
space :=
space +=
define split_target
$(subst -, , $(1))
endef
define extract_benchmark
$(word $(words $(call split_target, $(1))), $(call split_target, $(1)))
endef
define EXEC-RULE
$(1): $(2)
	$(CC) -o $(1) $$(^) $(3) $(4)
endef
define PTHREAD-EXEC-RULE
$(call EXEC-RULE, $(1), $(call extract_benchmark, $(1)).c $(NATIVE_EXTRAS), \
                  $(CFLAGS) -DUSE_PTHREAD, $(LDFLAGS) -lpthread)
endef
define UPTHREAD-EXEC-RULE
$(call EXEC-RULE, $(1), $(call extract_benchmark, $(1)).c, \
                  $(CFLAGS) -DUSE_UPTHREAD, $(LDFLAGS) -lupthread -lparlib)
endef
define UPTHREAD-YIELD-EXEC-RULE
$(call EXEC-RULE, $(1), $(call extract_benchmark, $(1)).c, \
                  $(CFLAGS) -DUSE_UPTHREAD -DWITH_YIELD, \
									$(LDFLAGS) -lupthread -lparlib)
endef
define UPTHREAD-JUGGLE-EXEC-RULE
$(call EXEC-RULE, $(1), $(call extract_benchmark, $(1)).c, \
                  $(CFLAGS) -DUSE_UPTHREAD_JUGGLE , \
									$(LDFLAGS) -lupthread-juggle -lparlib)
endef

# All the targets
all: $(EXECS) 

# Build targets for the executables
$(foreach exec, $(PTHREAD_EXECS), $(eval $(call PTHREAD-EXEC-RULE, $(exec))))
$(foreach exec, $(UPTHREAD_EXECS), $(eval $(call UPTHREAD-EXEC-RULE, $(exec))))
$(foreach exec, $(UPTHREAD_YIELD_EXECS), $(eval $(call UPTHREAD-YIELD-EXEC-RULE, $(exec))))
$(foreach exec, $(UPTHREAD_JUGGLE_EXECS), $(eval $(call UPTHREAD-JUGGLE-EXEC-RULE, $(exec))))

clean:
	rm -rf $(EXECS)
