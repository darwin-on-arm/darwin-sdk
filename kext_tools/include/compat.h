/*
 * gcc <-> clang compat glue
 */

#ifndef _COMPAT_H_
#define _COMPAT_H_

#ifndef __private_extern__
#define __private_extern__ __attribute__((visibility("hidden")))
#endif /* !__private_extern__ */

#endif /* !_COMPAT_H_ */
