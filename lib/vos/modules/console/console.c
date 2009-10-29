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
/* $NetBSD: display.h,v 1.4 1994/10/27 04:16:36 cgd Exp $  */
/* 
 * netbsd: arch/i386/isa/pccons.c 
 */

/*
 * Copyright MIT 1999
 */


/* for user level console access... emulates pc3... you can also just use
 * kprintf to print to console... kprintf is logged (via sys_cputs) so
 * cat_console would work as well.
 */

#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>
#include <xok/console.h>

#include <vos/proc.h>
#include <vos/vm-layout.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/errno.h>
#include <vos/wk.h>
#include <vos/fd.h>
#include <vos/assert.h>


 /* color attributes for foreground text */
#define FG_BLACK                   0
#define FG_BLUE                    1
#define FG_GREEN                   2
#define FG_CYAN                    3
#define FG_RED                     4
#define FG_MAGENTA                 5
#define FG_BROWN                   6
#define FG_LIGHTGREY               7
#define FG_DARKGREY                8
#define FG_LIGHTBLUE               9
#define FG_LIGHTGREEN             10
#define FG_LIGHTCYAN              11
#define FG_LIGHTRED               12
#define FG_LIGHTMAGENTA           13
#define FG_YELLOW                 14
#define FG_WHITE                  15
#define FG_BLINK                0x80
#define FG_MASK                 0x8f

/* color attributes for text background */
#define BG_BLACK                0x00
#define BG_BLUE                 0x10
#define BG_GREEN                0x20
#define BG_CYAN                 0x30
#define BG_RED                  0x40
#define BG_MAGENTA              0x50
#define BG_BROWN                0x60
#define BG_LIGHTGREY            0x70
#define BG_MASK                 0x70

/* monochrome attributes for foreground text */
#define FG_UNDERLINE            0x01
#define FG_INTENSE              0x08

/* monochrome attributes for text background */
#define BG_INTENSE              0x10


void fillw (short, void *, size_t);	/* implemented in fillw.S */
static void sput (const u_char * cp, int n);	/* end of this file */

static u_short *Crtat;		/* pointer to backing store */
static u_short *crtat = (u_short *) 0;	/* pointer to current char */


static struct video_state
{
    int cx, cy;			/* escape parameters */
    int row, col;		/* current cursor position */
    int nrow, ncol, nchr;	/* current screen geometry */
    int offset;			/* Saved cursor pos */
    u_char state;		/* parser state */
#define	VSS_ESCAPE	1
#define	VSS_EBRACE	2
#define	VSS_EPARAM	3
    char so;			/* in standout mode? */
    char color;			/* color or mono display */
    char at;			/* normal attributes */
    char so_at;			/* standout attributes */
}
vs;

#define COL CRT_COLS
#define ROW CRT_ROWS
#define	CHR 2


/*
 * initialize console
 */
static void
pcninit ()
{
  struct Crtctl c;
  unsigned cursorat = 0;
  u_short volatile *cp;
  int ret;

  ret = vm_self_insert_pte (0, (__sysinfo.si_crt_pg << PGSHIFT)
			    | PG_SHARED | PG_U | PG_W | PG_P,
			    CONSOLE_REGION, 0, NULL);
  assert(ret >= 0);

  StaticAssert (CONSOLE_REGION_SZ >= NBPG);

  cp = (u_short volatile *) CONSOLE_REGION;

  if (!sys_crtctl (&c, CRT_GETADDR))
    {
      if (c.addr == MONO_BASE)
	vs.color = 0;
      else
	vs.color = 1;
    }
  else
    assert(0);

  if (!sys_crtctl (&c, CRT_GETPOS))
    {
      cursorat = c.pos;
    }
  else
    assert(0);

  Crtat = (u_short *) cp;
  crtat = (u_short *) (cp + cursorat);

  vs.ncol = COL;
  vs.nrow = ROW;
  vs.nchr = COL * ROW;
  vs.at = FG_LIGHTGREY | BG_BLACK;

  if (vs.color == 0)
    vs.so_at = FG_BLACK | BG_LIGHTGREY;
  else
    vs.so_at = FG_YELLOW | BG_BLACK;
}


/*
 * read buffer from console
 */
