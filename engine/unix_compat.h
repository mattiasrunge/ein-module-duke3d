/*
 * unix_compat.h -- what the BUILD engine and the game expect of a DOS or
 * Windows compiler, on a POSIX one such as Emscripten's. It takes the
 * place of Chocolate Duke3D's own.
 */
#ifndef EIN_UNIX_COMPAT_H
#define EIN_UNIX_COMPAT_H

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef max
#define max(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define O_BINARY 0
#define S_IREAD S_IRUSR

#define stricmp strcasecmp
#define strcmpi strcasecmp

#endif
