#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

int _FD_SETSIZE = 1024;
char _NS_ADDRESS[256], _KLINE_ADDRESS[256];

char Makefile[] =
"CC=cl\n"
"FD_SETSIZE=/D FD_SETSIZE=$FD_SETSIZE\n"
"NS_ADDRESS=/D NS_ADDRESS=\"\\\"$NS_ADDRESS\\\"\"\n"
"KLINE_ADDRESS=/D KLINE_ADDRESS=\"\\\"$KLINE_ADDRESS\\\"\"\n"
"CFLAGS=/MT /O2 /I ./INCLUDE /Fosrc/ /nologo $(FD_SETSIZE) $(NS_ADDRESS) $(KLINE_ADDRESS) /D NOSPOOF=1 /c\n"
"INCLUDES=./include/struct.h ./include/config.h ./include/sys.h \\\n"
" ./include/common.h ./include/patchlevel.h ./include/h.h ./include/numeric.h \\\n"
" ./include/msg.h ./include/setup.h\n"
"LINK=link.exe\n"
"LFLAGS=kernel32.lib user32.lib gdi32.lib shell32.lib wsock32.lib \\\n"
" oldnames.lib libcmt.lib /nodefaultlib /nologo /out:SRC/WIRCD.EXE\n"
"OBJ_FILES=SRC/CHANNEL.OBJ SRC/USERLOAD.OBJ SRC/SEND.OBJ SRC/BSD.OBJ \\\n"
" SRC/CIO_MAIN.OBJ SRC/S_CONF.OBJ SRC/DBUF.OBJ SRC/RES.OBJ \\\n"
" SRC/HASH.OBJ SRC/CIO_INIT.OBJ SRC/PARSE.OBJ SRC/IRCD.OBJ \\\n"
" SRC/S_NUMERIC.OBJ SRC/WHOWAS.OBJ SRC/RES_COMP.OBJ SRC/S_AUTH.OBJ \\\n"
" SRC/HELP.OBJ SRC/S_MISC.OBJ SRC/MATCH.OBJ SRC/CRULE.OBJ \\\n"
" SRC/S_DEBUG.OBJ SRC/RES_INIT.OBJ SRC/SUPPORT.OBJ SRC/LIST.OBJ \\\n"
" SRC/S_ERR.OBJ SRC/PACKET.OBJ SRC/CLASS.OBJ SRC/S_BSD.OBJ \\\n"
" SRC/MD5.OBJ SRC/S_SERV.OBJ SRC/S_USER.OBJ SRC/WIN32.OBJ \\\n"
" SRC/VERSION.OBJ SRC/WIN32.RES\n"
"RC=rc.exe\n"
"\n"
"ALL: SRC/WIRCD.EXE SRC/CHKCONF.EXE\n"
"        @echo Please, please REMEMBER to add those U lines!\n"
"        @echo Read the file READTHIS.NOW formore info\n"
"\n"
"CLEAN:\n"
"        -@erase src\\*.exe 2>NUL\n"
"        -@erase src\\*.obj 2>NUL\n"
"        -@erase src\\win32.res 2>NUL\n"
"        -@erase src\\version.c 2>NUL\n"
"\n"
"include/setup.h:\n"
"        @echo Hmm...doesn't look like you've run Config...\n"
"        @echo Doing so now.\n"
"        @config.exe\n"
"\n"
"src/version.c: dummy\n"
"        @config.exe -v\n"
"\n"
"src/version.obj: src/version.c\n"
"        $(CC) $(CFLAGS) src/version.c\n"
"\n"
"SRC/WIRCD.EXE: $(OBJ_FILES) src/version.obj\n"
"        $(LINK) $(LFLAGS) $(OBJ_FILES)\n"
"\n"
"SRC/CHKCONF.EXE: ./include/struct.h ./include/config.h ./include/sys.h \\\n"
"                 ./include/common.h ./src/crule.c ./src/match.c ./src/chkconf.c\n"
"        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkcrule.obj /c src/crule.c\n"
"        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkmatch.obj /c src/match.c\n"
"        $(CC) /nologo /I ./include /D CR_CHKCONF /Fosrc/chkconf.obj /c src/chkconf.c\n"
"        $(LINK) /nologo /out:src/chkconf.exe src/chkconf.obj src/chkmatch.obj \\\n"
"                src/chkcrule.obj\n"
"\n"
"src/parse.obj: src/parse.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/parse.c\n"
"\n"
"src/bsd.obj: src/bsd.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/bsd.c\n"
"\n"
"src/dbuf.obj: src/dbuf.c $(INCLUDES) ./include/dbuf.h\n"
"        $(CC) $(CFLAGS) src/dbuf.c\n"
"\n"
"src/packet.obj: src/packet.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/packet.c\n"
"\n"
"src/send.obj: src/send.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/send.c\n"
"\n"
"src/match.obj: src/match.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/match.c\n"
"\n"
"src/support.obj: src/support.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/support.c\n"
"\n"
"src/channel.obj: src/channel.c $(INCLUDES) ./include/channel.h\n"
"        $(CC) $(CFLAGS) src/channel.c\n"
"\n"
"src/class.obj: src/class.c $(INCLUDES) ./include/class.h\n"
"        $(CC) $(CFLAGS) src/class.c\n"
"\n"
"src/ircd.obj: src/ircd.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/ircd.c\n"
"\n"
"src/list.obj: src/list.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/list.c\n"
"\n"
"src/res.obj: src/res.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/res.c\n"
"\n"
"src/s_bsd.obj: src/s_bsd.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/s_bsd.c\n"
"\n"
"src/s_auth.obj: src/s_auth.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/s_auth.c\n"
"\n"
"src/s_conf.obj: src/s_conf.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/s_conf.c\n"
"\n"
"src/s_debug.obj: src/s_debug.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/s_debug.c\n"
"\n"
"src/s_err.obj: src/s_err.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/s_err.c\n"
"\n"
"src/s_misc.obj: src/s_misc.c $(INCLUDES) ./include/dbuf.h\n"
"        $(CC) $(CFLAGS) src/s_misc.c\n"
"\n"
"src/s_user.obj: src/s_user.c $(INCLUDES) ./include/dbuf.h \\\n"
"                ./include/channel.h ./include/whowas.h\n"
"        $(CC) $(CFLAGS) src/s_user.c\n"
"\n"
"src/s_serv.obj: src/s_serv.c $(INCLUDES) ./include/dbuf.h ./include/whowas.h\n"
"        $(CC) $(CFLAGS) src/s_serv.c\n"
"\n"
"src/s_numeric.obj: src/s_numeric.c $(INCLUDES) ./include/dbuf.h\n"
"        $(CC) $(CFLAGS) src/s_numeric.c\n"
"\n"
"src/whowas.obj: src/whowas.c $(INCLUDES) ./include/dbuf.h ./include/whowas.h\n"
"        $(CC) $(CFLAGS) src/whowas.c\n"
"\n"
"src/hash.obj: src/hash.c $(INCLUDES) ./include/hash.h\n"
"        $(CC) $(CFLAGS) src/hash.c\n"
"\n"
"src/crule.obj: src/crule.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/crule.c\n"
"\n"
"src/win32.obj: src/win32.c $(INCLUDES) ./include/resource.h\n"
"        $(CC) $(CFLAGS) src/win32.c\n"
"\n"
"src/cio_main.obj: src/cio_main.c $(INCLUDES) ./include/cio.h ./include/ciofunc.h\n"
"        $(CC) $(CFLAGS) src/cio_main.c\n"
"\n"
"src/cio_init.obj: src/cio_init.c $(INCLUDES) ./include/cio.h ./include/ciofunc.h\n"
"        $(CC) $(CFLAGS) src/cio_init.c\n"
"\n"
"src/res_comp.obj: src/res_comp.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/res_comp.c\n"
"\n"
"src/res_init.obj: src/res_init.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/res_init.c\n"
"\n"
"src/help.obj: src/help.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/help.c\n"
"\n"
"src/md5.obj: src/md5.c $(INCLUDES)\n"
"        $(CC) $(CFLAGS) src/md5.c\n"
"\n"
"src/win32.res: src/win32.rc\n"
"        $(RC) /l 0x409 /fosrc/win32.res /i ./include /i ./src \\\n"
"              /d NDEBUG src/win32.rc\n"
"\n"
"dummy:\n"
"\n";


