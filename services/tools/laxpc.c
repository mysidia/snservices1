#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main()
{
	char buf[512];
	time_t v = 851990400, z;
	int xps[5000]={}, rgs[5000]={}, d = 0;
	FILE *fpReg = fopen("cregstats.dat", "w"), *fpExp = fopen("cexpstats.dat", "w");

	{
		rewind(stdin);

		while(fgets(buf, 512, stdin)) {
			if (strncmp (buf, "CS_RE", 5) == 0 || strncmp (buf, "CSE_EXP", 7) == 0)
			{
				if (sscanf (buf, "CS_REGISTER %ld - *", &z) > 0)
					if (z >= v)
						rgs[(z - v) / 86400]++;

				if (sscanf (buf, "CSE_EXPIRE %ld - *", &z) > 0)
					if (z >= v)
						xps[(z - v) / 86400]++;
			}
		}
	}

	for(d = 0; d < 5000; d++)
	{
		if (rgs[d] != 0)		
			fprintf(fpReg, "%d %d\n", d, rgs[d]);
		if (xps[d] != 0)
			fprintf(fpExp, "%d %d\n", d, xps[d]);
	}
	fclose(fpReg);
	fclose(fpExp);
}
