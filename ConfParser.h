#ifndef CONF_PARSER_H
#define CONF_PARSER_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 256

char *trimWhitespace(char *str) {
		char *end;

		while(isspace((unsigned char)*str)) str++;

		if(*str == 0)
				return str;


		end = str + strlen(str) - 1;
		while(end > str && isspace((unsigned char)*end)) end--;

		*(end + 1) = '\0';

		return str;
}

void parseLine(char *line) {
		if(line[0] == '\0' || line[0] == '#') return;

		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");

		if(key && value) {
				key = trimWhitespace(key);
				value = trimWhitespace(value);
#ifdef DEBUG
				fprintf(stdout, "Key: %s, Value %s\n", key, value);
#endif
		}
}

int parseConfFile(const char *filename) {
		FILE *fptr = fopen(filename, "r");
		if(!fptr) {
				perror("error opening file");
				return -1;
		}

		char line[MAX_LINE_LENGTH];

		while(fgets(line, sizeof(line), fptr)) {
				line[strcspn(line, "\n")] = '\0';
				parseLine(line);
		}

		fclose(fptr);

		return 0;
}


#endif
