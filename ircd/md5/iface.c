#include "setup.h"
#include "sys.h"
#include "md5.h"
#include "md5_iface.h"

const char *ircd_md5_hex(const char *passwd)
{
	static char buf[64+5];
	unsigned char target_buf[64];

	md5_buffer((unsigned char *)passwd, strlen(passwd), target_buf);

	strcpy(buf, "MD5(");
	strcpy(buf+4, md5_printable(target_buf));
	strcat(buf, ")");

	return buf;
}

const char *md5_printable(unsigned char md5buffer[16])
{
  unsigned static char tresult[64];
  int cnt = 0, o = 0;

  *tresult = 0;
  for (cnt = 0; cnt < 16; ++cnt)
       o+= sprintf ((char *)(tresult + o), "%02x", md5buffer[cnt]);
  tresult[32] = '\0';

  return (char *)tresult;
}

void md5_buffer(unsigned char *passwd, int len, unsigned char *target)
{
     struct MD5Context ctx;

     MD5Init(&ctx);
     MD5Update(&ctx, passwd, len);
     MD5Final(target, &ctx);
}

