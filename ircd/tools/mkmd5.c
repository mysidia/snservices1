#include <stdio.h>
#include <unistd.h>

#include "setup.h"
#include "md5.h"
#include "md5_iface.h"

#ifdef STRINGH
#include <string.h>
#endif

#ifdef STRINGSH
#include <strings.h>
#endif

int main(int argc, char **argv)
{
  char temp[255], temp2[255];
  char *plaintext = getpass("plaintext: ");

  strcpy(temp, ircd_md5_hex(plaintext));

  plaintext = getpass("Again: ");

  strcpy(temp2, ircd_md5_hex(plaintext));
  memset(plaintext, 0, 128);

  if (strcmp(temp, temp2)) {
      printf("\nPasswords didn't match.\n");
      return 0;
  }

  printf("%s\n", temp2);
  return 0;
}
