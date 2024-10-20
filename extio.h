#ifndef EXTIO_H
#define EXTIO_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__APPLE__) || defined(__unix__)
#include <unistd.h>
#endif


int fmove(const char *oldpath, char *newpath);

int fexists(const char *file);

// 0 on yes, 1 on no and -1 if something went really wrong.
int promptYesOrNo(const char *question);



#endif
