
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


/*
 * Generate the system calls from syscall.conf
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "linebuf.h"

/* From xok/trap.h */
#define MAX_SYSCALL 0x100

/* From xok/batch.h */
#define SYSBATCH_NARGS 12

#define MAX_ARGS 0x10
#define NDA 4                /* Number of direct arguments */

char *progname;

static int nerrs;
static Linebuf *lb;

const char *infile = "syscall.conf";

void fatal (const char *, ...)
     __attribute__ ((noreturn, format (printf, 1, 2)));
void syntax (const char *, ...) __attribute__ ((format (printf, 1, 2)));

void
fatal (const char *msg, ...)
{
  va_list ap;

  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  exit (1);
}

void
syntax (const char *msg, ...)
{
  va_list ap;

  fprintf (stderr, "%s:%d: ", infile, lb->lineno);
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  nerrs++;
}

static inline void
parsein (void (*pf) (int, char *, int, char **))
{
  char *l;
  unsigned char used_vec[MAX_SYSCALL>>3];

  bzero (used_vec, sizeof (used_vec));
  nerrs = 0;

  while (l = getline (lb)) {
    char *cp, *ip;
    unsigned int sn;
    char *args[MAX_ARGS+1];
    int nargs;
    int an;

    if (cp = strchr (l, '#'))      /* get rid of comments */
      *cp = '\0';

    /* Get syscall number */
    cp = strtok (l, " \t");
    if (!cp || !*cp)               /* skip blank/comment lines */
      continue;
    sn = strtol (cp, &cp, 0);
    if (cp == l || *cp) {          /* no number or garbage left over */
      syntax ("invalid syscall number `%s'\n", l);
      continue;
    }
    if (sn >= MAX_SYSCALL) {
      syntax ("syscall number 0x%x is out of range\n", sn);
      continue;
    }
    if (used_vec[(sn>>3)] & (1<<(sn & 0x7))) {
      syntax ("can't redefine syscall 0x%x\n", sn);
      continue;
    }
    used_vec[(sn>>3)] |= (1<<(sn & 0x7));

    cp = strtok (NULL, " \t");
    if (!cp || !*cp) {
      syntax ("missing syscall name for syscall 0x%x\n", sn);
      continue;
    }
    ip = cp;
    if (!isalpha (*ip) && *ip != '_') {
      syntax ("syscall 0x%x has invalid name `%s'\n", sn, cp);
      continue;
    }
    while (*++ip)
      if (!isalnum (*ip) && *ip != '_') {
	syntax ("syscall 0x%x has invalid name `%s'\n", sn, cp);
	goto again;
      }

    for (nargs = 0; args[nargs] = strtok (NULL, ","); nargs++) {
      if (nargs > MAX_ARGS) {
	syntax ("maximum argument count %d exceeded\n", MAX_ARGS);
	goto again;
      }
      while (isspace (*args[nargs]))
	args[nargs]++;
      if (! *args[nargs]) {
	syntax ("syscall 0x%x, argument %d bad type\n", sn, nargs);
	goto again;
      }
      ip = args[nargs] + strlen (args[nargs]) - 1;
      while (isspace (*ip))
	*ip-- = '\0';
    }
    if (nargs < 2) {
      syntax ("syscall 0x%x needs return and/or argument types (or void)\n",
	      sn);
      continue;
    }
    if (args[2] || strcmp (args[1], "void")) {
      for (an = 1; args[an]; an++)
	if (!strcmp (args[an], "void")) {
	  syntax ("illegal void type for argument");
	  goto again;
	}
    }
    else
      nargs = 1;
    (*pf) (sn, cp, nargs, args);
  again:
  }
}

static void
batch_dispatch (int sn, char *name, int nargs, char **args) {
  int an;

  printf ("\t\tcase SYS_%s: ", name);
  if (strcmp (args[0], "void"))
    printf ("ret = (int )");
  printf ("sys_%s (SYS_%s", name, name);
  if (strcmp (args[1], "void")) {
    for (an = 1; args[an]; an++) {
      printf (", (%s )sb[i].sb_syscall.args[%d]", args[an], an-1);
    }
  }
  printf ("); break;\n");
}
  
