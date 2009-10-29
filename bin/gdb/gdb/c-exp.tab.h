#define INT 257
#define FLOAT 258
#define STRING 259
#define NAME 260
#define TYPENAME 261
#define NAME_OR_INT 262
#define STRUCT 263
#define CLASS 264
#define UNION 265
#define ENUM 266
#define SIZEOF 267
#define UNSIGNED 268
#define COLONCOLON 269
#define TEMPLATE 270
#define ERROR 271
#define SIGNED_KEYWORD 272
#define LONG 273
#define SHORT 274
#define INT_KEYWORD 275
#define CONST_KEYWORD 276
#define VOLATILE_KEYWORD 277
#define LAST 278
#define REGNAME 279
#define VARIABLE 280
#define ASSIGN_MODIFY 281
#define THIS 282
#define ABOVE_COMMA 283
#define OROR 284
#define ANDAND 285
#define EQUAL 286
#define NOTEQUAL 287
#define LEQ 288
#define GEQ 289
#define LSH 290
#define RSH 291
#define UNARY 292
#define INCREMENT 293
#define DECREMENT 294
#define ARROW 295
#define BLOCKNAME 296
typedef union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    double dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  } YYSTYPE;
extern YYSTYPE c_lval;
