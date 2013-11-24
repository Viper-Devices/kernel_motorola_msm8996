#ifndef __NOUVEAU_OPTION_H__
#define __NOUVEAU_OPTION_H__

#include <core/os.h>

const char *nouveau_stropt(const char *optstr, const char *opt, int *len);
bool nouveau_boolopt(const char *optstr, const char *opt, bool value);

int nouveau_dbgopt(const char *optstr, const char *sub);

/* compares unterminated string 'str' with zero-terminated string 'cmp' */
static inline int
strncasecmpz(const char *str, const char *cmp, size_t len)
{
	if (strlen(cmp) != len)
		return len;
	return strncasecmp(str, cmp, len);
}

#endif
