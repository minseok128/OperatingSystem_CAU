/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS CAR_COUNTRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization test code.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>

#define NSEMLOOPS 63
#define NLOCKLOOPS 120
#define NCVLOOPS 5
#define NTHREADS 32

static volatile unsigned long testval1;
static volatile unsigned long testval2;
static volatile unsigned long testval3;
static struct semaphore *testsem;
static struct lock *testlock;
static struct cv *testcv;
static struct semaphore *donesem;

static struct semaphore *FIELD[4]; // 0: NW, 1: NE, 2: SW, 3: SE
static struct semaphore *CAR_COUNT;
int message_count = 0;

int info[4][3] = {
	{4, 7, 6},
	{5, 4, 7},
	{6, 5, 4},
	{7, 6, 5}};

static const char *
get_dir(int num)
{
	switch (num)
	{
	case 0:
		return ("N");
	case 1:
		return ("E");
	case 2:
		return ("S");
	case 3:
		return ("W");
	case 4:
		return ("NW");
	case 5:
		return ("NE");
	case 6:
		return ("SE");
	case 7:
		return ("SW");
	}
	return ("ERROR");
}

static const char *
get_turn(int start, int end)
{
	if ((start + 2) % 4 == end)
		return ("going straight");
	else if ((start + 3) % 4 == end)
		return ("going right");
	else
		return ("going left");
	return ("ERROR");
}

static void
inititems(void)
{
	// 5개의 새운 세마포어 초기화
	for (int i = 0; i < 4; i++)
	{
		if (FIELD[i] == NULL)
		{
			FIELD[i] = sem_create(get_dir(i + 4), 1);
			if (FIELD[i] == NULL)
				panic("synchtest: sem_create failed\n");
		}
	}
	if (CAR_COUNT == NULL)
	{
		CAR_COUNT = sem_create("CAR_COUNT", 3);
		if (CAR_COUNT == NULL)
			panic("synchtest: sem_create failed\n");
	}

	if (testsem == NULL)
	{
		testsem = sem_create("testsem", 2);
		if (testsem == NULL)
			panic("synchtest: sem_create failed\n");
	}
	if (testlock == NULL)
	{
		testlock = lock_create("testlock");
		if (testlock == NULL)
			panic("synchtest: lock_create failed\n");
	}
	if (testcv == NULL)
	{
		testcv = cv_create("testlock");
		if (testcv == NULL)
			panic("synchtest: cv_create failed\n");
	}
	if (donesem == NULL)
	{
		donesem = sem_create("donesem", 0);
		if (donesem == NULL)
			panic("synchtest: sem_create failed\n");
	}
}

static void
print_state(int car, int start, int end, int before, int crr)
{
	int i;

	P(testsem);
	kprintf("Car %d: ", car);
	for (i = 0; i < NSEMLOOPS; i++)
		kprintf("%c", (int)car + 64);
	kprintf("\ncar# %d is comming from %s %s\n", car, get_dir(start), get_turn(start, end));

	kprintf("car = %d, in = %s, out = %s : %s->%s", car, get_dir(start), get_dir(end), get_dir(before), get_dir(crr));
	if (crr == end)
		kprintf("\tGOAL!!");
	kprintf("\n\n\n");
	V(donesem);
}

static void
move(int car, int start, int end, int type)
{
	int i = 0;

	P(CAR_COUNT);
	P(FIELD[info[start][i] - 4]);
	print_state(car, start, end, start, info[start][i]);
	for (i = 1; i <= type; i++)
	{
		P(FIELD[info[start][i] - 4]);
		print_state(car, start, end, info[start][i - 1], info[start][i]);
		V(FIELD[info[start][i - 1] - 4]);
	}
	V(FIELD[info[start][type] - 4]);
	print_state(car, start, end, info[start][i - 1], end);
	V(CAR_COUNT);
}

