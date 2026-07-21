/*
 * port/user/init.c — the M8 user program: a real compiled C program
 * (crt0 + ulibc, ring 3) that exercises the kernel's syscall surface —
 * getpid, open/read/close on a memfs file, fstat, write — instead of
 * M5/M7's single hard-coded write.  This is the userland analog of M3:
 * the first genuinely capable process, built from ordinary C.
 */

#include "ulibc.h"

int
main(void)
{
	struct pstat st;
	char buf[128];
	int fd, n;

	printf("init: hello from ring 3, pid=%d\n", getpid());

	fd = open("/motd", 0);
	if (fd < 0) {
		printf("init: open(/motd) failed\n");
		return 1;
	}

	if (fstat(fd, &st) == 0)
		printf("init: /motd is %d bytes (ino %u)\n",
		    (int)st.st_size, st.st_ino);

	printf("init: ---- /motd ----\n");
	while ((n = read(fd, buf, sizeof(buf))) > 0)
		write(1, buf, n);
	printf("init: ---- end ----\n");

	close(fd);
	printf("init: syscalls (getpid/open/fstat/read/write/close) ok\n");
	return 0;
}