char SetupH[] =
"#ifndef __setup_include__\n"
"#define __setup_include__\n"
"#undef  PARAMH\n"
"#undef  UNISTDH\n"
"#define STRINGH\n"
"#undef  STRINGSH\n"
"#define STDLIBH\n"
"#undef  STDDEFH\n"
"#undef  SYSSYSLOGH\n"
"#define NOINDEX\n"
"#define NOBCOPY\n"
"#define NEED_STRERROR\n"
"#define NEED_STRTOKEN\n"
"#undef  NEED_STRTOK\n"
"#undef  NEED_INET_ADDR\n"
"#undef  NEED_INET_NTOA\n"
"#define NEED_INET_NETOF\n"
"#define GETTIMEOFDAY\n"
"#undef  LRAND48\n"
"#define MALLOCH <malloc.h>\n"
"#undef  NBLOCK_POSIX\n"
"#undef  POSIX_SIGNALS\n"
"#undef  TIMES_2\n"
"#undef  GETRUSAGE_2\n"
"\n"
"#define NO_U_TYPES\n"
"#define NEED_U_INT32_T\n"
"#endif\n";



int     main(int argc, char *argv[])
{
	if (argc>1)
	    {
		if (!strcmp(argv[1], "-v"))
			return do_version();

		if (!strcmp(argv[1], "-n"))
			return do_config(1);
	    }

	return do_config(0);
}


