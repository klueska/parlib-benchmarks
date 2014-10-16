#include <stdio.h>
#include <stdint.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <parlib/timing.h>

int arch_prctl(int code, unsigned long *addr);

static inline unsigned long rdfsbase(void)
{
	unsigned long fs;
	asm volatile(".byte 0xf3,0x48,0x0f,0xae,0xc0 # rdfsbaseq %%rax"
	             : "=a" (fs)
	             :: "memory");
	return fs;
}

static inline void wrfsbase(unsigned long fs)
{
	asm volatile(".byte 0xf3,0x48,0x0f,0xae,0xd0 # wrfsbaseq %%rax"
	             :: "a" (fs)
	             : "memory");
}

int main (int argc, char **argv)
{
	const uint64_t iters = 10000000;
	unsigned long tls_addr;
	uint64_t beg, end;

	// Read fs base with syscall
	beg = read_tsc();
	for (int i=0; i < iters; i++)
		arch_prctl(ARCH_GET_FS, &tls_addr);
	end = read_tsc();
	printf("arch_prctl get: %ldns\n", tsc2nsec(end - beg)/iters);

	// Write fs base with syscall
	beg = read_tsc();
	for (int i=0; i < iters; i++)
		arch_prctl(ARCH_SET_FS, (void *)tls_addr);
	end = read_tsc();
	printf("arch_prctl set: %ldns\n", tsc2nsec(end - beg)/iters);

	// Read fs base with rdfsbase instruction
	beg = read_tsc();
	for (int i=0; i < iters; i++)
		tls_addr = rdfsbase();
	end = read_tsc();
	printf("rdfsbase: %ldns\n", tsc2nsec(end - beg)/iters);

	// Write fs base with wrfsbase instruction
	beg = read_tsc();
	for (int i=0; i < iters; i++)
		wrfsbase(tls_addr);
	end = read_tsc();
	printf("wrfsbase: %ldns\n", tsc2nsec(end - beg)/iters);

	// Write fs base with wrfsbase instruction
	beg = read_tsc();
	for (int i=0; i < iters; i++) {
	  uint32_t lo, hi;
	  asm volatile("rdtscp" : "=a" (lo), "=d" (hi));
	  (uint64_t)hi << 32 | lo;
  }
	end = read_tsc();
	printf("rdtscp: %ldns\n", tsc2nsec(end - beg)/iters);
}