static void
semtestthread(void *junk, unsigned long car)
{
	int start, end, i;
	(void)junk;

	start = end = random() % 4;
	while (start == end)
		end = random() % 4;
	/*
	 * Only one of these should print at a time.
	 */

	P(testsem);
	kprintf("Car %d: ", (int)car);
	for (i = 0; i < NSEMLOOPS; i++)
		kprintf("%c", (int)car + 64);
	kprintf("\ncar: %d, waiting in %s to %s | %s", (int)car, get_dir(start), get_dir(end), get_turn(start, end));
	kprintf("\n\n");
	V(donesem);

	if ((start + 2) % 4 == end)
	{
		// 직진
		message_count += 3;
		move(car, start, end, 1);
	}
	else if ((start + 3) % 4 == end)
	{
		// 우회전
		message_count += 2;
		move(car, start, end, 0);
	}
	else
	{
		// 좌회전
		message_count += 4;
		move(car, start, end, 2);
	}
}

int semtest(int nargs, char **args)
{
	int i, result;

	(void)nargs;
	(void)args;

	inititems();
	kprintf("Starting semaphore test...\n");
	kprintf("20203361 Chang Minseok\n");
	kprintf("If this hangs, it's broken: ");
	P(testsem);
	P(testsem);
	kprintf("ok\n");

	for (i = 0; i < NTHREADS; i++)
	{
		result = thread_fork("semtest", NULL, semtestthread, NULL, i);
		if (result)
		{
			panic("semtest: thread_fork failed: %s\n",
				  strerror(result));
		}
	}

	for (i = 0; i < NTHREADS; i++)
	{
		// message_create에서의 testsem, donesem 회수
		V(testsem);
		P(donesem);
	}
	for (i = 0; i < message_count; i++)
	{
		// print_state에서의 testsem, donesem 회수
		V(testsem);
		P(donesem);
	}

	/* so we can run it again */
	V(testsem);
	V(testsem);

	kprintf("Semaphore test done.\n");
	return 0;
}

static void
fail(unsigned long num, const char *msg)
{
	kprintf("thread %lu: Mismatch on %s\n", num, msg);
	kprintf("Test failed\n");

	lock_release(testlock);

	V(donesem);
	thread_exit();
}

static void
locktestthread(void *junk, unsigned long num)
{
	int i;
	(void)junk;

	for (i = 0; i < NLOCKLOOPS; i++)
	{
		lock_acquire(testlock);
		testval1 = num;
		testval2 = num * num;
		testval3 = num % 3;

		if (testval2 != testval1 * testval1)
		{
			fail(num, "testval2/testval1");
		}

		if (testval2 % 3 != (testval3 * testval3) % 3)
		{
			fail(num, "testval2/testval3");
		}

		if (testval3 != testval1 % 3)
		{
			fail(num, "testval3/testval1");
		}

		if (testval1 != num)
		{
			fail(num, "testval1/num");
		}

		if (testval2 != num * num)
		{
			fail(num, "testval2/num");
		}

		if (testval3 != num % 3)
		{
			fail(num, "testval3/num");
		}

		lock_release(testlock);
	}
	V(donesem);
}

int locktest(int nargs, char **args)
{
	int i, result;

	(void)nargs;
	(void)args;

	inititems();
	kprintf("Starting lock test...\n");

	for (i = 0; i < NTHREADS; i++)
	{
		result = thread_fork("synchtest", NULL, locktestthread,
							 NULL, i);
		if (result)
		{
			panic("locktest: thread_fork failed: %s\n",
				  strerror(result));
		}
	}
	for (i = 0; i < NTHREADS; i++)
	{
		P(donesem);
	}

	kprintf("Lock test done.\n");

	return 0;
}