int     do_config(int autoconf)
{
	int     fd;
	char    str[128];


	if ((fd = open("include\\setup.h", O_CREAT|O_TRUNC|O_WRONLY|O_TEXT,
		       S_IREAD|S_IWRITE)) == -1)
		printf("Error opening include\\setup.h\n\r");
	else
	    {
		write(fd, SetupH, strlen(SetupH));
		close(fd);
	    }

	while (1)
	    {
		/*
		 * FD_SETSIZE
		 */
		printf("\n");
		printf("How many file descriptors (or sockets) can the irc server use?");
		printf("\n");
		printf("[%d] -> ", _FD_SETSIZE);
		gets(str);
		if (*str != '\n' && *str != '\r')
			sscanf(str, "%d", &_FD_SETSIZE);

		if (_FD_SETSIZE >= 100)
		    {
			printf("\n");
			printf("FD_SETSIZE will be overridden using -D "
			       "FD_SETSIZE=%d when compiling ircd.", _FD_SETSIZE);
			printf("\n");
			break;
		    }
		printf("\n");
		printf("You need to enter a number here, greater or equal to 100.\n");
	    }

	while (1)
	    {
		/*
		 * NS_ADDRESS
		 */
		printf("\n");
		printf("What is the contact address for connect problems due to the\n\r");
		printf("anti-spoof system, shown to the user when they connect?\n\r");
		printf("For SorceryNet, this is nospoof@sorcery.net.  If your server is not\n\r");
		printf("part of SorceryNet, please choose some other email address.\n\r");
		printf("\n");
		printf("[] -> ");
		gets(str);
		if (*str != '\n' && *str != '\r')
		    {
			sscanf(str, "%s", _NS_ADDRESS);
			break;
		    }

		printf("\n");
		printf("You must enter a valid e-mail address.\n");
	    }

	while (1)
	    {
		/*
		 * KLINE_ADDRESS
		 */
		printf("\n");
		printf("What is the contact address for connect problems due to the\n\r");
		printf("user being K:lined, shown to the user when they attempt to\n\r");
		printf("connect?  This should be a valid email address.\n\r");
		printf("\n\r");
		printf("For SorceryNet servers, note that this message is displayed when\n\r");
		printf("the user is affected by a local K:line or k:line.  With\n\r");
		printf("Services-based autokills, the message is set up automatically\n\r");
		printf("by Services to ask the user to email kline@sorcery.net.  It is\n\r");
		printf("recommended that you set this up to give a valid email address\n\r");
		printf("for the server's admin, not kline@sorcery.net.\n\r");
		printf("\n");
		printf("[] -> ");
		gets(str);
		if (*str != '\n' && *str != '\r')
		    {
			sscanf(str, "%s", _KLINE_ADDRESS);
			if (stricmp(_KLINE_ADDRESS, "kline@sorcery.net"))
				break;
			printf("Please note that kline@sorcery.net does not handle server-specific\n\r");
			printf("K:lines.\n\r");
			printf("Are you sure you want to set the address to kline@sorcery.net?\n\r");
			printf("[No] -> ");
			gets(str);
			if (*str == 'y' || *str == 'Y')
				break;
		    }

		printf("\n");
		printf("You must enter a valid e-mail address.\n");
	    }

	/*
	 * Now write the makefile out.
	 */
	write_makefile();

	return 0;
}


