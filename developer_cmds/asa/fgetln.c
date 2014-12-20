/* Written by Rich Felker: http://www.openwall.com/lists/musl/2012/08/12/1 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#ifdef THREAD_SAFE
#include <pthread.h>
#endif
#include <stdlib.h>

struct entry
{
	struct entry *next;
	int fd;
	FILE *f;
	char *buf;
};

struct entry *table[64];

#define K (sizeof table / sizeof table[0])

#ifdef THREAD_SAFE
pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
#endif

char *fgetln(FILE *f, size_t *l)
{
	int fd, h;
	struct entry *p;
	ssize_t cnt;
	char *ret = 0;

	flockfile(f);
	fd = fileno(f);
	h = fd % K;

#ifdef THREAD_SAFE
	pthread_rwlock_rdlock(&lock);
#endif
	for (p=table[h]; p && (p->fd!=fd || (fd<0 && p->f!=f)); p=p->next);
	if (!p) {
		if (!(p = calloc(sizeof *p, 1))) {
#ifdef THREAD_SAFE
			pthread_rwlock_unlock(&lock);
#endif
			funlockfile(f);
			return 0;
		}
		p->fd = fd;
		p->f = f;
#ifdef THREAD_SAFE
		pthread_rwlock_unlock(&lock);
		pthread_rwlock_wrlock(&lock);
#endif
		p->next = table[h];
		table[h] = p;
	}
#ifdef THREAD_SAFE
	pthread_rwlock_unlock(&lock);
#endif
	cnt = getline(&p->buf, (size_t[]){0}, f);
	if (cnt >= 0) {
		*l = cnt;
		ret = p->buf;
	}
	funlockfile(f);
	return ret;
}