static int
console_read (S_T(fd_entry) *fd, void *buffer, int nbyte)
{
  int ret;

  if (FD_ISNB(fd) && (__sysinfo.si_kbd_nchars == 0)) 
    RETERR(V_WOULDBLOCK);

  /* blocking, spin until there are chars */
  if (__sysinfo.si_kbd_nchars == 0) 
  {
    int sz;
    struct wk_term t[3];
    sz = 0;

    sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_kbd_nchars), CAP_CONSOLE);
    sz = wk_mkimm (sz, t, 0);
    sz = wk_mkop (sz, t, WK_GT);

    ret = wk_waitfor_timeout(t,sz,0,-1);
    
    if (ret != 0) 
      RETERR(ret);
  }

  ret = sys_cgets(buffer,nbyte);
  return ret;
}


/*
 * write buffer to console
 */
static int
console_write (S_T(fd_entry) *fd, const void *buffer, int nbyte)
{
  if (nbyte < 0)
    RETERR(V_INVALID);

  if (nbyte == 0)
    return 0;

  sput (buffer, nbyte);
  return nbyte;
}


/*
 * close fd
 */
static int
console_close (S_T(fd_entry) *fd)
{
  return 0;
}

static int
console_close0 (S_T(fd_entry) *fd)
{
  return 0;
}


/*
 * open fd
 */
static int
console_open (S_T(fd_entry) *fd, const char *name, int flags, mode_t mode)
{
  return fd->fd;
}


/*
 * verify state
 */
static int
console_verify(S_T(fd_entry) *fd)
{
  return 0;
}


/*
 * duplicate fd and state
 */
static int
console_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  return 0;
}


static fd_op_t const console_ops = {
  console_verify,
  console_incref,
  console_open,
  console_read,
  console_write,
  NULL, /* readv */
  NULL, /* writev */
  NULL, /* lseek */
  NULL, /* select */
  NULL, /* select_pred */
  NULL, /* ioctl */
  NULL, /* fcntl */
  NULL, /* flock */
  console_close0,
  console_close,
  NULL, /* dup */
  NULL, /* fstat */
  NULL, /* socket */
  NULL, /* bind */
  NULL, /* connect */
  NULL, /* accept */
  NULL, /* listen */
  NULL, /* sendto */
  NULL  /* recvfrom */
};


/* call this to initialize console fd stuff */
void
raw_console_init()
{
  pcninit ();
  register_fd_ops(FD_TYPE_CONSOLE, &console_ops);
}




/*
 * use syscall to update cursor position saved in kernel
 */
static void
async_update ()
{
  int pos;
  static int old_pos = -1;
  struct Crtctl c;

  pos = crtat - Crtat;
  if (pos != old_pos)
    {
      c.pos = pos;
      sys_crtctl (&c, CRT_SETPOS);
      old_pos = pos;
    }
}



#define	wrtchar(c, at) \
  do {\
    char *cp = (char *)crtat; \
    *cp++ = (c); \
    *cp = (at); \
    crtat++; \
    vs.col++; \
  } while (0)


/* translate ANSI color codes to standard pc ones */
static char fgansitopc[] =
{
  FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
  FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
};

static char bgansitopc[] =
{
  BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
  BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
};


/*
 * iso latin to ibm 437 encoding iso2ibm437[char-128)]
 * characters not available are displayed as a caret
 */
static u_char const iso2ibm437[] =
{
  4, 4, 4, 4, 4, 4, 4, 4,	/* 128 */
  4, 4, 4, 4, 4, 4, 4, 4,	/* 136 */
  4, 4, 4, 4, 4, 4, 4, 4,	/* 144 */
  4, 4, 4, 4, 4, 4, 4, 4,	/* 152 */
  0xff, 0xad, 0x9b, 0x9c, 4, 0x9d, 4, 0x15,	/* 160 */
  4, 4, 0xa6, 0xae, 0xaa, 4, 4, 4,	/* 168 */
  0xf8, 0xf1, 0xfd, 4, 4, 0xe6, 4, 0xfa,	/* 176 */
  4, 4, 0xa7, 0xaf, 0xac, 0xab, 4, 0xa8,	/* 184 */
  4, 4, 4, 4, 0x8e, 0x8f, 0x92, 0x80,	/* 192 */
  4, 0x90, 4, 4, 4, 4, 4, 4,	/* 200 */
  4, 0xa5, 4, 4, 4, 4, 0x99, 4,	/* 208 */
  4, 4, 4, 4, 0x9a, 4, 4, 0xe1,	/* 216 */
  0x85, 0xa0, 0x83, 4, 0x84, 0x86, 0x91, 0x87,	/* 224 */
  0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,	/* 232 */
  4, 0xa4, 0x95, 0xa2, 0x93, 4, 0x94, 0xf6,	/* 240 */
  4, 0x97, 0xa3, 0x96, 0x81, 4, 4, 0x98		/* 248 */
};


