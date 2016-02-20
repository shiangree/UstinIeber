#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void regist(long int n)
{
	pid_t pid=getpid();
	char *command=(char*)malloc(sizeof(pid)+23);
	sprintf(command, "echo %d >/proc/mp1/status", pid);
	system(command);
	while(1)
	{
		fac(n/65535);
	}
	free(command);
}

int fac(int n)
{
	if(n<=1) return 1;
	return n*fac(n-1);
}

int main(int argc, char const *argv[])
{

	regist(2147483647);
	return 0;
}
