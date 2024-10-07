#ifndef PORTERD_H
#define PORTERD_H

struct UserPaths {
  char Home[30];
  char Downloads[50];
  char Music[50];
  char Documents[50];
};

int fexists(const char *file);

int fmove(const char *oldpath, char *newpath);

#endif