static void
ubatch (int sn, char *name, int nargs, char **args) {
  int an;

  /* generate the function declaration */

  printf ("\nstatic inline int sys_batch_%s (", name);
  printf ("struct Sysbatch sb[]");
  for (an = 1; args[an]; an++) {
    if (!strcmp (args[an], "void")) {
      if (an != 1) {
	syntax ("illegal void type for argument");
      }
    } else {
      printf (", ");
      printf ("%s%sa%d", args[an],
	      (args[an][strlen (args[an]) - 1] == '*') ? "" : " ", an);
    }
  }
  printf (") {\n");

  /* generate code to update the header of the batched array */

  printf ("\tint i;\n\n");
  printf ("\ti = sb[0].sb_header.next_free++;\n");
  printf ("\tif (i >= sb[0].sb_header.max_syscalls) return -1;\n");
  if (nargs > SYSBATCH_NARGS)
    syntax ("too many arguments to batched system call");

  /* finally generate code to queue the arguments */

  printf ("\tsb[i].sb_syscall.syscall = SYS_%s;\n", name);
  if (strcmp (args[1], "void")) {
    for (an = 1; args[an]; an++) {
      printf ("\tsb[i].sb_syscall.args[%d] = (u_int )a%d;\n", an-1, an);
    }
  }
  printf ("\treturn 0;\n");
  printf ("}\n");
}

static void
ustub (int sn, char *name, int nargs, char **args)
{
  int i, an = 1, op;
  int has_ret = strcmp (args[0], "void");

  printf ("\nstatic inline %s\nsys_%s (", args[0], name);
  if (args[2] || strcmp (args[1], "void"))
    for (an = 1; args[an]; an++) {
      if (!strcmp (args[an], "void"))
	syntax ("illegal void type for argument");
      if (an > 1)
	printf (", ");
      printf ("%s%sa%d", args[an],
	      (args[an][strlen (args[an]) - 1] == '*') ? "" : " ",
	      an);
    }
  else
    printf ("void");
  printf (")\n{\n");
  if (has_ret)
    printf ("  %s%sret;\n", args[0],
	    (args[0][strlen (args[0]) - 1] == '*') ? "" : " ");

  printf ("#ifdef KERN_CALL_COUNT\n");
  printf (" {\n");
  printf ("  extern unsigned int *__kern_call_counts;\n");
  printf ("  if (UAREA.u_start_kern_call_counting) "
	  "__kern_call_counts[SYS_%s]++;\n", name);
  printf (" }\n");
  printf ("#endif\n");

  op = an;
  printf ("  __asm __volatile (\"\\n\"\n");
  if (NDA < an) {
    for (i = NDA; i < an; i++) {
      printf ("                    \"\\tmovl %%%d,%%%%eax\\n\"\n"
	      "                    \"\\tpushl %%%%eax\\n\"", i-NDA);
      if (i != an-1)
	printf ("\n");
    }
    printf (" ::\n");
    for (i = NDA; i < an; i++) {
      printf ("                    \"mi\" (a%d)", (NDA - 1) + an - i);
      if (i != an-1)
	printf (",\n");
    }
    printf ("\n                    : \"eax\", \"memory\");\n");
    printf ("  __asm __volatile (\"\\n\"\n");
    op = NDA;
  }
  printf ("                    \"\\tmovl %%%d, %%%%eax\\n\"\n", op);
  printf ("                    \"\\tint %%%d\\n\"\n", op+1);
  if (an > NDA)
    printf ("                    \"\\taddl $%d,%%%%esp\\n\"\n",
	    (an - NDA) << 2);
  if (has_ret)
    printf ("                    : \"=a\" (ret) :\n");
  else
    printf ("                    :: \"i\" (0),\n");
  switch (an) {
  default:
    printf ("                    \"b\" (a3),\n");
  case 3:
    printf ("                    \"c\" (a2),\n");
  case 2:
    printf ("                    \"d\" (a1),\n");
  case 1:
  case 0:
  }
  printf ("                    \"i\" (SYS_%s), \"i\" (T_SYSCALL)\n", name);
  printf ("                    : \"eax\", \"edx\", \"ecx\", %s\"cc\","
	  " \"memory\");\n", nargs > 3 ? "\"ebx\", " : "");
  if (has_ret)
    printf ("  return (ret);\n");
  printf ("}\n");
}

