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
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
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

static struct semaphore *NW;	// NW 방향 추가
static struct semaphore *NE;	// NE 방향 추가
static struct semaphore *SW;	// SW 방향 추가
static struct semaphore *SE;	// SE 방향 추가
static struct semaphore *INTER; // 교차로에 들어올 수 있는 차의 개수
int numcallmessage = 0;			// message_function 호출 횟수

static void inititems(void)
{
	if (testsem == NULL)
	{
		testsem = sem_create("testsem", 2);
		if (testsem == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
	if (testlock == NULL)
	{
		testlock = lock_create("testlock");
		if (testlock == NULL)
		{
			panic("synchtest: lock_create failed\n");
		}
	}
	if (testcv == NULL)
	{
		testcv = cv_create("testlock");
		if (testcv == NULL)
		{
			panic("synchtest: cv_create failed\n");
		}
	}
	if (donesem == NULL)
	{
		donesem = sem_create("donesem", 0);
		if (donesem == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}

	// 추가한 부분(NW, NE, SW, SE, INTER 세마포어 생성)
	if (NW == NULL)
	{
		NW = sem_create("NW", 1);
		if (NW == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
	if (NE == NULL)
	{
		NE = sem_create("NE", 1);
		if (NE == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
	if (SW == NULL)
	{
		SW = sem_create("SW", 1);
		if (SW == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
	if (SE == NULL)
	{
		SE = sem_create("SE", 1);
		if (SE == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
	if (INTER == NULL)
	{
		INTER = sem_create("INTER", 3);
		if (INTER == NULL)
		{
			panic("synchtest: sem_create failed\n");
		}
	}
}
static void message_create(int car_num, char start, int turnnum)
{
	/*스레드가 생성되었을 때 메시지 출력*/
	char str[15];

	if (turnnum == 0)
		strcpy(str, "go straight");
	else if (turnnum == 1)
		strcpy(str, "turn right");
	else if (turnnum == 2)
		strcpy(str, "turn left");
	else
		strcpy(str, "error");

	P(testsem);
	kprintf("car: %d, waiting in %c to %s\n", car_num, start, str);
	V(donesem);
}
static void message_function(int car_num, const char *start, const char *before, const char *current, const char *after, const char *destination)
{
	/*메시지 출력*/
	if (strcmp(start, before) == 0)
	{
		/* 교차로에 방금 진입했을 때 */
		P(testsem);
		kprintf("car: %d, enter %s -> %s\n", car_num, before, current);
		V(donesem);
	}
	else if (strcmp(current, destination) == 0)
	{
		/* 목적지에 도착하여 교차로를 빠져나올 때 */
		P(testsem);
		kprintf("car: %d, arrive %s, start: %s\n", car_num, destination, start);
		V(donesem);
	}
	else
	{
		/* 교차로내에서 움직일 때 */
		P(testsem);
		kprintf("car: %d, moved %s -> %s, start: %s, after: %s, dest: %s\n", car_num, before, current, start, after, destination);
		V(donesem);
	}
}
static void gostraightprocess(int car_num, const char *start, struct semaphore *step1, struct semaphore *step2, const char *dest)
{
	/* 직진 과정을 나타내는 함수. step1은 맨 처음 교차로에 진입했을 때의 교차로 위치, step2는 다음에 이동한 교차로의 위치 */
	P(INTER); // 교차로 진입
	P(step1);
	message_function(car_num, start, start, step1->sem_name, step2->sem_name, dest);
	P(step2);
	message_function(car_num, start, step1->sem_name, step2->sem_name, dest, dest);
	V(step1);
	message_function(car_num, start, step2->sem_name, dest, dest, dest);
	V(step2);
	V(INTER); // 교차로 진입
}
static void gostraight(int car_num, char start)
{
	/* 차량이 교차로에서 직진함을 의미하는 함수 */
	switch (start)
	{
	case 'N':
		gostraightprocess(car_num, "N", NW, SW, "S");
		break;
	case 'E':
		gostraightprocess(car_num, "E", NE, NW, "W");
		break;
	case 'S':
		gostraightprocess(car_num, "S", SE, NE, "N");
		break;
	case 'W':
		gostraightprocess(car_num, "W", SW, SE, "E");
		break;
	default:
		P(testsem);
		kprintf("잘못된 입력입니다.\n");
		V(donesem);
		break;
	}
}

static void leftturnprocess(int car_num, const char *start, struct semaphore *step1, struct semaphore *step2, struct semaphore *step3, const char *dest)
{
	/* 좌회전 과정을 나타내는 함수. 과정은 gostraight과 유사 */
	P(INTER); // 교차로 진입
	P(step1);
	message_function(car_num, start, start, step1->sem_name, step2->sem_name, dest);
	P(step2);
	message_function(car_num, start, step1->sem_name, step2->sem_name, step3->sem_name, dest);
	V(step1);
	P(step3);
	message_function(car_num, start, step2->sem_name, step3->sem_name, dest, dest);
	V(step2);
	message_function(car_num, start, step3->sem_name, dest, dest, dest);
	V(step3);
	V(INTER); // 교차로 진입
}
static void leftturn(int car_num, char start)
{
	/* 차량이 교차로에서 좌회전함을 의미하는 함수 */
	switch (start)
	{
	case 'N':
		leftturnprocess(car_num, "N", NW, SW, SE, "E");
		break;
	case 'E':
		leftturnprocess(car_num, "E", NE, NW, SW, "S");
		break;
	case 'S':
		leftturnprocess(car_num, "S", SE, NE, NW, "W");
		break;
	case 'W':
		leftturnprocess(car_num, "W", SW, SE, NE, "N");
		break;
	default:
		P(testsem);
		kprintf("잘못된 입력입니다.\n");
		V(donesem);
		break;
	}
}
static void rightturnprocess(int car_num, const char *start, struct semaphore *step, const char *dest)
{
	/* 우회전 과정을 나타내는 함수 */
	P(INTER); // 교차로 진입
	P(step);
	message_function(car_num, start, start, step->sem_name, dest, dest);
	message_function(car_num, start, step->sem_name, dest, dest, dest);
	V(step);
	V(INTER); // 교차로 진입
}
static void rightturn(int car_num, char start)
{
	/* 차량이 교차로에서 우회전함을 의미하는 함수 */
	switch (start)
	{
	case 'N':
		rightturnprocess(car_num, "N", NW, "W");
		break;
	case 'E':
		rightturnprocess(car_num, "E", NE, "N");
		break;
	case 'S':
		rightturnprocess(car_num, "S", SE, "E");
		break;
	case 'W':
		rightturnprocess(car_num, "W", SW, "S");
		break;
	default:
		P(testsem);
		kprintf("잘못된 입력입니다.\n");
		V(donesem);
		break;
	}
}

static void semtestthread(void *junk, unsigned long num)
{
	(void)junk;
	/*
	 * Only one of these should print at a time.
	 */
	int startnum = random() % 4; // 시작위치 생성
	int turnnum = random() % 3;	 // 직진, 우회전, 좌회전
	char start;					 // 시작위치

	/* 시작 위치 */
	if (startnum == 0)
		start = 'N';
	else if (startnum == 1)
		start = 'E';
	else if (startnum == 2)
		start = 'S';
	else
		start = 'W';

	message_create(num, start, turnnum); // 스레드 생성과 교차로의 차량 대수는 별개이므로 여기에서 생성 메시지 호출

	/* 직진, 우회전, 좌회전 */
	if (turnnum == 0)
	{
		numcallmessage += 3; // 직진에서 message_function 호출 횟수 = 3
		gostraight(num, start);
	}
	else if (turnnum == 1)
	{
		numcallmessage += 2; // 우회전에서 message_function 호출 횟수 = 2
		rightturn(num, start);
	}
	else
	{
		numcallmessage += 4; // 좌회전에서 message_function 호출 횟수 = 4
		leftturn(num, start);
	}
}

int semtest(int nargs, char **args)
{
	int i, result;

	(void)nargs;
	(void)args;

	inititems();
	kprintf("Starting semaphore test...\n");
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
	for (i = 0; i < numcallmessage; i++)
	{
		// message_function에서의 testsem, donesem 회수
		V(testsem);
		P(donesem);
	}

	/* so we can run it again */
	V(testsem);
	V(testsem);

	kprintf("Semaphore test done.\n");
	kprintf("ver 4.1\n");
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

	sem_destroy(exitsem);
	sem_destroy(gatesem);
	exitsem = gatesem = NULL;
	for (i = 0; i < NCVS; i++)
	{
		lock_destroy(testlocks[i]);
		cv_destroy(testcvs[i]);
		testlocks[i] = NULL;
		testcvs[i] = NULL;
	}

	kprintf("cvtest2 done\n");
	return 0;
}