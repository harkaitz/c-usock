#define _POSIX_C_SOURCE 200809L
#include "usock.h"

static void
service(FILE *_fp, void *_udata)
{
	char	 b[1024] = {0};
	int	 bsz;
	bsz = fread(b, 1, sizeof(b), _fp);
	fwrite(b, 1, bsz, _fp);
	fclose(_fp);
}

int
main(int _argc, char *_argv[])
{
	struct	 usock_srv_s *o;
	o = usock_srv_new_unix("/tmp/usock_test_srv", service, NULL);
	usock_srv_wait(o);
	return 0;
}