static void
kernstr (int sn, char *name, int nargs, char **args)
{
  printf("  { %d, \"sys_%s\" } ,\n", sn, name);
}

static void
sproto (int sn, char *name, int nargs, char **args)
{
  int an;

  printf ("%s sys_%s (u_int", args[0], name);
  if (nargs > 1)
    for (an = 1; args[an]; an++)
      printf (", %s", args[an]);
  printf (") __attribute__ ((regparm (3)));\n");
}

#if 0 /* XXX - SYSCALL DISPATCH CODE NOT USED */
static void
sdisp (int sn, char *name, int nargs, char **args)
{
  int an;

  printf ("  case SYS_%s:\n", name);
  if (nargs > NDA)
    printf ("    {\n"
	    "      u_int *uap = trup ((u_int *) utf->tf_esp);\n");
  if (strcmp (args[0], "void"))
    printf ("%s    return ((u_int) sys_%s (a0", nargs > NDA ? "  " : "", name);
  else
    printf ("%s    sys_%s (a0", nargs > NDA ? "  " : "", name);
  if (nargs > 1)
    for (an = 1; an < nargs; an++) {
      printf (", (%s) ", args[an]);
      if (an < NDA)
	printf ("a%d", an);
      else
	printf ("uap[%d]", an - NDA);
    }
  if (strcmp (args[0], "void"))
    printf ("));\n");
  else
    printf (");\n%s    return (0);\n", nargs > NDA ? "  " : "");
  if (nargs > NDA)
    printf ("    }\n");
}
#endif

static void
hdefs (int sn, char *name, int nargs, char **args)
{
  printf ("#define SYS_%s 0x%x\n", name, sn);
}

static void
stab (int sn, char *name, int nargs, char **args)
{
  printf ("  [0x%x] = { (void *) sys_%s, %d },\n", sn, name, nargs - 1);
}

static void usage (void) __attribute__ ((noreturn));
static void
usage (void)
{
  fatal ("usage: %s {-p | -u | -s | -b | -d | -h | -t} [conffile]\n", progname);
}

