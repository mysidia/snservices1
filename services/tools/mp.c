#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main()
{
	char buf[512];
	long sst, sstn;
	int emNum, emP, noneNum, noneP;
	int negNum, negP, nicksTot, nicksH;

	FILE *fpNicks = fopen("nicks-old.dat", "w");
	FILE *fpNicksHeldBan = fopen("nicksheldban.dat", "w");

	sst = sstn = 0;

	while(fgets(buf, 512, stdin)) {
		if (*buf == '[') {
			if ( sscanf(buf, "[%ld]", &sstn) < 1)
				abort();
		}
		if (*buf == 'E' && buf[1] == '-') {
			if ( sscanf(buf, "E-mail addies          :  %d( %d%%) * Elected to set none    :  %d( %d%%)", 
				&emNum, &emP, &noneNum, &noneP) < 4 )
					abort();
		}
		if (*buf == 'N' && buf[1] == 'e') {
			int pz;
			if ( pz = sscanf(buf, "Neglected e-mail field :   %d(  %d%%) * Nicks                  :  %d/   %d",
				&negNum, &negP, &nicksTot, &nicksH) < 3 )
					abort();

			if ((sst - 851990400) / (3600*24) != (sstn - 851990400) / (3600*24))
			{
				sst = sstn;
	
				fprintf(fpNicks, "%ld %ld\n",  (sstn - 851990400) / (3600*24), nicksTot);
				if (pz >= 4)
					fprintf(fpNicksHeldBan, "%ld %ld\n",  (sstn - 851990400) / (3600*24), nicksTot);
			}
		}
	}
}
