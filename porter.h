#ifndef PORTER_H
#define PORTER_H

#include <getopt.h>
#include <stdint.h>

typedef struct {
  char Home[30];
  char Downloads[50];
  char Music[50];
  char Documents[50];
} UserPaths;

UserPaths *paths;

int fexists(const char *file);

int fmove(const char *oldpath, char *newpath);


void printHelp() {
		printf("Usage: porter [OPTIONS]...\n");
		printf("  -h, --help          Display this help message.\n");
		printf("  -s, --single        Run a single loop of delivieries.\n");
		printf("  -d, --daemon        Run the program in daemon mode.\n");
		printf("  -b, --block         Run the program in blocking mode.\n");
		printf("  -l, --logs          Use less to view logs of the daemon.\n");
}

#endif