static void
cvtestthread(void *junk, unsigned long num)
{
	int i;
	volatile int j;
	struct timespec ts1, ts2;

	(void)junk;

	for (i = 0; i < NCVLOOPS; i++)
	{
		lock_acquire(testlock);
		while (testval1 != num)
		{
			gettime(&ts1);
			cv_wait(testcv, testlock);
			gettime(&ts2);

			/* ts2 -= ts1 */
			timespec_sub(&ts2, &ts1, &ts2);

			/* Require at least 2000 cpu cycles (we're 25mhz) */
			if (ts2.tv_sec == 0 && ts2.tv_nsec < 40 * 2000)
			{
				kprintf("cv_wait took only %u ns\n",
						ts2.tv_nsec);
				kprintf("That's too fast... you must be "
						"busy-looping\n");
				V(donesem);
				thread_exit();
			}
		}
		kprintf("Thread %lu\n", num);
		testval1 = (testval1 + NTHREADS - 1) % NTHREADS;

		/*
		 * loop a little while to make sure we can measure the
		 * time waiting on the cv.
		 */
		for (j = 0; j < 3000; j++)
			;

		cv_broadcast(testcv, testlock);
		lock_release(testlock);
	}
	V(donesem);
}

int cvtest(int nargs, char **args)
{

	int i, result;

	(void)nargs;
	(void)args;

	inititems();
	kprintf("Starting CV test...\n");
	kprintf("Threads should print out in reverse order.\n");

	testval1 = NTHREADS - 1;

	for (i = 0; i < NTHREADS; i++)
	{
		result = thread_fork("synchtest", NULL, cvtestthread, NULL, i);
		if (result)
		{
			panic("cvtest: thread_fork failed: %s\n",
				  strerror(result));
		}
	}
	for (i = 0; i < NTHREADS; i++)
	{
		P(donesem);
	}

	kprintf("CV test done\n");

	return 0;
}

////////////////////////////////////////////////////////////

/*
 * Try to find out if going to sleep is really atomic.
 *
 * What we'll do is rotate through NCVS lock/CV pairs, with one thread
 * sleeping and the other waking it up. If we miss a wakeup, the sleep
 * thread won't go around enough times.
 */

#define NCVS 250
#define NLOOPS 40
static struct cv *testcvs[NCVS];
static struct lock *testlocks[NCVS];
static struct semaphore *gatesem;
static struct semaphore *exitsem;

static void
sleepthread(void *junk1, unsigned long junk2)
{
	unsigned i, j;

	(void)junk1;
	(void)junk2;

	for (j = 0; j < NLOOPS; j++)
	{
		for (i = 0; i < NCVS; i++)
		{
			lock_acquire(testlocks[i]);
			V(gatesem);
			cv_wait(testcvs[i], testlocks[i]);
			lock_release(testlocks[i]);
		}
		kprintf("sleepthread: %u\n", j);
	}
	V(exitsem);
}

static void
wakethread(void *junk1, unsigned long junk2)
{
	unsigned i, j;

	(void)junk1;
	(void)junk2;

	for (j = 0; j < NLOOPS; j++)
	{
		for (i = 0; i < NCVS; i++)
		{
			P(gatesem);
			lock_acquire(testlocks[i]);
			cv_signal(testcvs[i], testlocks[i]);
			lock_release(testlocks[i]);
		}
		kprintf("wakethread: %u\n", j);
	}
	V(exitsem);
}

int cvtest2(int nargs, char **args)
{
	unsigned i;
	int result;

	(void)nargs;
	(void)args;

	for (i = 0; i < NCVS; i++)
	{
		testlocks[i] = lock_create("cvtest2 lock");
		testcvs[i] = cv_create("cvtest2 cv");
	}
	gatesem = sem_create("gatesem", 0);
	exitsem = sem_create("exitsem", 0);

	kprintf("cvtest2...\n");

	result = thread_fork("cvtest2", NULL, sleepthread, NULL, 0);
	if (result)
	{
		panic("cvtest2: thread_fork failed\n");
	}
	result = thread_fork("cvtest2", NULL, wakethread, NULL, 0);
	if (result)
	{
		panic("cvtest2: thread_fork failed\n");
	}

	P(exitsem);
	P(exitsem);

	sem_endroy(exitsem);
	sem_endroy(gatesem);
	exitsem = gatesem = NULL;
	for (i = 0; i < NCVS; i++)
	{
		lock_endroy(testlocks[i]);
		cv_endroy(testcvs[i]);
		testlocks[i] = NULL;
		testcvs[i] = NULL;
	}

	kprintf("cvtest2 done\n");
	return 0;
}