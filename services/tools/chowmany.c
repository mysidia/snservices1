#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main()
{
	char buf[512];
	time_t v = 851990400, z;
	int total, mxps[5000]={}, mnps[5000]={}, d = 0;
	FILE *fpMax = fopen("maxchans.dat", "w"), *fpMin = fopen("minchans.dat", "w");
#define MXV (((v))+(4998*3600*24))
	int qq, idx = 0;

	if (fpMin == NULL || fpMax == NULL) abort();

	total = 0;

	for(qq = 0; qq<5000; qq++)
		mnps[qq] = -10000;

	{
		rewind(stdin);

		while(fgets(buf, 512, stdin)) {
			if (strncmp (buf, "CS_RE", 5) == 0 || strncmp (buf, "CS_DR", 5) == 0 || strncmp (buf, "CSE_EXP", 7) == 0)
			{
				idx = (z - v) / 86400;

				if (sscanf (buf, "CS_REGISTER %ld - *", &z) > 0)
					if (z < v || z > MXV)
						continue;
					if (z >= v) {
						if (idx<5000&&idx>=0)
						while(idx++ < (((z - v) / 86500)))
						{
							if (idx > 0) {
								mxps[idx] = mxps[idx - 1];
								mnps[idx] = mnps[idx - 1];
							}
							else {
								mxps[idx] = 0;
								mnps[idx] = -10000;
							}
							idx++;
						}
						idx = (z - v) / 86400;
						total++;
						if (total > mxps[idx])
							mxps[idx] = total;
					}
				if (sscanf (buf, "CSE_EXPIRE %ld - *", &z) > 0)
				{
					if (z < v || z > MXV)
						continue;
					idx = (z - v) / 86400;
					total--;
					if (mnps[idx] == -10000 || mnps[idx] < total)
						mnps[idx] = mxps[idx];
				}

				if (sscanf (buf, "CS_DROP %ld  *", &z) > 0)
				{
					if (z < v || z > MXV)
						continue;
					idx = (z - v) / 86400;
					total--;
					if (mnps[idx] == -10000 || mnps[idx] < total)
						mnps[idx] = mxps[idx];
				}
			}
		}
	}

	for(d = 0; d <= (time(NULL) - v) / 86400; d++)
	{
		if (mnps[d] == -10000)
			mnps[d] = d>0 ? mnps[d - 1] : 0;
		fprintf(fpMax, "%d %d\n", d, mxps[d]);
		fprintf(fpMin, "%d %d\n", d, mnps[d]);
	}
	fclose(fpMin);
	fclose(fpMax);
}