int     write_makefile(void)
{
	int     fd, makfd, len;
	char    *buffer, *s;

	buffer = (char *)malloc(strlen(Makefile)+4096);
	memcpy(buffer, Makefile, strlen(Makefile)+1);

	s = (char *)strstr(buffer, "$FD_SETSIZE");
	if (s)
	    {
		itoa(_FD_SETSIZE, s, 10);
		memmove(s+strlen(s), s+11, strlen(s+11)+1);
	    }

	s = (char *)strstr(buffer, "$NS_ADDRESS");
	if (s)
	    {
		memmove(s+strlen(_NS_ADDRESS), s+11, strlen(s+11)+1);
		memmove(s, _NS_ADDRESS, strlen(_NS_ADDRESS));
	    }

	s = (char *)strstr(buffer, "$KLINE_ADDRESS");
	if (s)
	    {
		memmove(s+strlen(_KLINE_ADDRESS), s+14, strlen(s+14)+1);
		memmove(s, _KLINE_ADDRESS, strlen(_KLINE_ADDRESS));
	    }

	if ((makfd = open("Makefile", O_CREAT|O_TRUNC|O_WRONLY|O_TEXT,
		       S_IREAD|S_IWRITE)) == -1)
	    {
		printf("Error creating Makefile\n\r");
		return 1;
	    }
	write(makfd, buffer, strlen(buffer));
	close(makfd);

	free(buffer);
	return 0;
}


int     do_version(void)
{
	int     fd, verfd, generation=0, len, doingvernow=0;
	char    buffer[16384], *s;

	if ((verfd = open("src\\version.c", O_RDONLY | O_TEXT)) != -1)
	    {
		while (!eof(verfd))
		    {
			len = read(verfd, buffer, sizeof(buffer)-1);
			if (len == -1)
				break;
			buffer[len] = 0;
			s = (char *)strstr(buffer, "char *generation = \"");
			if (s)
			    {
				s += 20;
				generation = atoi(s);
				break;
			    }
		    }

		    close(verfd);
	    }

	if ((fd = open("src\\version.c.SH", O_RDONLY | O_TEXT)) == -1)
	    {
		printf("Error opening src\\version.c.SH\n\r");
		return 1;
	    }

	if ((verfd = open("src\\version.c", O_CREAT|O_TRUNC|O_WRONLY|O_TEXT,
		       S_IREAD|S_IWRITE)) == -1)
	    {
		printf("Error opening src\\version.c\n\r");
		return 1;
	    }

	generation++;

	printf("Extracting IRC/ircd/version.c...\n\r");

	while (!eof(fd))
	    {
		len = read(fd, buffer, sizeof(buffer)-1);
		if (len == -1)
			break;
		buffer[len] = 0;
		if (!doingvernow)
		    {
			s = (char *)strstr(buffer, "/*");
			if (!s)
				continue;
			memmove(buffer, s, strlen(s)+1);
			doingvernow=1;
		    }
		s = (char *)strstr(buffer, "$generation");
		if (s)
		    {
			itoa(generation, s, 10);
			memmove(s+strlen(s), s+11, strlen(s+11)+1);
		    }
		s = (char *)strstr(buffer, "$creation");
		if (s)
		    {
			time_t  t = time(0);
			char    *ct = ctime(&t);

			memmove(s+strlen(ct)-1, s+9, strlen(s+9)+1);
			memmove(s, ct, strlen(ct)-1);
		    }
		s = (char *)strstr(buffer, "$package");
		if (s)
		    {
			memmove(s, "IRC", 3);
			memmove(s+3, s+8, strlen(s+8)+1);
		    }

		s = (char *)strstr(buffer, "!SUB!THIS!");
		if (s)
			*s = 0;

		write(verfd, buffer, strlen(buffer));
	    }

	close(fd);
	close(verfd);
	return 0;
}


