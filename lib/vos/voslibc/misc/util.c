

extern inline void
reverse(char s[])
{
  int c, i, j;

  for(i=0, j=strlen(s)-1; i<j; i++, j--)
  {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

void
uitoa(int n, char s[])
{
  int i;

  i = 0;
  do {
    s[i++] = n % 10 + '0';
  } while ((n /= 10) > 0);

  s[i] = '\0';
  reverse(s);
}

