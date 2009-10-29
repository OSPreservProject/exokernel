#define FIXME_01 257
#define FIXME_02 258
#define FIXME_03 259
#define FIXME_04 260
#define FIXME_05 261
#define FIXME_06 262
#define FIXME_07 263
#define FIXME_08 264
#define FIXME_09 265
#define FIXME_10 266
#define FIXME_11 267
#define FIXME_12 268
#define FIXME_13 269
#define FIXME_14 270
#define FIXME_15 271
#define FIXME_16 272
#define FIXME_17 273
#define FIXME_18 274
#define FIXME_19 275
#define FIXME_20 276
#define FIXME_21 277
#define FIXME_22 278
#define FIXME_24 279
#define FIXME_25 280
#define FIXME_26 281
#define FIXME_27 282
#define FIXME_28 283
#define FIXME_29 284
#define FIXME_30 285
#define INTEGER_LITERAL 286
#define BOOLEAN_LITERAL 287
#define CHARACTER_LITERAL 288
#define FLOAT_LITERAL 289
#define GENERAL_PROCEDURE_NAME 290
#define LOCATION_NAME 291
#define SET_LITERAL 292
#define EMPTINESS_LITERAL 293
#define CHARACTER_STRING_LITERAL 294
#define BIT_STRING_LITERAL 295
#define TYPENAME 296
#define FIELD_NAME 297
#define CASE 298
#define OF 299
#define ESAC 300
#define LOGIOR 301
#define ORIF 302
#define LOGXOR 303
#define LOGAND 304
#define ANDIF 305
#define NOTEQUAL 306
#define GTR 307
#define LEQ 308
#define IN 309
#define SLASH_SLASH 310
#define MOD 311
#define REM 312
#define NOT 313
#define POINTER 314
#define RECEIVE 315
#define UP 316
#define IF 317
#define THEN 318
#define ELSE 319
#define FI 320
#define ELSIF 321
#define ILLEGAL_TOKEN 322
#define NUM 323
#define PRED 324
#define SUCC 325
#define ABS 326
#define CARD 327
#define MAX_TOKEN 328
#define MIN_TOKEN 329
#define SIZE 330
#define UPPER 331
#define LOWER 332
#define LENGTH 333
#define GDB_REGNAME 334
#define GDB_LAST 335
#define GDB_VARIABLE 336
#define GDB_ASSIGNMENT 337
typedef union
  {
    LONGEST lval;
    unsigned LONGEST ulval;
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
extern YYSTYPE chill_lval;
