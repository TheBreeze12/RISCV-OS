#include "../include/def.h"

void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

char*
strcpy(char *dst, const char *src)
{
  char *d = dst;
  while((*d++ = *src++) != 0)
    ;
  return dst;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (unsigned char)*p - (unsigned char)*q;
}

int
strlen(const char *s)
{
  int n;
  for(n = 0; s[n]; n++)
    ;
  return n;
}

// 安全字符串复制，最多复制 n-1 个字符，确保以 null 结尾
char*
safestrcpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  if(n <= 0)
    return os;
  while(--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

void*
memmove(void *dst, const void *src, uint n)
{
  const char *s;
  char *d;

  if(n == 0)
    return dst;
  
  s = src;
  d = dst;
  if(s < d && s + n > d){
    s += n;
    d += n;
    while(n-- > 0)
      *--d = *--s;
  } else
    while(n-- > 0)
      *d++ = *s++;

  return dst;
}