/*
 * `pc3' termcap emulation.
 */
static void
sput (const u_char * cp, int n)
{
  u_char c, scroll = 0;

#ifdef XSERVER
  if (pc_xmode > 0)
    return;
#endif

#if 0
  if (crtat == (u_short *) 0)
    {
      pcninit ();
    }

  else
#else
  assert(crtat != 0);
#endif
    {
      struct Crtctl crt;
      u_short *start = (u_short*) CONSOLE_REGION;
      if (!sys_crtctl (&crt, CRT_GETPOS))
	{
	  Crtat = (u_short *) start;
	  crtat = (u_short *) (start + crt.pos);
	}
    }

  while (n--)
    {
      if (!(c = *cp++))
	continue;

      switch (c)
	{
	case 0x1B:
	  if (vs.state >= VSS_ESCAPE)
	    {
	      wrtchar (c, vs.so_at);
	      vs.state = 0;
	      goto maybe_scroll;
	    }
	  else
	    vs.state = VSS_ESCAPE;
	  break;
	case 0x9B:		/* CSI */
	  vs.cx = vs.cy = 0;
	  vs.state = VSS_EBRACE;
	  break;

	case '\t':
	  {
	    int inccol = 8 - (vs.col & 7);
	    crtat += inccol;
	    vs.col += inccol;
	  }
	maybe_scroll:
	  if (vs.col >= COL)
	    {
	      vs.col -= COL;
	      scroll = 1;
	    }
	  break;

	case '\b':
	  if (crtat <= Crtat)
	    break;
	  --crtat;
	  if (--vs.col < 0)
	    vs.col += COL;	/* non-destructive backspace */
	  break;

	case '\r':
	  crtat -= vs.col;
	  vs.col = 0;
	  break;

	case '\n':
	  crtat += vs.ncol;
	  scroll = 1;
#if 1
	  crtat -= vs.col;
	  vs.col = 0;
#endif
	  break;

	default:
	  switch (vs.state)
	    {
	    case 0:
	      if (c == '\a')
#if 0
		sysbeep (BEEP_FREQ, BEEP_TIME);
#else
		;
#endif
	      else
		{
		  /*
		   * If we're outputting multiple printed
		   * characters, just blast them to the
		   * screen until we reach the end of the
		   * buffer or a control character.  This
		   * saves time by short-circuiting the
		   * switch.
		   * If we reach the end of the line, we
		   * break to do a scroll check.
		   */
		  for (;;)
		    {
		      if (c & 0x80)
			c = iso2ibm437[c & 0x7f];
		      if (vs.so)
			wrtchar (c, vs.so_at);
		      else
			wrtchar (c, vs.at);
		      if (vs.col >= vs.ncol)
			{
			  vs.col = 0;
			  scroll = 1;
			  break;
			}
		      if (!n || (c = *cp) < ' ')
			break;
		      n--, cp++;
		    }
		}
	      break;
	    case VSS_ESCAPE:
	      switch (c)
		{
		case '[':	/* Start ESC [ sequence */
		  vs.cx = vs.cy = 0;
		  vs.state = VSS_EBRACE;
		  break;
		case 'c':	/* Create screen & home */
		  fillw ((vs.at << 8) | ' ',
			 Crtat, vs.nchr);
		  crtat = Crtat;
		  vs.col = 0;
		  vs.state = 0;
		  break;
		case '7':	/* save cursor pos */
		  vs.offset = crtat - Crtat;
		  vs.state = 0;
		  break;
		case '8':	/* restore cursor pos */
		  crtat = Crtat + vs.offset;
		  vs.row = vs.offset / vs.ncol;
		  vs.col = vs.offset % vs.ncol;
		  vs.state = 0;
		  break;
		default:	/* Invalid, clear state */
		  wrtchar (c, vs.so_at);
		  vs.state = 0;
		  goto maybe_scroll;
		}
	      break;
	    default:		/* VSS_EBRACE or VSS_EPARAM */
	      switch (c)
		{
		  int pos;
		case 'm':
		  if (!vs.cx)
		    vs.so = 0;
		  else
		    vs.so = 1;
		  vs.state = 0;
		  break;
		case 'A':
		  {		/* back cx rows */
		    int cx = vs.cx;
		    if (cx <= 0)
		      cx = 1;
		    else
		      cx %= vs.nrow;
		    pos = crtat - Crtat;
		    pos -= vs.ncol * cx;
		    if (pos < 0)
		      pos += vs.nchr;
		    crtat = Crtat + pos;
		    vs.state = 0;
		    break;
		  }
		case 'B':
		  {		/* down cx rows */
		    int cx = vs.cx;
		    if (cx <= 0)
		      cx = 1;
		    else
		      cx %= vs.nrow;
		    pos = crtat - Crtat;
		    pos += vs.ncol * cx;
		    if (pos >= vs.nchr)
		      pos -= vs.nchr;
		    crtat = Crtat + pos;
		    vs.state = 0;
		    break;
		  }
		case 'C':
		  {		/* right cursor */
		    int cx = vs.cx, col = vs.col;
		    if (cx <= 0)
		      cx = 1;
		    else
		      cx %= vs.ncol;
		    pos = crtat - Crtat;
		    pos += cx;
		    col += cx;
		    if (col >= vs.ncol)
		      {
			pos -= vs.ncol;
			col -= vs.ncol;
		      }
		    vs.col = col;
		    crtat = Crtat + pos;
		    vs.state = 0;
		    break;
		  }
		case 'D':
		  {		/* left cursor */
		    int cx = vs.cx, col = vs.col;
		    if (cx <= 0)
		      cx = 1;
		    else
		      cx %= vs.ncol;
		    pos = crtat - Crtat;
		    pos -= cx;
		    col -= cx;
		    if (col < 0)
		      {
			pos += vs.ncol;
			col += vs.ncol;
		      }
		    vs.col = col;
		    crtat = Crtat + pos;
		    vs.state = 0;
		    break;
		  }
		case 'J':	/* Clear ... */
		  switch (vs.cx)
		    {
		    case 0:
		      /* ... to end of display */
		      fillw ((vs.at << 8) | ' ',
			     crtat,
			     Crtat + vs.nchr - crtat);
		      break;
		    case 1:
		      /* ... to next location */
		      fillw ((vs.at << 8) | ' ',
			     Crtat, crtat - Crtat + 1);
		      break;
		    case 2:
		      /* ... whole display */
		      fillw ((vs.at << 8) | ' ',
			     Crtat, vs.nchr);
		      break;
		    }
		  vs.state = 0;
		  break;
		case 'K':	/* Clear line ... */
		  switch (vs.cx)
		    {
		    case 0:
		      /* ... current to EOL */
		      fillw ((vs.at << 8) | ' ',
			     crtat, vs.ncol - vs.col);
		      break;
		    case 1:
		      /* ... beginning to next */
		      fillw ((vs.at << 8) | ' ',
			     crtat - vs.col, vs.col + 1);
		      break;
		    case 2:
		      /* ... entire line */
		      fillw ((vs.at << 8) | ' ',
			     crtat - vs.col, vs.ncol);
		      break;
		    }
		  vs.state = 0;
		  break;
		case 'f':	/* in system V consoles */
		case 'H':
		  {		/* Cursor move */
		    int cx = vs.cx, cy = vs.cy;
		    if (!cx || !cy)
		      {
			crtat = Crtat;
			vs.col = 0;
		      }
		    else
		      {
			if (cx > vs.nrow)
			  cx = vs.nrow;
			if (cy > vs.ncol)
			  cy = vs.ncol;
			crtat = Crtat +
			  (cx - 1) * vs.ncol + cy - 1;
			vs.col = cy - 1;
		      }
		    vs.state = 0;
		    break;
		  }
		case 'M':
		  {		/* delete cx rows */
		    u_short *crtAt = crtat - vs.col;
		    int cx = vs.cx, row = (crtAt - Crtat) / vs.ncol, nrow = vs.nrow - row;
		    if (cx <= 0)
		      cx = 1;
		    else if (cx > nrow)
		      cx = nrow;
		    if (cx < nrow)
		      bcopy (crtAt + vs.ncol * cx,
			     crtAt, vs.ncol * (nrow -
					       cx) * CHR);
		    fillw ((vs.at << 8) | ' ',
			   crtAt + vs.ncol * (nrow - cx),
			   vs.ncol * cx);
		    vs.state = 0;
		    break;
		  }
		case 'S':
		  {		/* scroll up cx lines */
		    int cx = vs.cx;
		    if (cx <= 0)
		      cx = 1;
		    else if (cx > vs.nrow)
		      cx = vs.nrow;
		    if (cx < vs.nrow)
		      bcopy (Crtat + vs.ncol * cx,
			     Crtat, vs.ncol * (vs.nrow -
					       cx) * CHR);
		    fillw ((vs.at << 8) | ' ',
			   Crtat + vs.ncol * (vs.nrow - cx),
			   vs.ncol * cx);
#if 0
		    crtat -= vs.ncol * cx;	/* XXX */
#endif
		    vs.state = 0;
		    break;
		  }
		case 'L':
		  {		/* insert cx rows */
		    u_short *crtAt = crtat - vs.col;
		    int cx = vs.cx, 
		        row = (crtAt - Crtat) / vs.ncol, nrow = vs.nrow - row;
		    if (cx <= 0)
		      cx = 1;
		    else if (cx > nrow)
		      cx = nrow;
		    if (cx < nrow)
		      bcopy (crtAt,
			     crtAt + vs.ncol * cx,
			     vs.ncol * (nrow - cx) *
			     CHR);
		    fillw ((vs.at << 8) | ' ',
			   crtAt, vs.ncol * cx);
		    vs.state = 0;
		    break;
		  }
		case 'T':
		  {		/* scroll down cx lines */
		    int cx = vs.cx;
		    if (cx <= 0)
		      cx = 1;
		    else if (cx > vs.nrow)
		      cx = vs.nrow;
		    if (cx < vs.nrow)
		      bcopy (Crtat,
			     Crtat + vs.ncol * cx,
			     vs.ncol * (vs.nrow - cx) *
			     CHR);
		    fillw ((vs.at << 8) | ' ',
			   Crtat, vs.ncol * cx);
#if 0
		    crtat += vs.ncol * cx;	/* XXX */
#endif
		    vs.state = 0;
		    break;
		  }
		case ';':	/* Switch params in cursor def */
		  vs.state = VSS_EPARAM;
		  break;
		case 'r':
		  vs.so_at = (vs.cx & FG_MASK) |
		    ((vs.cy << 4) & BG_MASK);
		  vs.state = 0;
		  break;
		case 's':	/* save cursor pos */
		  vs.offset = crtat - Crtat;
		  vs.state = 0;
		  break;
		case 'u':	/* restore cursor pos */
		  crtat = Crtat + vs.offset;
		  vs.row = vs.offset / vs.ncol;
		  vs.col = vs.offset % vs.ncol;
		  vs.state = 0;
		  break;
		case 'x':	/* set attributes */
		  switch (vs.cx)
		    {
		    case 0:
		      vs.at = FG_LIGHTGREY | BG_BLACK;
		      break;
		    case 1:
		      /* ansi background */
		      if (!vs.color)
			break;
		      vs.at &= FG_MASK;
		      vs.at |= bgansitopc[vs.cy & 7];
		      break;
		    case 2:
		      /* ansi foreground */
		      if (!vs.color)
			break;
		      vs.at &= BG_MASK;
		      vs.at |= fgansitopc[vs.cy & 7];
		      break;
		    case 3:
		      /* pc text attribute */
		      if (vs.state >= VSS_EPARAM)
			vs.at = vs.cy;
		      break;
		    }
		  vs.state = 0;
		  break;

		default:	/* Only numbers valid here */
		  if ((c >= '0') && (c <= '9'))
		    {
		      if (vs.state >= VSS_EPARAM)
			{
			  vs.cy *= 10;
			  vs.cy += c - '0';
			}
		      else
			{
			  vs.cx *= 10;
			  vs.cx += c - '0';
			}
		    }
		  else
		    vs.state = 0;
		  break;
		}
	      break;
	    }
	}
      if (scroll)
	{
	  scroll = 0;
	  /* scroll check */
	  if (crtat >= Crtat + vs.nchr)
	    {
#if 0
	      if (!kernel)
		{
		  int s = spltty ();
		  if (lock_state & KB_SCROLL)
		    tsleep (&lock_state,
			    PUSER, "pcputc", 0);
		  splx (s);
		}
#endif
	      bcopy (Crtat + vs.ncol, Crtat,
		     (vs.nchr - vs.ncol) * CHR);
	      fillw ((vs.at << 8) | ' ',
		     Crtat + vs.nchr - vs.ncol,
		     vs.ncol);
	      crtat -= vs.ncol;
	    }
	}
    }
  async_update ();
}
