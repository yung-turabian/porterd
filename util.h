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

bool promptYesOrNo(const char *question) {
		char response[4];

		while(1) {
				fprintf(stdout, "%s (Y/n): ", question);
				if(fgets(response, sizeof(response), stdin) == NULL)
						continue;
				
				response[strcspn(response, "\n")] = 0;

				if(toupper(response[0]) == 'Y' && response[1] == '\0') {
						return true;
				} else if(toupper(response[0]) == 'N' && response[1] == '\0') {
								return false;
				} else {
						fprintf(stderr, "Invalid input.\n");
				}
		}
}


int fmove(const char *oldpath, char *newpath)
{
  char new_name[100];
  int copy_num = 0;
  
  if(fexists(newpath) == 0) {
    strcpy(new_name, newpath);
    while(fexists(new_name) == 0) {
		  strcpy(new_name, newpath);
      sprintf(new_name + strlen(new_name), " (%d)", ++copy_num);
    }

      if(copy_num != 0) {
      	fprintf(stdout, "Renaming file: '%s' -> '%s'\n", oldpath, new_name);
				strcpy(newpath, new_name);
      }
  }
  
  if(rename(oldpath, newpath) == 0) {
    return 0;
	} else {
    perror("fmove() error on attempts to rename");
    return -1;
  }
}

int fexists(const char *file)
{
  if(access(file, F_OK) == 0) {
    return 0;
  } else {
    return -1;
  }
}

#endif
