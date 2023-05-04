#include "codec.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
	char data[] = "my text to encrypt";
	int key = 12;

	encrypt(data,key);
	printf("encripted data: %s\n",data);

	decrypt(data,key);
	printf("decripted data: %s\n",data);

	return 0;
}