int
main (int argc, char **argv)
{
  if (progname = strrchr (argv[0], '/'))
    progname++;
  else
    progname = argv[0];

  if (argc == 3)
    infile = argv[2];
  else if (argc > 3)
    usage ();

  lb = Linebuf_alloc (infile, fatal);

  if (!strcmp (argv[1], "-h")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#ifndef _XOK_SYSCALLNO_H_\n"
	    "#define _XOK_SYSCALLNO_H_\n\n");
    parsein (hdefs);
    printf ("\n#endif /* !_XOK_SYSCALLNO_H_ */\n");
  }
  else if (!strcmp (argv[1], "-p")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#ifndef _XOK_SYS_PROTO_H_\n"
	    "#define _XOK_SYS_PROTO_H_\n\n");
    printf ("#include <xok/syscall.h>\n\n");
    printf ("void sys_badcall (unsigned int);\n");
    parsein (sproto);
    printf ("\n#endif /* !_XOK_SYS_PROTO_H_ */\n");
  }
  else if (!strcmp (argv[1], "-u")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#ifndef _XOK_SYS_UCALL_H_\n"
	    "#define _XOK_SYS_UCALL_H_\n\n");
    printf ("#include <xok/syscall.h>\n");
    printf ("#include <exos/callcountdefs.h>\n");
    printf ("#ifdef KERN_CALL_COUNT\n");
    printf ("#include <xok/env.h>\n");
    printf ("#endif\n");
    parsein (ustub);
    printf ("\n#endif /* !_XOK_SYS_UCALL_H_ */\n");
  }
  else if (!strcmp (argv[1], "-s")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#include <xok/syscall.h>\n");
    printf ("#include <xok/types.h>\n");
    printf ("struct kerntableentry {\n"
	    "  int sn;\n"
	    "  char *name;\n"
	    "};\n"
	    "\n"
	    "static struct kerntableentry kerncalltable[] = {\n");
    parsein (kernstr);
    printf ("  { 0, 0 }\n"
	    "};\n"
	    "char *kerncallstr(u_int kerncall) {\n"
	    "  int i;\n"
	    "\n"
	    "  for (i=0; kerncalltable[i].name != 0; i++)\n"
	    "    if (kerncalltable[i].sn == kerncall)\n"
	    "      return kerncalltable[i].name;\n"
	    "  return 0;\n"
	    "}\n");
  }
  else if (!strcmp (argv[1], "-b")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#ifndef _XOK_SYS_UBATCH_H_\n");
    printf ("#define _XOK_SYS_UBATCH_H_\n\n");
    printf ("#include <xok/syscallno.h>\n");
    printf ("#include <xok/types.h>\n");
    printf ("#include <xok/syscall.h>\n");
    printf ("#include <xok/batch.h>\n\n");
    parsein (ubatch);
    printf ("\n#endif /* !_XOK_SYS_UBATCH_H_ */\n");
  }
  else if (!strcmp (argv[1], "-d")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#include <xok/sys_proto.h>\n"
	    "#include <xok/kerrno.h>\n"
	    "#include <xok/batch.h>\n\n"
	    "\nstatic inline u_int dispatch_syscall (struct Sysbatch sb[], int i) {\n"
	    "\tint ret = 0;\n\n"
	    "\tswitch (sb[i].sb_syscall.syscall) {\n");
    parsein (batch_dispatch);
    printf ("\t\tdefault: return -1;\n"
	    "\t}\n"
	    "\tsb[i].sb_syscall.retval = ret;\n"
	    "\tif (ret < 0) {\n"
	    "\t\tsb[0].sb_header.error = i;\n"
	    "\t\treturn -1;\n"
	    "\t}\n"
	    "\treturn 0;\n"
	    "}\n\n"
	    "int sys_batch (u_int sn, struct Sysbatch *sb) {\n"
	    "\tint i;\n\n"
	    "\tfor (i = 1; i < sb[0].sb_header.next_free; i++) {\n"
	    "\t\tif (dispatch_syscall (sb, i) == -1) return -E_INVAL;\n"
	    "\t}\n"
	    "\treturn 0;\n"
	    "}\n");
  }
  else if (!strcmp (argv[1], "-t")) {
    printf ("/* autogenerated from %s */\n\n"
	    "#include <xok/trap.h>\n"
	    "#include <xok/sys_proto.h>\n\n"
	    "unsigned int nsyscalls = 0;\n\n"
	    "void (*syscall_pfcleanup)(u_int, u_int, u_int);\n\n"
	    "struct Syscall sctab[MAX_SYSCALL] = {\n", infile);
    parsein (stab);
    printf ("};\n");
  }
#if 0 /* XXX - SYSCALL DISPATCH CODE NOT USED */
  else if (!strcmp (argv[1], "-s")) {
    printf ("/* autogenerated from %s */\n\n", infile);
    printf ("#include <xok/trap.h>\n"
	    "#include <xok/syscall.h>\n"
	    "#include <xok/sys_proto.h>\n\n");
    printf ("u_int syscall (u_int, u_int, u_int, u_int, u_int)"
	    " __attribute__ ((regparm (3)));\n\n");
    printf ("u_int\nsyscall (u_int a0, u_int a1, u_int a2,"
	    " u_int a3, u_int trapno)\n{\n");
    printf ("  switch (a0) {\n");
    parsein (sdisp);
    printf ("  default:\n    sys_badcall (a0);\n    return (-1);\n");
    printf ("  }\n}\n");
  }
#endif
  else
    usage ();

  if (nerrs)
    fatal ("%s: errors in %s\n", progname, infile);

  return (0);
}





