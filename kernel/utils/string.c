#include "../def.h"

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
int
sprintf(char *dst, const char *fmt, ...)
{
  // 简单实现，仅支持 %s 和 %d
  const char *s;
  char *d = dst;
  int i, c;
  uint64 *ap;
  
  ap = (uint64*)(&fmt + 1);
  
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      *d++ = c;
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      // 简单的整数转换
      {
        int val = (int)*ap++;
        if(val < 0){
          *d++ = '-';
          val = -val;
        }
        char buf[20];
        int j = 0;
        if(val == 0){
          buf[j++] = '0';
        } else {
          while(val > 0){
            buf[j++] = '0' + (val % 10);
            val /= 10;
          }
        }
        while(j > 0){
          *d++ = buf[--j];
        }
      }
      break;
    case 's':
      s = (char*)*ap++;
      if(s == 0)
        s = "(null)";
      while(*s)
        *d++ = *s++;
      break;
    case '%':
      *d++ = '%';
      break;
    default:
      *d++ = '%';
      *d++ = c;
      break;
    }
  }
  *d = 0;
  
  return d - dst;
}