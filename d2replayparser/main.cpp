#include <stdio.h>
#include "demodump.h"

int main(int argc, char **argv)
{
	CDemoFileDump cd;
	if(cd.Open(argv[1]))
		cd.Dump();
	cd.Output();
	return 0;
}