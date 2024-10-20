#ifndef UTIL_H
#define UTIL_H

int isNum(const char *str) {
		while(*str) {
				if(!isdigit(*str))
						return 0;
				str++;
		}
		return 1;
}

#endif
