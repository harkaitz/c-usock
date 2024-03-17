#define _POSIX_C_SOURCE 200809L
#include "usock.h"

int
main(int _argc, char *_argv[])
{
	FILE	*fp;
	char	 buffer[512] = {0};
	
	fp = usock_cli_new_unix("/tmp/usock_test_srv");
	if (!fp/*err*/) {
		return 1;
	}
	for (int i=1; i<_argc; i++) {
		fputs(_argv[i], fp);
		fputs("\n", fp);
	}
	fclose_write(fp);
	
	while (fgets(buffer, sizeof(buffer)-1, fp)) {
		fputs(buffer, stdout);
	}
	fclose(fp);
	
	return 0;
}
