#ifndef BSD_COMPAT_H
#define BSD_COMPAT_H

extern int cgetmatch(const char *buf, char *name);
extern int cgetent(char **buf, char **db_array, const char *name);
extern char *cgetcap(char *buf, const char *cap, int type);
extern int cgetustr(char *buf, const char *cap, char **str);

#endif /* !BSD_COMPAT_H */
