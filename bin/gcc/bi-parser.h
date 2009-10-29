typedef union
{
  char *string;
  struct def *def;
  struct variation *variation;
  struct node *node;
} YYSTYPE;
#define	DEFOP	257
#define	STRING	258


extern YYSTYPE yylval;
