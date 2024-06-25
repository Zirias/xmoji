#define _POSIX_C_SOURCE 200112L

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static const char hex[16] = "0123456789abcdef";

int main(int argc, char **argv)
{
    int rc = 1;
    if (argc != 3) return rc;

    int outfd = -1;
    int infd = -1;
    unsigned char *data = 0;
    char *datastr = 0;

    outfd = open(argv[1], O_WRONLY|O_CREAT|O_TRUNC, 0664);
    if (outfd < 0) goto done;
    infd = open(argv[2], O_RDONLY);
    if (infd < 0) goto done;
    struct stat st;
    if (fstat(infd, &st) < 0) goto done;

    size_t datasz = st.st_size;
    data = malloc(datasz);
    if (!data) goto done;
    size_t strsz = 4 * st.st_size + 3;
    datastr = malloc(strsz);
    if (!datastr) goto done;

    size_t rdpos = 0;
    ssize_t rdchunk = 0;
    while ((rdchunk = read(infd, data+rdpos, datasz-rdpos)) > 0)
    {
	if ((rdpos += rdchunk) == datasz) break;
    }
    if (rdpos < datasz) goto done;

    char *op = datastr;
    *op++ = '"';
    for (size_t i = 0; i < datasz; ++i)
    {
	*op++ = '\\';
	*op++ = 'x';
	*op++ = hex[data[i]>>4];
	*op++ = hex[data[i]&15];
    }
    *op++ = '"';
    *op = '\n';

    size_t wrpos = 0;
    ssize_t wrchunk = 0;
    while ((wrchunk = write(outfd, datastr+wrpos, strsz-wrpos)) > 0)
    {
	if ((wrpos += wrchunk) == strsz) break;
    }
    if (wrpos == strsz) rc = 0;

done:
    free(datastr);
    free(data);
    if (infd >= 0) close(infd);
    if (outfd >= 0) close(outfd);
    return rc;
}
