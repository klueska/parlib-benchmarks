BENCHMARKS = fixedwork
LIBS = native-pthread upthread upthread-juggle
CFLAGS += -std=gnu99 
#LDFLAGS += -Wl,--no-as-needed -lprofiler -Wl,--as-needed
LDFLAGS += 

UPTHREAD_YIELD_EXECS = $(foreach b, $(BENCHMARKS), upthread-yield-$b)
EXECS += $(UPTHREAD_YIELD_EXECS)

include ../Makefrag

$(foreach exec, $(UPTHREAD_YIELD_EXECS), $(eval $(call UPTHREAD-YIELD-EXEC-RULE, $(exec))))

