#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <string.h>
#include <asm/unistd.h>

#define SIZE_LONG	8

static long perf_event_open(struct perf_event_attr *hw_event,
		pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
						group_fd, flags);
	return ret;
}

#define DTLB_LOAD_MISSES_WALK_DURATION		0x531008
#define DTLB_STORE_MISSES_WALK_DURATION		0x531049
#define CPU_CLK_UNHALTED			0x53003C

static int set_perf_raw_event(struct perf_event_attr *attr, unsigned long event)
{
	memset(attr, 0, sizeof(struct perf_event_attr));
	attr->type = PERF_TYPE_RAW;
	attr->size = sizeof(struct perf_event_attr);
	attr->config = event;
	attr->disabled = 1;
	attr->exclude_kernel = 1;
	attr->exclude_hv = 1;
}

static inline int get_translation_overhead(int fd1, int fd2, int fd_load)
{
	unsigned long load_walk_duration = 0, store_walk_duration = 0;
	unsigned long total_cycles = 0;
	int ret;

	ret = read(fd1, &load_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd2, &store_walk_duration, SIZE_LONG);
	if (ret != SIZE_LONG)
		goto failure;

	ret = read(fd_load, &total_cycles, SIZE_LONG);
	if (ret != SIZE_LONG || total_cycles == 0)
		goto failure;

	/* TODO: Check for overflow conditions */
	return ((load_walk_duration + store_walk_duration) * 100)/ total_cycles;

failure:
	return -1;
}

/*
 * TODO: Error handling for the next 3 functions
 */
static inline void reset_events(int fd_load, int fd_store, int fd_total)
{
	ioctl(fd_load, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_store, PERF_EVENT_IOC_RESET, 0);
	ioctl(fd_total, PERF_EVENT_IOC_RESET, 0);
}

static inline void enable_events(int fd_load, int fd_store, int fd_total)
{
	ioctl(fd_load, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_store, PERF_EVENT_IOC_ENABLE, 0);
	ioctl(fd_total, PERF_EVENT_IOC_ENABLE, 0);
}

static inline void disable_events(int fd_load, int fd_store, int fd_total)
{
	ioctl(fd_load, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_store, PERF_EVENT_IOC_DISABLE, 0);
	ioctl(fd_total, PERF_EVENT_IOC_DISABLE, 0);
}

static inline void close_files(int fd_load, int fd_store, int fd_total)
{
	close(fd_load);
	close(fd_store);
	close(fd_total);
}

int update_translation_overhead(struct process *proc)
{
/*
 * 	pe_load  ---- DTLB LOAD MISSES WALK DURATION
 * 	pe_store ---- DTLB STORE MISSES WALK DURATION
 * 	pe_total ---- UNHALTED CLOCK CYCLES
 * 	fd_load  ---- File descriptor for pe_load
 * 	fd_store ---- File descriptor for pe_store
 * 	fd_total ---- File descriptor for pe_total
 */
	struct perf_event_attr pe_load, pe_store, pe_total;
	long long count, count2, count3, count4, count5;
	int fd_load = -1, fd_store = -1, fd_total = -1;
	int i, pid, overhead, sleep_period = 1;

	pid = proc->pid;
	/* set per event descriptors */
	set_perf_raw_event(&pe_load, DTLB_LOAD_MISSES_WALK_DURATION);
	set_perf_raw_event(&pe_store, DTLB_STORE_MISSES_WALK_DURATION);
	set_perf_raw_event(&pe_total, CPU_CLK_UNHALTED);

	/* open file descriptors for each perf event */
	fd_load = perf_event_open(&pe_load, pid, -1, -1, 0);
	fd_store = perf_event_open(&pe_store, pid, -1, -1, 0);
	fd_total = perf_event_open(&pe_total, pid, -1, -1, 0);

	/* check for error conditions */
	if (fd_load == -1 || fd_store == -1 || fd_total == -1) {
		printf("Failure while opening file descriptors: %d %d %d\n",
					fd_load,fd_store,fd_total);
		goto failure;
	}
	/* reset the counter values before profiling */
	reset_events(fd_load, fd_store, fd_total);
	enable_events(fd_load, fd_store, fd_total);

	/* sleep till the sampling period completes */
	sleep(sleep_period);

	/* disable profiling */
	disable_events(fd_load, fd_store, fd_total);

	/* get the translation overhead from the measured counters */
	proc->overhead = get_translation_overhead(fd_load, fd_store, fd_total);
	close_files(fd_load, fd_store, fd_total);
	return 0;

failure:
	return -1;
}
