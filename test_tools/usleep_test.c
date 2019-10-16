#include <stdio.h>
#include <time.h>
#include <sys/time.h>

void nsleep(long us)
{
struct timespec wait;
	//printf("Will sleep for is %ld\n", diff); //This will take extra ~70 microseconds
	wait.tv_sec = us / (1000 * 1000);
	wait.tv_nsec = (us % (1000 * 1000)) * 1000;
	nanosleep(&wait, NULL);
}
 
main()
{
	struct timeval t1, t2;
	long long t;
	long microseconds = 970;
	volatile int counter;

//nanosleep test
	gettimeofday(&t1, NULL);
	for(counter = 0; counter < 1000 ; counter++)
		nsleep(microseconds);
	gettimeofday(&t2, NULL);
	t = ((t2.tv_sec * 1000000) + t2.tv_usec) - ((t1.tv_sec * 1000000) + t1.tv_usec);
	printf("Test nsleep took %lld (1000000) is >= 10%% deviation?\n",  t);
 
//usleep test
	gettimeofday(&t1, NULL);
	for(counter = 0; counter < 1000 ; counter++)
		usleep(1000);
	gettimeofday(&t2, NULL);
	t = ((t2.tv_sec * 1000000) + t2.tv_usec) - ((t1.tv_sec * 1000000) + t1.tv_usec);
	printf("Test usleep took %lld (1000000) is >= 10%% deviation?\n",  t);


/*
//sleep test
	gettimeofday(&t1, NULL);
	sleep(1);
	gettimeofday(&t2, NULL);
	t = ((t2.tv_sec * 1000000) + t2.tv_usec) - ((t1.tv_sec * 1000000) + t1.tv_usec);
	printf("Call to sleep(1) took %lld\n", t);
*/

}

