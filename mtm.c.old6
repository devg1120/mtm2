/* Copyright 2017 - 2019 Rob King <jking@deadpixi.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
//#include <string.h>
//#include <stdio.h>

#include "vtparser.h"

/*** CONFIGURATION */
#include "config.h"
#include "log.h"

#include "toml.h"
#include <panel.h>

char *lineedit();

int read_symlink(char *filepath, char *buf);
int isfullscreen = START_IN_FULLSCREEN;


#define MIN(x, y) ((x) < (y)? (x) : (y))
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define CTL(x) ((x) & 0x1f)
#define USAGE "usage: mtm [-T NAME] [-t NAME] [-c KEY]\n"

#define GRASS_PAIR     1
#define EMPTY_PAIR     1
#define WATER_PAIR     2
#define MOUNTAIN_PAIR  3
#define LINE_PAIR    4
#define STATUS_BAR_PAIR    5
#define STATUS_BAR_EXPANDROOT_PAIR    6



/*** DATA TYPES */
typedef enum{
    HORIZONTAL,
    VERTICAL,
    VIEW
} Node;

typedef enum{
    ROOT,
    CHILD,
    EXPANDROOT
} NodeType;

typedef struct SCRN SCRN;
struct SCRN{
    int sy, sx, vis, tos, off;
    short fg, bg, sfg, sbg, sp;
    bool insert, oxenl, xenl, saved;
    attr_t sattr;
    WINDOW *win;
};

typedef struct NODE NODE;
struct NODE{
    Node t;
    int y, x, h, w, pt, ntabs;
    bool *tabs, pnm, decom, am, lnm;
    wchar_t repc;
    NODE *p, *c1, *c2;
    SCRN pri, alt, *s;
    wchar_t *g0, *g1, *g2, *g3, *gc, *gs, *sgc, *sgs;
    VTPARSER vp;
    int refcnt;
    NodeType type;
    bool expand;
    char label_buf[256];
    //char *label_buf ;
    bool  command_shell_loop;
    int pid;
    bool  isfetch;
    NODE *fetch_save_node;
    NODE **fetch_save_node_cp;
    int root_index;
    //bool c1_isfetch;
    //bool c2_isfetch;
    //NODE *fetch_parent;
};

/*** GLOBALS AND PROTOTYPES */
//static NODE *root, *focused, *lastfocused = NULL;
static bool t_restore_mode = false;
//static NODE *focused, *lastfocused = NULL;
//static NODE *root = NULL;
#define PANE_MAX 10
static NODE *t_root[PANE_MAX] ;
static int  t_root_enable[PANE_MAX] ;
static NODE *t_focused[PANE_MAX] ;
static NODE *t_lastfocused[PANE_MAX] ;

static int  t_fd_set[PANE_MAX];
static int  t_fd_set_init_count = 0;

static int   t_root_index = 0;
static int   t_root_change = 0;
static bool  window_label_show = false;
//static bool  command_shell_loop = false;
//
static int t_mouse_x = 0;
static int t_mouse_y = 0;
static bool  t_mouse_focus = false;

static NODE * push_node = NULL;
//static NODE * save_node = NULL;
//static NODE ** save_node_cp = NULL;

enum Pane_change_type { CREATE , RESTORE, NEXT , PREV, TOGGLE_LABEL, EXPAND, SWAP, FETCH};
static enum Pane_change_type t_root_change_type;
static NODE *t_expand_node;

//#define root t_root[t_root_index]

static int commandkey = CTL(COMMAND_KEY), nfds = 1, b_nfds; /* stdin */
static fd_set fds;
static fd_set b_fds;
static char iobuf[BUFSIZ];

static void setupevents(NODE *n);
static void reshape(NODE *n, int y, int x, int h, int w);
static void draw(NODE *n);
static void reshapechildren(NODE *n);
static const char *term = NULL;
static void freenode(NODE *n, bool recursive);
void start_pairs(void);
short mtm_alloc_pair(int fg, int bg);
static toml_table_t* restore_tab;

/*** UTILITY FUNCTIONS */
static void
quit(int rc, const char *m) /* Shut down MTM. */
{
    if (m)
        fprintf(stderr, "%s\n", m);
    if (t_root[t_root_index])
        freenode(t_root[t_root_index], true);
    endwin();
    exit(rc);
}

static void
safewrite(int fd, const char *b, size_t n) /* Write, checking for errors. */
{
    //LOG_PRINT(":%s\n", b);
    //LOG_PRINT(":%d\n", n);
    size_t w = 0;
    while (w < n){
        ssize_t s = write(fd, b + w, n - w);
        if (s < 0 && errno != EINTR)
            return;
        else if (s < 0)
            s = 0;
        w += (size_t)s;
    }
}

static const char *
getshell(void) /* Get the user's preferred shell. */
{
    if (getenv("SHELL"))
        return getenv("SHELL");
    struct passwd *pwd = getpwuid(getuid());
    if (pwd)
        return pwd->pw_shell;
    return "/bin/sh";
}

/*** TERMINAL EMULATION HANDLERS
 * These functions implement the various terminal commands activated by
 * escape sequences and printing to the terminal. Large amounts of boilerplate
 * code is shared among all these functions, and is factored out into the
 * macros below:
 *      PD(n, d)       - Parameter n, with default d.
 *      P0(n)          - Parameter n, default 0.
 *      P1(n)          - Parameter n, default 1.
 *      CALL(h)        - Call handler h with no arguments.
 *      SENDN(n, s, c) - Write string c bytes of s to n.
 *      SEND(n, s)     - Write string s to node n's host.
 *      (END)HANDLER   - Declare/end a handler function
 *      COMMONVARS     - All of the common variables for a handler.
 *                       x, y     - cursor position
 *                       mx, my   - max possible values for x and y
 *                       px, py   - physical cursor position in scrollback
 *                       n        - the current node
 *                       win      - the current window
 *                       top, bot - the scrolling region
 *                       tos      - top of the screen in the pad
 *                       s        - the current SCRN buffer
 * The funny names for handlers are from their ANSI/ECMA/DEC mnemonics.
 */
#define PD(x, d) (argc < (x) || !argv? (d) : argv[(x)])
#define P0(x) PD(x, 0)
#define P1(x) (!P0(x)? 1 : P0(x))
#define CALL(x) (x)(v, n, 0, 0, 0, NULL, NULL)
#define SENDN(n, s, c) safewrite(n->pt, s, c)
#define SEND(n, s) SENDN(n, s, strlen(s))
#define COMMONVARS                                                      \
    NODE *n = (NODE *)p;                                                \
    SCRN *s = n->s;                                                     \
    WINDOW *win = s->win;                                               \
    int py, px, y, x, my, mx, top = 0, bot = 0, tos = s->tos;           \
    (void)v; (void)p; (void)w; (void)iw; (void)argc; (void)argv;        \
    (void)win; (void)y; (void)x; (void)my; (void)mx; (void)osc;         \
    (void)tos;                                                          \
    getyx(win, py, px); y = py - s->tos; x = px;                        \
    getmaxyx(win, my, mx); my -= s->tos;                                \
    wgetscrreg(win, &top, &bot);                                        \
    bot++; bot -= s->tos;                                               \
    top = top <= tos? 0 : top - tos;                                    \

#define HANDLER(name)                                   \
    static void                                         \
    name (VTPARSER *v, void *p, wchar_t w, wchar_t iw,  \
          int argc, int *argv, const wchar_t *osc)      \
    { COMMONVARS
#define ENDHANDLER n->repc = 0; } /* control sequences aren't repeated */

HANDLER(bell) /* Terminal bell. */
    beep();
ENDHANDLER

HANDLER(numkp) /* Application/Numeric Keypad Mode */
    n->pnm = (w == L'=');
ENDHANDLER

HANDLER(vis) /* Cursor visibility */
    s->vis = iw == L'6'? 0 : 1;
ENDHANDLER

HANDLER(cup) /* CUP - Cursor Position */
    s->xenl = false;
    wmove(win, tos + (n->decom? top : 0) + P1(0) - 1, P1(1) - 1);
ENDHANDLER

HANDLER(dch) /* DCH - Delete Character */
    for (int i = 0; i < P1(0); i++)
        wdelch(win);
ENDHANDLER

HANDLER(ich) /* ICH - Insert Character */
    for (int i = 0; i < P1(0); i++)
        wins_nwstr(win, L" ", 1);
ENDHANDLER

HANDLER(cuu) /* CUU - Cursor Up */
    wmove(win, MAX(py - P1(0), tos + top), x);
ENDHANDLER

HANDLER(cud) /* CUD - Cursor Down */
    wmove(win, MIN(py + P1(0), tos + bot - 1), x);
ENDHANDLER

HANDLER(cuf) /* CUF - Cursor Forward */
    wmove(win, py, MIN(x + P1(0), mx - 1));
ENDHANDLER

HANDLER(ack) /* ACK - Acknowledge Enquiry */
    SEND(n, "\006");
ENDHANDLER

HANDLER(hts) /* HTS - Horizontal Tab Set */
    if (x < n->ntabs && x > 0)
        n->tabs[x] = true;
ENDHANDLER

HANDLER(ri) /* RI - Reverse Index */
    int otop = 0, obot = 0;
    wgetscrreg(win, &otop, &obot);
    wsetscrreg(win, otop >= tos? otop : tos, obot);
    y == top? wscrl(win, -1) : wmove(win, MAX(tos, py - 1), x);
    wsetscrreg(win, otop, obot);
ENDHANDLER

HANDLER(decid) /* DECID - Send Terminal Identification */
    if (w == L'c')
        SEND(n, iw == L'>'? "\033[>1;10;0c" : "\033[?1;2c");
    else if (w == L'Z')
        SEND(n, "\033[?6c");
ENDHANDLER

HANDLER(hpa) /* HPA - Cursor Horizontal Absolute */
    wmove(win, py, MIN(P1(0) - 1, mx - 1));
ENDHANDLER

HANDLER(hpr) /* HPR - Cursor Horizontal Relative */
    wmove(win, py, MIN(px + P1(0), mx - 1));
ENDHANDLER

HANDLER(vpa) /* VPA - Cursor Vertical Absolute */
    wmove(win, MIN(tos + bot - 1, MAX(tos + top, tos + P1(0) - 1)), x);
ENDHANDLER

HANDLER(vpr) /* VPR - Cursor Vertical Relative */
    wmove(win, MIN(tos + bot - 1, MAX(tos + top, py + P1(0))), x);
ENDHANDLER

HANDLER(cbt) /* CBT - Cursor Backwards Tab */
    for (int i = x - 1; i < n->ntabs && i >= 0; i--) if (n->tabs[i]){
        wmove(win, py, i);
        return;
    }
    wmove(win, py, 0);
ENDHANDLER

HANDLER(ht) /* HT - Horizontal Tab */
    for (int i = x + 1; i < n->w && i < n->ntabs; i++) if (n->tabs[i]){
        wmove(win, py, i);
        return;
    }
    wmove(win, py, mx - 1);
ENDHANDLER

HANDLER(tab) /* Tab forwards or backwards */
    for (int i = 0; i < P1(0); i++) switch (w){
        case L'I':  CALL(ht);  break;
        case L'\t': CALL(ht);  break;
        case L'Z':  CALL(cbt); break;
    }
ENDHANDLER

HANDLER(decaln) /* DECALN - Screen Alignment Test */
    chtype e[] = {COLOR_PAIR(0) | 'E', 0};
    for (int r = 0; r < my; r++){
        for (int c = 0; c <= mx; c++)
            mvwaddchnstr(win, tos + r, c, e, 1);
    }
    wmove(win, py, px);
ENDHANDLER

HANDLER(su) /* SU - Scroll Up/Down */
    wscrl(win, (w == L'T' || w == L'^')? -P1(0) : P1(0));
ENDHANDLER

HANDLER(sc) /* SC - Save Cursor */
    s->sx = px;                              /* save X position            */
    s->sy = py;                              /* save Y position            */
    wattr_get(win, &s->sattr, &s->sp, NULL); /* save attrs and color pair  */
    s->sfg = s->fg;                          /* save foreground color      */
    s->sbg = s->bg;                          /* save background color      */
    s->oxenl = s->xenl;                      /* save xenl state            */
    s->saved = true;                         /* save data is valid         */
    n->sgc = n->gc; n->sgs = n->gs;          /* save character sets        */
ENDHANDLER

HANDLER(rc) /* RC - Restore Cursor */
    if (iw == L'#'){
        CALL(decaln);
        return;
    }
    if (!s->saved)
        return;
    wmove(win, s->sy, s->sx);                /* get old position          */
    wattr_set(win, s->sattr, s->sp, NULL);   /* get attrs and color pair  */
    s->fg = s->sfg;                          /* get foreground color      */
    s->bg = s->sbg;                          /* get background color      */
    s->xenl = s->oxenl;                      /* get xenl state            */
    n->gc = n->sgc; n->gs = n->sgs;          /* save character sets        */

    /* restore colors */
    int cp = mtm_alloc_pair(s->fg, s->bg);
    wcolor_set(win, cp, NULL);
    cchar_t c;
    setcchar(&c, L" ", A_NORMAL, cp, NULL);
    wbkgrndset(win, &c);
ENDHANDLER

HANDLER(tbc) /* TBC - Tabulation Clear */
    switch (P0(0)){
        case 0: n->tabs[x < n->ntabs? x : 0] = false;          break;
        case 3: memset(n->tabs, 0, sizeof(bool) * (n->ntabs)); break;
    }
ENDHANDLER

HANDLER(cub) /* CUB - Cursor Backward */
    s->xenl = false;
    wmove(win, py, MAX(x - P1(0), 0));
ENDHANDLER

HANDLER(el) /* EL - Erase in Line */
    cchar_t b;
    setcchar(&b, L" ", A_NORMAL, mtm_alloc_pair(s->fg, s->bg), NULL);
    switch (P0(0)){
        case 0: wclrtoeol(win);                                                 break;
        case 1: for (int i = 0; i <= x; i++) mvwadd_wchnstr(win, py, i, &b, 1); break;
        case 2: wmove(win, py, 0); wclrtoeol(win);                              break;
    }
    wmove(win, py, x);
ENDHANDLER

HANDLER(ed) /* ED - Erase in Display */
    int o = 1;
    switch (P0(0)){
        case 0: wclrtobot(win);                     break;
        case 3: werase(win);                        break;
        case 2: wmove(win, tos, 0); wclrtobot(win); break;
        case 1:
            for (int i = tos; i < py; i++){
                wmove(win, i, 0);
                wclrtoeol(win);
            }
            wmove(win, py, x);
            el(v, p, w, iw, 1, &o, NULL);
            break;
    }
    wmove(win, py, px);
ENDHANDLER

HANDLER(ech) /* ECH - Erase Character */
    cchar_t c;
    setcchar(&c, L" ", A_NORMAL, mtm_alloc_pair(s->fg, s->bg), NULL);
    for (int i = 0; i < P1(0); i++)
        mvwadd_wchnstr(win, py, x + i, &c, 1);
    wmove(win, py, px);
ENDHANDLER

HANDLER(dsr) /* DSR - Device Status Report */
    char buf[100] = {0};
    if (P0(0) == 6)
        snprintf(buf, sizeof(buf) - 1, "\033[%d;%dR",
                 (n->decom? y - top : y) + 1, x + 1);
    else
        snprintf(buf, sizeof(buf) - 1, "\033[0n");
    SEND(n, buf);
ENDHANDLER

HANDLER(idl) /* IL or DL - Insert/Delete Line */
    /* we don't use insdelln here because it inserts above and not below,
     * and has a few other edge cases... */
    int otop = 0, obot = 0, p1 = MIN(P1(0), (my - 1) - y);
    wgetscrreg(win, &otop, &obot);
    wsetscrreg(win, py, obot);
    wscrl(win, w == L'L'? -p1 : p1);
    wsetscrreg(win, otop, obot);
    wmove(win, py, 0);
ENDHANDLER

HANDLER(csr) /* CSR - Change Scrolling Region */
    if (wsetscrreg(win, tos + P1(0) - 1, tos + PD(1, my) - 1) == OK)
        CALL(cup);
ENDHANDLER

HANDLER(decreqtparm) /* DECREQTPARM - Request Device Parameters */
    SEND(n, P0(0)? "\033[3;1;2;120;1;0x" : "\033[2;1;2;120;128;1;0x");
ENDHANDLER

HANDLER(sgr0) /* Reset SGR to default */
    wattrset(win, A_NORMAL);
    wcolor_set(win, 0, NULL);
    s->fg = s->bg = -1;
    wbkgdset(win, COLOR_PAIR(0) | ' ');
ENDHANDLER

HANDLER(cls) /* Clear screen */
    CALL(cup);
    wclrtobot(win);
    CALL(cup);
ENDHANDLER

HANDLER(ris) /* RIS - Reset to Initial State */
    n->gs = n->gc = n->g0 = CSET_US; n->g1 = CSET_GRAPH;
    n->g2 = CSET_US; n->g3 = CSET_GRAPH;
    n->decom = s->insert = s->oxenl = s->xenl = n->lnm = false;
    CALL(cls);
    CALL(sgr0);
    n->am = true;
    n->pnm = false;
    n->pri.vis = n->alt.vis = 1;
    n->s = &n->pri;
    wsetscrreg(n->pri.win, 0, MAX(SCROLLBACK, n->h) - 1);
    wsetscrreg(n->alt.win, 0, n->h - 1);
    for (int i = 0; i < n->ntabs; i++)
        n->tabs[i] = (i % 8 == 0);
ENDHANDLER

HANDLER(mode) /* Set or Reset Mode */
    bool set = (w == L'h');
    for (int i = 0; i < argc; i++) switch (P0(i)){
        case  1: n->pnm = set;              break;
        case  3: CALL(cls);                 break;
        case  4: s->insert = set;           break;
        case  6: n->decom = set; CALL(cup); break;
        case  7: n->am = set;               break;
        case 20: n->lnm = set;              break;
        case 25: s->vis = set? 1 : 0;       break;
        case 34: s->vis = set? 1 : 2;       break;
        case 1048: CALL((set? sc : rc));    break;
        case 1049:
            CALL((set? sc : rc)); /* fall-through */
        case 47: case 1047: if (set && n->s != &n->alt){
                n->s = &n->alt;
                CALL(cls);
            } else if (!set && n->s != &n->pri)
                n->s = &n->pri;
            break;
    }
ENDHANDLER

HANDLER(sgr) /* SGR - Select Graphic Rendition */
    bool doc = false, do8 = COLORS >= 8, do16 = COLORS >= 16, do256 = COLORS >= 256;
    if (!argc)
        CALL(sgr0);

    short bg = s->bg, fg = s->fg;
    for (int i = 0; i < argc; i++) switch (P0(i)){
        case  0:  CALL(sgr0);                                              break;
        case  1:  wattron(win,  A_BOLD);                                   break;
        case  2:  wattron(win,  A_DIM);                                    break;
        case  4:  wattron(win,  A_UNDERLINE);                              break;
        case  5:  wattron(win,  A_BLINK);                                  break;
        case  7:  wattron(win,  A_REVERSE);                                break;
        case  8:  wattron(win,  A_INVIS);                                  break;
        case 22:  wattroff(win, A_DIM); wattroff(win, A_BOLD);             break;
        case 24:  wattroff(win, A_UNDERLINE);                              break;
        case 25:  wattroff(win, A_BLINK);                                  break;
        case 27:  wattroff(win, A_REVERSE);                                break;
        case 30:  fg = COLOR_BLACK;                           doc = do8;   break;
        case 31:  fg = COLOR_RED;                             doc = do8;   break;
        case 32:  fg = COLOR_GREEN;                           doc = do8;   break;
        case 33:  fg = COLOR_YELLOW;                          doc = do8;   break;
        case 34:  fg = COLOR_BLUE;                            doc = do8;   break;
        case 35:  fg = COLOR_MAGENTA;                         doc = do8;   break;
        case 36:  fg = COLOR_CYAN;                            doc = do8;   break;
        case 37:  fg = COLOR_WHITE;                           doc = do8;   break;
        case 38:  fg = P0(i+1) == 5? P0(i+2) : s->fg; i += 2; doc = do256; break;
        case 39:  fg = -1;                                    doc = true;  break;
        case 40:  bg = COLOR_BLACK;                           doc = do8;   break;
        case 41:  bg = COLOR_RED;                             doc = do8;   break;
        case 42:  bg = COLOR_GREEN;                           doc = do8;   break;
        case 43:  bg = COLOR_YELLOW;                          doc = do8;   break;
        case 44:  bg = COLOR_BLUE;                            doc = do8;   break;
        case 45:  bg = COLOR_MAGENTA;                         doc = do8;   break;
        case 46:  bg = COLOR_CYAN;                            doc = do8;   break;
        case 47:  bg = COLOR_WHITE;                           doc = do8;   break;
        case 48:  bg = P0(i+1) == 5? P0(i+2) : s->bg; i += 2; doc = do256; break;
        case 49:  bg = -1;                                    doc = true;  break;
        case 90:  fg = COLOR_BLACK;                           doc = do16;  break;
        case 91:  fg = COLOR_RED;                             doc = do16;  break;
        case 92:  fg = COLOR_GREEN;                           doc = do16;  break;
        case 93:  fg = COLOR_YELLOW;                          doc = do16;  break;
        case 94:  fg = COLOR_BLUE;                            doc = do16;  break;
        case 95:  fg = COLOR_MAGENTA;                         doc = do16;  break;
        case 96:  fg = COLOR_CYAN;                            doc = do16;  break;
        case 97:  fg = COLOR_WHITE;                           doc = do16;  break;
        case 100: bg = COLOR_BLACK;                           doc = do16;  break;
        case 101: bg = COLOR_RED;                             doc = do16;  break;
        case 102: bg = COLOR_GREEN;                           doc = do16;  break;
        case 103: bg = COLOR_YELLOW;                          doc = do16;  break;
        case 104: bg = COLOR_BLUE;                            doc = do16;  break;
        case 105: bg = COLOR_MAGENTA;                         doc = do16;  break;
        case 106: bg = COLOR_CYAN;                            doc = do16;  break;
        case 107: bg = COLOR_WHITE;                           doc = do16;  break;
        #if defined(A_ITALIC) && !defined(NO_ITALICS )
        case  3:  wattron(win,  A_ITALIC);                    break;
        case 23:  wattroff(win, A_ITALIC);                    break;
        #endif
    }
    if (doc){
        int p = mtm_alloc_pair(s->fg = fg, s->bg = bg);
        wcolor_set(win, p, NULL);
        cchar_t c;
        setcchar(&c, L" ", A_NORMAL, p, NULL);
        wbkgrndset(win, &c);
   }
}

HANDLER(cr) /* CR - Carriage Return */
    s->xenl = false;
    wmove(win, py, 0);
ENDHANDLER

HANDLER(ind) /* IND - Index */
    y == (bot - 1)? scroll(win) : wmove(win, py + 1, x);
ENDHANDLER

HANDLER(nel) /* NEL - Next Line */
    CALL(cr); CALL(ind);
ENDHANDLER

HANDLER(pnl) /* NL - Newline */
    CALL((n->lnm? nel : ind));
ENDHANDLER

HANDLER(cpl) /* CPL - Cursor Previous Line */
    wmove(win, MAX(tos + top, py - P1(0)), 0);
ENDHANDLER

HANDLER(cnl) /* CNL - Cursor Next Line */
    wmove(win, MIN(tos + bot - 1, py + P1(0)), 0);
ENDHANDLER

HANDLER(print) /* Print a character to the terminal */
    if (wcwidth(w) < 0)
        return;

    if (s->insert)
        CALL(ich);

    if (s->xenl){
        s->xenl = false;
        if (n->am)
            CALL(nel);
        getyx(win, y, x);
        y -= tos;
    }

    if (w < MAXMAP && n->gc[w])
        w = n->gc[w];
    n->repc = w;

    if (x == mx - wcwidth(w)){
        s->xenl = true;
        wins_nwstr(win, &w, 1);
    } else
        waddnwstr(win, &w, 1);
    n->gc = n->gs;
} /* no ENDHANDLER because we don't want to reset repc */

HANDLER(rep) /* REP - Repeat Character */
    for (int i = 0; i < P1(0) && n->repc; i++)
        print(v, p, n->repc, 0, 0, NULL, NULL);
ENDHANDLER

HANDLER(scs) /* Select Character Set */
    wchar_t **t = NULL;
    switch (iw){
        case L'(': t = &n->g0;  break;
        case L')': t = &n->g1;  break;
        case L'*': t = &n->g2;  break;
        case L'+': t = &n->g3;  break;
        default: return;        break;
    }
    switch (w){
        case L'A': *t = CSET_UK;    break;
        case L'B': *t = CSET_US;    break;
        case L'0': *t = CSET_GRAPH; break;
        case L'1': *t = CSET_US;    break;
        case L'2': *t = CSET_GRAPH; break;
    }
ENDHANDLER

HANDLER(so) /* Switch Out/In Character Set */
    if (w == 0x0e)
        n->gs = n->gc = n->g1; /* locking shift */
    else if (w == 0xf)
        n->gs = n->gc = n->g0; /* locking shift */
    else if (w == L'n')
        n->gs = n->gc = n->g2; /* locking shift */
    else if (w == L'o')
        n->gs = n->gc = n->g3; /* locking shift */
    else if (w == L'N'){
        n->gs = n->gc; /* non-locking shift */
        n->gc = n->g2;
    } else if (w == L'O'){
        n->gs = n->gc; /* non-locking shift */
        n->gc = n->g3;
    }
ENDHANDLER

static void
setupevents(NODE *n)
{
    n->vp.p = n;
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x05, ack);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x07, bell);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x08, cub);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x09, tab);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0a, pnl);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0b, pnl);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0c, pnl);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0d, cr);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0e, so);
    vtonevent(&n->vp, VTPARSER_CONTROL, 0x0f, so);
    vtonevent(&n->vp, VTPARSER_CSI,     L'A', cuu);
    vtonevent(&n->vp, VTPARSER_CSI,     L'B', cud);
    vtonevent(&n->vp, VTPARSER_CSI,     L'C', cuf);
    vtonevent(&n->vp, VTPARSER_CSI,     L'D', cub);
    vtonevent(&n->vp, VTPARSER_CSI,     L'E', cnl);
    vtonevent(&n->vp, VTPARSER_CSI,     L'F', cpl);
    vtonevent(&n->vp, VTPARSER_CSI,     L'G', hpa);
    vtonevent(&n->vp, VTPARSER_CSI,     L'H', cup);
    vtonevent(&n->vp, VTPARSER_CSI,     L'I', tab);
    vtonevent(&n->vp, VTPARSER_CSI,     L'J', ed);
    vtonevent(&n->vp, VTPARSER_CSI,     L'K', el);
    vtonevent(&n->vp, VTPARSER_CSI,     L'L', idl);
    vtonevent(&n->vp, VTPARSER_CSI,     L'M', idl);
    vtonevent(&n->vp, VTPARSER_CSI,     L'P', dch);
    vtonevent(&n->vp, VTPARSER_CSI,     L'S', su);
    vtonevent(&n->vp, VTPARSER_CSI,     L'T', su);
    vtonevent(&n->vp, VTPARSER_CSI,     L'X', ech);
    vtonevent(&n->vp, VTPARSER_CSI,     L'Z', tab);
    vtonevent(&n->vp, VTPARSER_CSI,     L'`', hpa);
    vtonevent(&n->vp, VTPARSER_CSI,     L'^', su);
    vtonevent(&n->vp, VTPARSER_CSI,     L'@', ich);
    vtonevent(&n->vp, VTPARSER_CSI,     L'a', hpr);
    vtonevent(&n->vp, VTPARSER_CSI,     L'b', rep);
    vtonevent(&n->vp, VTPARSER_CSI,     L'c', decid);
    vtonevent(&n->vp, VTPARSER_CSI,     L'd', vpa);
    vtonevent(&n->vp, VTPARSER_CSI,     L'e', vpr);
    vtonevent(&n->vp, VTPARSER_CSI,     L'f', cup);
    vtonevent(&n->vp, VTPARSER_CSI,     L'g', tbc);
    vtonevent(&n->vp, VTPARSER_CSI,     L'h', mode);
    vtonevent(&n->vp, VTPARSER_CSI,     L'l', mode);
    vtonevent(&n->vp, VTPARSER_CSI,     L'm', sgr);
    vtonevent(&n->vp, VTPARSER_CSI,     L'n', dsr);
    vtonevent(&n->vp, VTPARSER_CSI,     L'r', csr);
    vtonevent(&n->vp, VTPARSER_CSI,     L's', sc);
    vtonevent(&n->vp, VTPARSER_CSI,     L'u', rc);
    vtonevent(&n->vp, VTPARSER_CSI,     L'x', decreqtparm);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'0', scs);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'1', scs);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'2', scs);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'7', sc);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'8', rc);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'A', scs);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'B', scs);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'D', ind);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'E', nel);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'H', hts);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'M', ri);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'Z', decid);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'c', ris);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'p', vis);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'=', numkp);
    vtonevent(&n->vp, VTPARSER_ESCAPE,  L'>', numkp);
    vtonevent(&n->vp, VTPARSER_PRINT,   0,    print);
}

/*** MTM FUNCTIONS
 * These functions do the user-visible work of MTM: creating nodes in the
 * tree, updating the display, and so on.
 */
static bool *
newtabs(int w, int ow, bool *oldtabs) /* Initialize default tabstops. */
{
    bool *tabs = calloc(w, sizeof(bool));
    if (!tabs)
        return NULL;
    for (int i = 0; i < w; i++) /* keep old overlapping tabs */
        tabs[i] = i < ow? oldtabs[i] : (i % 8 == 0);
    return tabs;
}

static NODE *
newnode(Node t, NODE *p, int y, int x, int h, int w) /* Create a new node. */
{
    NODE *n = calloc(1, sizeof(NODE));
    bool *tabs = newtabs(w, 0, NULL);
    if (!n || h < 2 || w < 2 || !tabs)
        return free(n), free(tabs), NULL;

    n->t = t;
    n->pt = -1;
    n->p = p;
    n->y = y;
    n->x = x;
    n->h = h;
    n->w = w;
    n->tabs = tabs;
    n->ntabs = w;
    n->refcnt = 1;
    n->type = CHILD;
    n->expand = false;
    n->command_shell_loop = false;
    n->pid = 0;
    //n->c1_isfetch = false;
    //n->c2_isfetch = false;
    n->root_index = t_root_index;

    return n;
}

/*
static NODE *
clonenode(NODE *p) 
{
    NODE *n = calloc(1, sizeof(NODE));
    memcpy(n, p, sizeof(NODE));
    fcntl(n->pt, F_SETFL, O_NONBLOCK);
    nfds = n->pt > nfds? n->pt : nfds;

    return n;
}

static NODE *
clonenode2(NODE *p, int y, int x, int h, int w) 
{
    struct winsize ws = {.ws_row = h, .ws_col = w};
    NODE *n = newnode(VIEW, p, y, x, h, w);
    if (!n)
        return NULL;

    SCRN *pri = &n->pri, *alt = &n->alt;
    pri->win = newpad(MAX(h, SCROLLBACK), w);
    alt->win = newpad(h, w);
    if (!pri->win || !alt->win)
        return freenode(n, false), NULL;
    pri->tos = pri->off = MAX(0, SCROLLBACK - h);
    n->s = pri;

    nodelay(pri->win, TRUE); nodelay(alt->win, TRUE);
    scrollok(pri->win, TRUE); scrollok(alt->win, TRUE);
    keypad(pri->win, TRUE); keypad(alt->win, TRUE);

    //setupevents(n);
    //ris(&n->vp, n, L'c', 0, 0, NULL, NULL);

    //pid_t pid = forkpty(&n->pt, NULL, NULL, &ws);
    //if (pid < 0){
    //    if (!p)
    //        perror("forkpty");
    //    return freenode(n, false), NULL;
    //} else if (pid == 0){
    //    char buf[100] = {0};
    //    snprintf(buf, sizeof(buf) - 1, "%lu", (unsigned long)getppid());
    //    setsid();
    //    setenv("MTM", buf, 1);
    //    setenv("TERM", getterm(), 1);
    //    signal(SIGCHLD, SIG_DFL);
    //    execl(getshell(), getshell(), NULL);
    //    return NULL;
    //}

    //FD_SET(n->pt, &fds);
    //fcntl(n->pt, F_SETFL, O_NONBLOCK);
    //nfds = n->pt > nfds? n->pt : nfds;
    return n;
}
*/
static void
freenode(NODE *n, bool recurse) /* Free a node. */
{
    if (n){
        n->refcnt--;
        if (n->refcnt > 0) return;
        if (t_lastfocused[t_root_index] == n)
            t_lastfocused[t_root_index] = NULL;
        if (n->pri.win)
            delwin(n->pri.win);
        if (n->alt.win)
            delwin(n->alt.win);
        if (recurse)
            freenode(n->c1, true);
        if (recurse)
            freenode(n->c2, true);
        if (n->pt >= 0){
            close(n->pt);
            FD_CLR(n->pt, &fds);
        }
        free(n->tabs);
        free(n);
    }
}

static void
fixcursor(void) /* Move the terminal cursor to the active view. */
{
    if (t_focused[t_root_index]){
        NODE *focused = t_focused[t_root_index];
        int y, x;
        curs_set(focused->s->off != focused->s->tos? 0 : focused->s->vis);
        getyx(focused->s->win, y, x);
        y = MIN(MAX(y, focused->s->tos), focused->s->tos + focused->h - 1);
        wmove(focused->s->win, y, x);
    }
}

//static void
//fixcursor_old(void) /* Move the terminal cursor to the active view. */
//{
//    if (focused){
//        int y, x;
//        curs_set(focused->s->off != focused->s->tos? 0 : focused->s->vis);
//        getyx(focused->s->win, y, x);
//        y = MIN(MAX(y, focused->s->tos), focused->s->tos + focused->h - 1);
//        wmove(focused->s->win, y, x);
//    }
//}


static const char *
getterm(void)
{
    const char *envterm = getenv("TERM");
    if (term)
        return term;
    if (envterm && COLORS >= 256 && !strstr(DEFAULT_TERMINAL, "-256color"))
        return DEFAULT_256_COLOR_TERMINAL;
    return DEFAULT_TERMINAL;
}

static NODE *
//newview(NODE *p, int y, int x, int h, int w, ...) /* Open a new view. */
newview(NODE *p, int y, int x, int h, int w, char *cwd, char *exe) /* Open a new view. */
{
	/*
    char *cwd = NULL;
    char *exe = NULL;

    va_list args;
    va_start( args, w );
    cwd = va_arg( args, char* );
    exe = va_arg( args, char* );
    va_end( args );
*/


    struct winsize ws = {.ws_row = h, .ws_col = w};
    NODE *n = newnode(VIEW, p, y, x, h, w);
    if (!n)
        return NULL;

    SCRN *pri = &n->pri, *alt = &n->alt;
    pri->win = newpad(MAX(h, SCROLLBACK), w);
    alt->win = newpad(h, w);
    if (!pri->win || !alt->win)
        return freenode(n, false), NULL;
    pri->tos = pri->off = MAX(0, SCROLLBACK - h);
    n->s = pri;

    nodelay(pri->win, TRUE); nodelay(alt->win, TRUE);
    scrollok(pri->win, TRUE); scrollok(alt->win, TRUE);
    keypad(pri->win, TRUE); keypad(alt->win, TRUE);

    setupevents(n);
    ris(&n->vp, n, L'c', 0, 0, NULL, NULL);

    pid_t pid = forkpty(&n->pt, NULL, NULL, &ws);
    if (pid < 0){
        if (!p)
            perror("forkpty");
        return freenode(n, false), NULL;
    } else if (pid == 0){
        char buf[100] = {0};
        snprintf(buf, sizeof(buf) - 1, "%lu", (unsigned long)getppid());
        setsid();
        setenv("MTM", buf, 1);
        setenv("TERM", getterm(), 1);
        setenv("PS1", ">", 1);
        signal(SIGCHLD, SIG_DFL);
	if ( cwd != NULL ) chdir(cwd);
	//chdir("/");
	if ( exe != NULL ){
            //execl("/bin/bash","-i","-c", "nvim /etc/passwd" );
            //execl("/bin/bash","-i","-c", "nvim _log.txt" );
            execl("/bin/bash","-login", "-i","-c", exe );
	} else {
            //execl(getshell(), getshell(), NULL);
            execl("/bin/bash", NULL);
	}
        //n->pid = getpid();
        return NULL;
    } else {
	    n->pid = pid;
    }

    FD_SET(n->pt, &fds);
    fcntl(n->pt, F_SETFL, O_NONBLOCK);
    nfds = n->pt > nfds? n->pt : nfds;
    return n;
}

static NODE *
newview2(NODE *p, int y, int x, int h, int w) /* Open a new view. */
{
    struct winsize ws = {.ws_row = h, .ws_col = w};
    NODE *n = newnode(VIEW, p, y, x, h, w);
    if (!n)
        return NULL;

    SCRN *pri = &n->pri, *alt = &n->alt;
    pri->win = newpad(MAX(h, SCROLLBACK), w);
    alt->win = newpad(h, w);
    if (!pri->win || !alt->win)
        return freenode(n, false), NULL;
    pri->tos = pri->off = MAX(0, SCROLLBACK - h);
    n->s = pri;

    nodelay(pri->win, TRUE); nodelay(alt->win, TRUE);
    scrollok(pri->win, TRUE); scrollok(alt->win, TRUE);
    keypad(pri->win, TRUE); keypad(alt->win, TRUE);

    setupevents(n);
    ris(&n->vp, n, L'c', 0, 0, NULL, NULL);

    pid_t pid = forkpty(&n->pt, NULL, NULL, &ws);
    if (pid < 0){
        if (!p)
            perror("forkpty");
        return freenode(n, false), NULL;
    } else if (pid == 0){
        char buf[100] = {0};
        snprintf(buf, sizeof(buf) - 1, "%lu", (unsigned long)getppid());
        setsid();
        setenv("MTM", buf, 1);
        setenv("TERM", getterm(), 1);
        setenv("PS1", ">", 1);
        signal(SIGCHLD, SIG_DFL);
        execl(getshell(), getshell(), NULL);
        //n->pid = getpid();
        return NULL;
    } else {
	    n->pid = pid;
    }

    FD_SET(n->pt, &fds);
    fcntl(n->pt, F_SETFL, O_NONBLOCK);
    //nfds = n->pt > nfds? n->pt : nfds;
    return n;
}
static NODE *
newcontainer(Node t, NODE *p, int y, int x, int h, int w,
             NODE *c1, NODE *c2) /* Create a new container */
{
    NODE *n = newnode(t, p, y, x, h, w);
    if (!n)
        return NULL;

    n->c1 = c1;
    n->c2 = c2;
    c1->p = c2->p = n;

    reshapechildren(n);
    return n;
}

static void
command_shell(NODE *n);
       
static void
focus(NODE *n) /* Focus a node. */
{
    if (!n)
        return;
    else if (n->t == VIEW){
        t_lastfocused[t_root_index] = t_focused[t_root_index];
        t_focused[t_root_index] = n;
    } else
        focus(n->c1? n->c1 : n->c2);

}

//static void
//focus_old(NODE *n) /* Focus a node. */
//{
//    if (!n)
//        return;
//    else if (n->t == VIEW){
//        lastfocused = focused;
//        focused = n;
//    } else
//        focus(n->c1? n->c1 : n->c2);
//
//}

#define BACKWARD(n) ( n->p ? (n->p->c1 == n ? n->p->p : n->p->c1) : n )
#define FORWARD(n)  ( n->p ? (n->p->c2 ? n->p->c2 : n->c2) : n )

#define ABOVE(n) n->y - 2, n->x + n->w / 2
#define BELOW(n) n->y + n->h + 2, n->x + n->w / 2
#define LEFT(n)  n->y + n->h / 2, n->x - 2
#define RIGHT(n) n->y + n->h / 2, n->x + n->w + 2

static NODE *
findnode(NODE *n, int y, int x) /* Find the node enclosing y,x. */
{
    #define IN(n, y, x) (y >= n->y && y <= n->y + n->h && \
                         x >= n->x && x <= n->x + n->w)
    if (IN(n, y, x)){
        if (n->c1 && IN(n->c1, y, x))
            return findnode(n->c1, y, x);
        if (n->c2 && IN(n->c2, y, x))
            return findnode(n->c2, y, x);
        return n;
    }
    return NULL;
}

static void
replacechild(NODE *n, NODE *c1, NODE *c2) /* Replace c1 of n with c2. */
{
    c2->p = n;
    if (!n){
        c2->type = ROOT;
        t_root[t_root_index] = c2;
        reshape(c2, 0, 0, LINES-1, COLS);
    } else if (n->c1 == c1)
        n->c1 = c2;
    else if (n->c2 == c1)
        n->c2 = c2;

    n = n? n : t_root[t_root_index];
    reshape(n, n->y, n->x, n->h, n->w);
    draw(n);
}

static void
removechild(NODE *p, const NODE *c) /* Replace p with other child. */
{
    replacechild(p->p, p, c == p->c1? p->c2 : p->c1);
    freenode(p, false);
}


static void
root_delete(NODE *n)
{
    if (n->isfetch) return;

    for ( int i =0 ; i < PANE_MAX ; i++) {
        if (t_root[i] == n)
	{
	    t_root[i] = NULL;
	    t_root_enable[i] = 0;
	    return;
	}
    }
}

static bool
root_empty()
{
    int cnt = 0;
    for ( int i =0 ; i < PANE_MAX ; i++) {
	{
	    if (t_root_enable[i] == 1)
	    {
		    cnt++;
	    }
	}
    }

    if ( cnt > 0 ) {
	    return false;
    } else {
	    return true;
    }
}

static int
root_num()
{
    int cnt = 0;
    for ( int i =0 ; i < PANE_MAX ; i++) {
	{
	    if (t_root_enable[i] == 1)
	    {
		    cnt++;
	    }
	}
    }
    return cnt;
}

static bool
set_create_root_index()
{
    for ( int n =0 ; n < PANE_MAX ; n++) {
        if (t_root_enable[n] == 0)
	{
	    t_root_index = n;
	    return true;
	}
    }
  return false;
}

static void
set_next_root_index()
{
    int c_index = t_root_index;

    for ( int n = c_index + 1 ; n < PANE_MAX ; n++) {
        if (t_root_enable[n] == 1)
	{
	    t_root_index = n;
	    return;
	}
    }
    for ( int n = 0 ; n < c_index  ; n++) {
        if (t_root_enable[n] == 1)
	{
	    t_root_index = n;
	    return;
	}
    }
}

static void
set_prev_root_index()
{
    int c_index = t_root_index;

    for ( int n = c_index - 1 ; n >= 0 ; n--) {
        if (t_root_enable[n] == 1)
	{
	    t_root_index = n;
	    return;
	}
    }
    for ( int n = PANE_MAX ; n > c_index  ; n--) {
        if (t_root_enable[n] == 1)
	{
	    t_root_index = n;
	    return;
	}
    }
}


static void
deletenode(NODE *n) /* Delete a node. */
{
    if (n->isfetch) return;
    //FD_CLR(n->pt, &fds);
    if (n->type == ROOT)
    {
         FD_CLR(n->pt, &fds);
	 if ( n->p == NULL && n->c1 == NULL && n->c2 == NULL)
	 //if ( n->c1 == NULL && n->c2 == NULL)
	 {

	       root_delete(n);
	       if (root_empty()) {
                   quit(EXIT_SUCCESS, NULL);
	       } else {
                  t_root_change = 1;
                  t_root_change_type = NEXT;
	       }
	 } else {
               //if (n == focused)
               if (n == t_focused[t_root_index])
                   focus(n->p->c1 == n? n->p->c2 : n->p->c1);
               removechild(n->p, n);
               freenode(n, true);
	 }
	 
    //} else if (n->type == EXPANDROOT) {
    } else if (n->type == EXPANDROOT) {
	    
	       n->refcnt--;
	       n->expand = false;
	       root_delete(n);
	       if (root_empty()) {
                   quit(EXIT_SUCCESS, NULL);
	       } else {
                  t_root_change = 1;
                  t_root_change_type = NEXT;
		  n->type = CHILD;
	       }
	       
    } else {
         FD_CLR(n->pt, &fds);
         //if (!n || !n->p)
         //    quit(EXIT_SUCCESS, NULL);
         //if (n == focused)
         if (n == t_focused[t_root_index])
             focus(n->p->c1 == n? n->p->c2 : n->p->c1);
         removechild(n->p, n);
         freenode(n, true);
    }
}

static void
swapnode(NODE *n) /* Swap a node. */
{
     NODE *p = n->p;
     if (n == p->c1 ){
	     NODE *tmp = p->c1;
	     p->c1 = p->c2;
	     p->c2 = tmp;

     } else if ( n == p->c2) {
	     NODE *tmp = p->c2;
	     p->c2 = p->c1;
	     p->c1 = tmp;
     }
     t_root_change = 1;
     t_root_change_type = SWAP;

}
static void
reshapeview(NODE *n, int d, int ow) /* Reshape a view. */
{
    int oy, ox;
    bool *tabs = newtabs(n->w, ow, n->tabs);
    struct winsize ws = {.ws_row = n->h, .ws_col = n->w};

    if (tabs){
        free(n->tabs);
        n->tabs = tabs;
        n->ntabs = n->w;
    }

    getyx(n->s->win, oy, ox);
    wresize(n->pri.win, MAX(n->h, SCROLLBACK), MAX(n->w, 2));
    wresize(n->alt.win, MAX(n->h, 2), MAX(n->w, 2));
    n->pri.tos = n->pri.off = MAX(0, SCROLLBACK - n->h);
    n->alt.tos = n->alt.off = 0;
    wsetscrreg(n->pri.win, 0, MAX(SCROLLBACK, n->h) - 1);
    wsetscrreg(n->alt.win, 0, n->h - 1);
    if (d > 0){ /* make sure the new top line syncs up after reshape */
        wmove(n->s->win, oy + d, ox);
        wscrl(n->s->win, -d);
    }
    doupdate();
    refresh();
    ioctl(n->pt, TIOCSWINSZ, &ws);
}

static void
reshapechildren_ups(NODE *n) /* Reshape all children of a view. */
{
    if (n->t == HORIZONTAL){
        int i = n->w % 2? 0 : 1;
        //reshape(n->c1, n->y, n->x, n->h, n->w / 2);
        reshape(n->c1, n->y + 1, n->x, n->h -1, n->w / 2);
        //reshape(n->c2, n->y, n->x + n->w / 2 + 1, n->h, n->w / 2 - i);
        reshape(n->c2, n->y + 1, n->x + n->w / 2 + 1, n->h -1, n->w / 2 - i);
    } else if (n->t == VERTICAL){
        int i = n->h % 2? 0 : 1;
        //reshape(n->c1, n->y, n->x, n->h / 2, n->w);
        reshape(n->c1, n->y +1, n->x, (n->h / 2) -1, n->w);
        //reshape(n->c2, n->y + n->h / 2 + 1, n->x, n->h / 2 - i, n->w);
        reshape(n->c2, n->y + n->h / 2 + 1 + 1, n->x, n->h / 2 - i-1, n->w);
    }
}

static void
reshapechildren(NODE *n) /* Reshape all children of a view. */
{
    if (isfullscreen) {
        reshape(n->c1, 0, 0, LINES, COLS);
        reshape(n->c2, 0, 0, LINES, COLS);
    } else {
        if (n->t == HORIZONTAL){
            int i = n->w % 2? 0 : 1;
            reshape(n->c1, n->y, n->x, n->h, n->w / 2);
            reshape(n->c2, n->y, n->x + n->w / 2 + 1, n->h, n->w / 2 - i);
        } else if (n->t == VERTICAL){
            int i = n->h % 2? 0 : 1;
            reshape(n->c1, n->y, n->x, n->h / 2, n->w);
            reshape(n->c2, n->y + n->h / 2 + 1, n->x, n->h / 2 - i, n->w);
        }
    }
}

static void
reshapechildren_old(NODE *n) /* Reshape all children of a view. */
{
    if (n->t == HORIZONTAL){
        int i = n->w % 2? 0 : 1;
        reshape(n->c1, n->y, n->x, n->h, n->w / 2);
        reshape(n->c2, n->y, n->x + n->w / 2 + 1, n->h, n->w / 2 - i);
    } else if (n->t == VERTICAL){
        int i = n->h % 2? 0 : 1;
        reshape(n->c1, n->y, n->x, n->h / 2, n->w);
        reshape(n->c2, n->y + n->h / 2 + 1, n->x, n->h / 2 - i, n->w);
    }
}
static void  status_bar();
static void  status_bar_clear();
static void
reshape(NODE *n, int y, int x, int h, int w) /* Reshape a node. */
{
    if (n->y == y && n->x == x && n->h == h && n->w == w && n->t == VIEW)
        return;

    int d = n->h - h;
    int ow = n->w;
    n->y = y;
    n->x = x;
    n->h = MAX(h, 1);
    n->w = MAX(w, 1);

    if (isfullscreen) {
       n->h = LINES;
       n->w = COLS;
    }

    if (n->t == VIEW)
        reshapeview(n, d, ow);
    else
        reshapechildren(n);
    draw(n);
    //status_bar();
}

static void
drawchildren(const NODE *n) /* Draw all children of n. */
{
    draw(n->c1);
    if(!isfullscreen) {
        attron(COLOR_PAIR(LINE_PAIR));
        if (n->t == HORIZONTAL)
            mvvline(n->y, n->x + n->w / 2, ACS_VLINE, n->h);
        else
            mvhline(n->y + n->h / 2, n->x, ACS_HLINE, n->w);
    
        attroff(COLOR_PAIR(LINE_PAIR));
        wnoutrefresh(stdscr);
    }
    draw(n->c2);
}

static void
draw(NODE *n) /* Draw a node. */
{
    if (n->t == VIEW) {
        //if (isfullscreen && n != focused)
        if (isfullscreen && n != t_focused[t_root_index])
		            return;
        if (n->isfetch) {
	    //init_pair(2, COLOR_RED, COLOR_GREEN);
            //PANEL *panel = new_panel(n->s->win);
	    //update_panels();

        }
        pnoutrefresh(n->s->win, n->s->off, 0, n->y, n->x,
                     n->y + n->h - 1, n->x + n->w - 1);

    } else {
        drawchildren(n);
    }
}


static void
//split(NODE *n, Node t, ...) /* Split a node. */
split(NODE *n, Node t,char *cwd, char *exe) /* Split a node. */
{
	/*
    char *cwd = NULL;
    char *exe = NULL;

    va_list args;
    va_start( args, t );
    cwd = va_arg( args, char* );
    exe = va_arg( args, char* );
    va_end( args );
*/
    if (n->isfetch) return;

    n->type = CHILD;
    int nh = t == VERTICAL? (n->h - 1) / 2 : n->h;
    int nw = t == HORIZONTAL? (n->w) / 2 : n->w;

    if (isfullscreen) {
	            nh = LINES;
		    nw = COLS;
    }




    NODE *p = n->p;
/*
    NODE *v ;
    if ( cwd != NULL) {
       if ( exe != NULL) {
        v = newview(NULL, 0, 0, MAX(0, nh), MAX(0, nw), cwd, exe);
       } else {
        v = newview(NULL, 0, 0, MAX(0, nh), MAX(0, nw), cwd);
       }
    } else {
        v = newview(NULL, 0, 0, MAX(0, nh), MAX(0, nw));
    }
    */
    NODE *v = newview(NULL, 0, 0, MAX(0, nh), MAX(0, nw), cwd, exe);

    if (!v)
        return;

    //
    //     c--------n
    //        |
    //        +-----v
    //
    NODE *c = newcontainer(t, n->p, n->y, n->x, n->h, n->w, n, v);
    if (!c){
        freenode(v, false);
        return;
    }

    replacechild(p, n, c);
    focus(v);
    draw(p? p : t_root[t_root_index]);
}

/*
static NODE *
clonenode(NODE *p);
static NODE *
clonenode2(NODE *p, int y, int x, int h, int w);

static void
view_fork(NODE *n, Node t) 
{
    int nh = t == VERTICAL? (n->h - 1) / 2 : n->h;
    int nw = t == HORIZONTAL? (n->w) / 2 : n->w;
    NODE *p = n->p;
    //NODE *v = newview(NULL, 0, 0, MAX(0, nh), MAX(0, nw));
    //if (!v)
    //    return;

    //NODE *c = newcontainer(t, n->p, n->y, n->x, n->h, n->w, n, v);
    NODE *z = clonenode(n);
    NODE *z2 = clonenode2(NULL, 0, 0, MAX(0, nh), MAX(0, nw));
    NODE *c = newcontainer(t, n->p, n->y, n->x, n->h, n->w, n, z);
    if (!c){
        //freenode(v, false);
        return;
    }
    n->refcnt++;

    replacechild(p, n, c);
    //focus(v);
    draw(p? p : t_root[t_root_index]);
}
*/



static void
togglefullscreen() {
    isfullscreen = !isfullscreen;
    //reshape(root, 0, 0, LINES, COLS);
    reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
}

static bool
getinput(NODE *n, fd_set *f) /* Recursively check all ptty's for input. */
{
    if (n && n->c1 && !getinput(n->c1, f))
        return false;

    if (n && n->c2 && !getinput(n->c2, f))
        return false;

    if (n && n->t == VIEW && n->pt > 0 && FD_ISSET(n->pt, f)){
        ssize_t r = read(n->pt, iobuf, sizeof(iobuf));
        if (r > 0){
            vtwrite(&n->vp, iobuf, r);
	}
        if (r <= 0 && errno != EINTR && errno != EWOULDBLOCK)
	{
            status_bar_clear();
            //FD_CLR(n->pt, &fds);
            return deletenode(n), false;
	}
    }
   status_bar_clear();
   return true;
}

static void
scrollback(NODE *n)
{
    n->s->off = MAX(0, n->s->off - n->h / 2);
}

static void
scrollforward(NODE *n)
{
    n->s->off = MIN(n->s->tos, n->s->off + n->h / 2);
}

static void
scrollbottom(NODE *n)
{
    n->s->off = n->s->tos;
}

static void
sendarrow(const NODE *n, const char *k)
{
    char buf[100] = {0};
    snprintf(buf, sizeof(buf) - 1, "\033%s%s", n->pnm? "O" : "[", k);
    SEND(n, buf);
}

static void
create_pane()
{
  t_root_change = 1;
  t_root_change_type = CREATE;
}
static void
restore_pane()
{
  t_root_change = 1;
  t_root_change_type = RESTORE;
}
static void
next_pane()
{
  t_root_change = 1;
  t_root_change_type = NEXT;
}

static void
prev_pane()
{
  t_root_change = 1;
  t_root_change_type = PREV;
}

static void
toggle_window_label()
{
   window_label_show = !window_label_show;
   reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
   //draw(t_root[t_root_index]);
  t_root_change = 1;
  t_root_change_type = TOGGLE_LABEL;
}

static void
window_label_edit(NODE *n)
{
char *string = lineedit();

  if (string[0] == '$') {
   strcpy(n->label_buf, string);
   t_root_change = 1;
   t_root_change_type = TOGGLE_LABEL;
  } else {
   SEND(n,"\n");
   SEND(n,string);
   SEND(n,"\n");
  }

}


static void
node_save(NODE *n, FILE *fp, int lv)
{
    char indent_format[256];
    char indent[256];

    char nodetype[256];
    char viewtype[256];
    char label[256];
    char pid[24];
    char proc_cwd_path[256];
    char proc_exe_path[256];
    char cwd[256];
    char exe[256];

    char *proc_cwd_path_format = "/proc/%d/cwd";
    char *proc_exe_path_format = "/proc/%d/exe";

    char output_param[256];
    char output[256];

    lv++;

    if (n->type != EXPANDROOT)
    {
          switch (n->type) 
	  {
		  case ROOT:
			  sprintf(nodetype,"%s", "ROOT");
			  break;
                  case CHILD:
			  sprintf(nodetype,"%s", "CHILD");
			  break;
		  default:
			  sprintf(nodetype,"%s", "-");
			  break;
	  }
          switch (n->t) 
	  {
		  case VIEW:
			  sprintf(viewtype,"%s", "VIEW");
			  break;
                  case HORIZONTAL:
			  sprintf(viewtype,"%s", "HORIZONTAL");
			  break;
                  case VERTICAL:
			  sprintf(viewtype,"%s", "VERTICAL");
			  break;
		  default:
			  sprintf(viewtype,"%s", "-");
			  break;
	  }

             sprintf(indent_format,"%s%d%s","%",lv * 4 ,"s");
	     sprintf(indent, indent_format, " ");
	     sprintf(label, "\"%s\"", n->label_buf);

             if (n->pid > 0 )
	     {
	       sprintf(output_param, "%s %s %s [%d]", nodetype, viewtype, label, n->pid);
	     } else {
	       sprintf(output_param, "%s %s %s ", nodetype, viewtype, label );
	     }
	     sprintf(output, "%s%s\n", indent, output_param);
	
	     //fputs(indent,fp);
	     fputs(output,fp);

             if (n->pid > 0 )
	     {
               sprintf(proc_cwd_path, proc_cwd_path_format, n->pid);
               sprintf(proc_exe_path, proc_exe_path_format, n->pid);

	       read_symlink(proc_cwd_path, cwd);
	       read_symlink(proc_exe_path, exe);

	       sprintf(output_param, "cwd: %s", cwd);
	       sprintf(output, "%s%s\n", indent, output_param);
	       fputs(output,fp);

	       sprintf(output_param, "exe: %s", exe);
	       sprintf(output, "%s%s\n", indent, output_param);
	       fputs(output,fp);


             }
	     

	    if (n->c1) node_save(n->c1, fp, lv);
	    if (n->c2) node_save(n->c2, fp, lv);
    }

}


static void
session_save(NODE *n)
{
    char format[256];

    FILE *fp;

    fp = fopen("mtm_session.ini", "w");

    if (fp  == NULL) {
	    return;
    }


    for ( int n = 0 ; n < PANE_MAX ; n++) {
        if (t_root_enable[n] == 1)
	{
             sprintf(format,"root:%d\n",n);
	     fputs(format, fp);
	     node_save(t_root[n], fp, 0);
	}
    }

    fclose(fp);
}

static bool
is_coordinate(int y, int x, int h, int w, int mx , int my)
{

  if (mx > x && mx < (x + w) && my > y && my < ( y + h )) return true;

  return false;

}

static NODE*
coordinate_findnode_recursive(NODE *n, int x, int y)
{
    NODE *sn = 0;

    if (n->t == VIEW)
    {
        if (is_coordinate(n->y, n->x, n->h, n->w, x , y)) return n;
    }

    if (n->c1) 
    {
	    sn = coordinate_findnode_recursive(n->c1, x, y);
    }

    if (sn != 0)  return sn;

    if (n->c2) 
    {
	    sn = coordinate_findnode_recursive(n->c2, x, y);
    }

    if (sn != 0)  return sn;

  return 0;
}


static NODE* 
coordinate_findnode(int x, int y)
{
  return coordinate_findnode_recursive(t_root[t_root_index], x ,y);
}

static void
command_shell(NODE *n)
{
/*
  const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
       SEND(n,cur_top_left);
       SEND(n,clear_line);
*/
  //SEND(n, "\033[0\;0H");
  //wmove(n->s->win,0,0);
  //winsertln(n->s->win);
   // wrefresh();
   // int wmove (WINDOW *win, int y, int x);
   //
   
  int x;
  int y;
  getyx(n->s->win,x, y);
  wmove(n->s->win,x,0);
  wclrtoeol(n->s->win);
  wrefresh(n->s->win);
  
  draw(n);
  doupdate();

  n->command_shell_loop = true;
  char *string = lineedit();
  if (string[0] == '.') {
       n->command_shell_loop = false;
  } else if (string[0] == '$') {
       int len = strlen(string);
       memcpy(n->label_buf, &string[1],len -1 );
       n->label_buf[len -1] = '\0';
       t_root_change = 1;
       t_root_change_type = TOGGLE_LABEL;
       window_label_show = true;

  } else {
       SEND(n,"\n");
       SEND(n,string);
       SEND(n,"\n");
  }

}
static void
expandnode(NODE *n) /* Expand a node. */
{
  n->refcnt++;
  n->expand = true;
  t_root_change = 1;
  t_root_change_type = EXPAND;
  t_expand_node = n;

}

NODE * get_root(NODE *n)
{
	if ( n->type == ROOT) {
                LOG_PRINT("ROOT\n");
		return n;
	} else if (n->type == EXPANDROOT) {
                LOG_PRINT("EXPANDROOT\n");
		return n;
	} else if (n->type == CHILD) {
                LOG_PRINT("CHILD\n");
		return get_root(n->p);
        } else {
                LOG_PRINT("other\n");
		return n;
	}

}

static  void
dump_node(NODE *n, int level) {
        //char  level_str[32];
        char  indent_fmt[32];
        char  indent[32];
	sprintf(indent_fmt, "%s%d%s", "%",level*4, "s");
	sprintf(indent, indent_fmt, "|");
	level += 1;

	if ( n->type == ROOT) {
                LOG_PRINT("%s ROOT\n", indent);
		//return n;
	} else if (n->type == EXPANDROOT) {
                LOG_PRINT("%s EXPANDROOT\n"), indent;
		//return n;
	} else if (n->type == CHILD) {
                LOG_PRINT("%s CHILD\n", indent);
		//return get_root(n->p);
        } else {
                LOG_PRINT("%s other\n", indent);
		//return n;
	}

	if (n->c1 !=NULL) {
		dump_node(n->c1, level);
	}
	if (n->c2 !=NULL) {
		dump_node(n->c2, level);
	}
}

static  void
dump_tree() {
    for ( int i =0 ; i < PANE_MAX ; i++) {
        if (t_root_enable[i] > 0)
	{
            LOG_PRINT("index :%d\n", i);
            //LOG_PRINT("root type :%d\n", t_root[i]->type);
            dump_node(t_root[i], 1);


	}
    }
}

static 
int get_root_index(NODE *n) /* Expand a node. */
{
    NODE * root = get_root(n);
    int i = 0;
    for ( int i =0 ; i < PANE_MAX ; i++) {
        if (t_root[i] == root)
	{
	    return i;
	}
    }
        return -1;
}

static void
pushnode(NODE *n) /* Push a node. */
{
  LOG_PRINT("push node  \n");
    push_node = n;
}

static void
fetchnode(NODE *n) /* Fetch a node. */
{

  LOG_PRINT("fetch node  \n");
  int root_index =  get_root_index(n);
  LOG_PRINT("root index: %d\n", root_index);

  if (push_node == NULL) return;

  if (n->type == CHILD) {
     FD_CLR(n->pt, &fds);
     NODE *p = n->p;
     push_node->isfetch = true;
   
     if (p->c1 == n) {
   	 //save_node = p->c1;
   	 //save_node_cp = &(p->c1);
   	 push_node->fetch_save_node = p->c1;
   	 push_node->fetch_save_node_cp = &(p->c1);
   	 p->c1 = push_node;
	 //p->c1_isfetch = true;
   
     } else if (p->c2 == n) {
   	 //save_node = p->c2;
   	 //save_node_cp = &(p->c2);
   	 push_node->fetch_save_node = p->c2;
   	 push_node->fetch_save_node_cp = &(p->c2);
   	 p->c2 = push_node;
	 //p->c2_isfetch = true;
   
     }
  } else if (n->type == ROOT) {
     LOG_PRINT("root is uneble fetch  \n");
  }
  t_root_change = 1;
  t_root_change_type = FETCH;
}

static void
unfetchnode(NODE *n) /* UnFetch a node. */
{
  n->isfetch = 0;
  if (n->type == CHILD) {
     //FD_SET(save_node->pt, &fds);
     FD_SET(n->fetch_save_node->pt, &fds);
     //NODE *p = save_node->p;
     NODE *p = n->fetch_save_node->p;
   
     //p->c1 = save_node;
     //*save_node_cp = save_node;
     *(n->fetch_save_node_cp) = n->fetch_save_node;
   
   
  } else if (n->type == ROOT) {
     LOG_PRINT("root is uneble fetch  \n");
  }
  t_root_change = 1;
  t_root_change_type = FETCH;

}

//static void
//fetchnode_old(NODE *n) /* Fetch a node. */
//{
//
//  LOG_PRINT("fetch node  \n");
//  int root_index =  get_root_index(n);
//  LOG_PRINT("root index: %d\n", root_index);
//
//  if (push_node == NULL) return;
//
//  if (n->type == CHILD) {
//     FD_CLR(n->pt, &fds);
//     NODE *p = n->p;
//   
//     if (p->c1 == n) {
//   	 save_node = p->c1;
//   	 p->c1 = push_node;
//   
//   
//     } else if (p->c2 == n) {
//   	 save_node = p->c2;
//   	 p->c2 = push_node;
//   
//  }
//  } else if (n->type == ROOT) {
//     LOG_PRINT("root is uneble fetch  \n");
//
//
//  }
//
//	/*
//  n->refcnt++;
//  n->expand = true;
//  t_root_change = 1;
//  t_root_change_type = EXPAND;
//  t_expand_node = n;
//*/
//
//  t_root_change = 1;
//  t_root_change_type = FETCH;
//}

static void
mouse_focus_change() 
{
  if (t_mouse_focus) {
     t_mouse_focus = false;
     mousemask(0, NULL);
  } else {
     t_mouse_focus = true;
     mousemask(BUTTON1_CLICKED, NULL);
  }

}

static void
mouse_event(NODE *n) 
{
   MEVENT e;
   if (getmouse(&e) == OK) {
	   t_mouse_x = e.x;
	   t_mouse_y = e.y;
   }

   NODE *cn = coordinate_findnode(e.x, e.y);
   if (cn) {
       focus(cn);
   }
   //focus(t_root[t_root_index]);
   
   //ungetmouse(&e);
}

static void 
app_exit() {

  quit(EXIT_SUCCESS, NULL);
  exit(0);

}


static bool
handlechar(int r, int k) /* Handle a single input character. */
{
    const char cmdstr[] = {commandkey, 0};
    static bool cmd = false;
    //NODE *n = focused;
    NODE *n = t_focused[t_root_index];
    #define KERR(i) (r == ERR && (i) == k)
    #define KEY(i)  (r == OK  && (i) == k)
    #define CODE(i) (r == KEY_CODE_YES && (i) == k)
    #define INSCR (n->s->tos != n->s->off)
    #define SB scrollbottom(n)
    #define DO(s, t, a) \
        if (s == cmd && (t)) { a ; cmd = false; return true; }

    DO(cmd,   KERR(k),             return false)
    DO(cmd,   CODE(KEY_RESIZE),    reshape(t_root[t_root_index], 0, 0, LINES-1, COLS); SB)
    DO(false, KEY(commandkey),     return cmd = true)
    DO(false, KEY(0),              SENDN(n, "\000", 1); SB)
    DO(false, KEY(L'\n'),          SEND(n, "\n"); SB)
    DO(false, KEY(L'\r'),          SEND(n, n->lnm? "\r\n" : "\r"); SB)
    DO(false, PANE_NEXT,           next_pane())
    DO(false, PANE_PREV,           prev_pane())
    DO(false, SCROLLUP && INSCR,   scrollback(n))
    DO(false, SCROLLDOWN && INSCR, scrollforward(n))
    DO(false, RECENTER && INSCR,   scrollbottom(n))
    DO(false, CODE(KEY_ENTER),     SEND(n, n->lnm? "\r\n" : "\r"); SB)
    DO(false, CODE(KEY_UP),        sendarrow(n, "A"); SB);
    DO(false, CODE(KEY_DOWN),      sendarrow(n, "B"); SB);
    DO(false, CODE(KEY_RIGHT),     sendarrow(n, "C"); SB);
    DO(false, CODE(KEY_LEFT),      sendarrow(n, "D"); SB);
    DO(false, CODE(KEY_HOME),      toggle_window_label())
    //DO(false, CODE(KEY_MOUSE),     mouse_event(n);SENDN(n, r, 1))
    DO(false, CODE(KEY_MOUSE),     mouse_event(n))
    //DO(false, CODE(KEY_HOME),      SEND(n, "\033[1~"); SB)
    DO(false, CODE(KEY_END),       SEND(n, "\033[4~"); SB)
    DO(false, CODE(KEY_PPAGE),     SEND(n, "\033[5~"); SB)
    DO(false, CODE(KEY_NPAGE),     SEND(n, "\033[6~"); SB)
    DO(false, CODE(KEY_BACKSPACE), SEND(n, "\177"); SB)
    DO(false, CODE(KEY_DC),        SEND(n, "\033[3~"); SB)
    DO(false, CODE(KEY_IC),        SEND(n, "\033[2~"); SB)
    //DO(false, CODE(KEY_BTAB),      SEND(n, "\033[Z"); SB)
    DO(false, CODE(KEY_BTAB),      command_shell(n))
    //DO(false, CODE(KEY_F(2)),      mouse_focus_change())
    //DO(false, CODE(KEY_F(1)),      SEND(n, "\033OP"); SB)
    //DO(false, CODE(KEY_F(2)),      SEND(n, "\033OQ"); SB)
    DO(false, CODE(KEY_F(3)),      SEND(n, "\033OR"); SB)
    DO(false, CODE(KEY_F(4)),      SEND(n, "\033OS"); SB)
    DO(false, CODE(KEY_F(1)),      mouse_focus_change())
    //DO(false, CODE(KEY_F(5)),      SEND(n, "\033[15~"); SB)
    DO(false, CODE(KEY_F(6)),      SEND(n, "\033[17~"); SB)
    DO(false, CODE(KEY_F(7)),      SEND(n, "\033[18~"); SB)
    DO(false, CODE(KEY_F(8)),      SEND(n, "\033[19~"); SB)
    DO(false, CODE(KEY_F(9)),      SEND(n, "\033[20~"); SB)
    DO(false, CODE(KEY_F(10)),     SEND(n, "\033[21~"); SB)
    DO(false, CODE(KEY_F(11)),     SEND(n, "\033[23~"); SB)
    DO(false, CODE(KEY_F(12)),     SEND(n, "\033[24~"); SB)
    DO(true,  QUIT,                app_exit())
    DO(true,  MOVE_UP,             focus(findnode(t_root[t_root_index], ABOVE(n))))
    DO(true,  MOVE_DOWN,           focus(findnode(t_root[t_root_index], BELOW(n))))
    DO(true,  MOVE_LEFT,           focus(findnode(t_root[t_root_index], LEFT(n))))
    DO(true,  MOVE_RIGHT,          focus(findnode(t_root[t_root_index], RIGHT(n))))
    //DO(true,  MOVE_OTHER,          focus(lastfocused))
    DO(true,  MOVE_OTHER,          focus(t_lastfocused[t_root_index]))
    DO(true,  HSPLIT,              split(n, HORIZONTAL,NULL,NULL))
    DO(true,  VSPLIT,              split(n, VERTICAL,NULL,NULL))
//    DO(true,  HFORK,               view_fork(n, HORIZONTAL))
//    DO(true,  VFORK,               view_fork(n, VERTICAL))
    DO(true,  DUMP_TREE,         dump_tree())
    DO(true,  FULLSCREEN,          togglefullscreen())
    DO(true,  DELETE_NODE,         deletenode(n))
    DO(true,  SWAP_NODE,         swapnode(n))
    DO(true,  FETCH_NODE,         fetchnode(n))
    DO(true,  UNFETCH_NODE,         unfetchnode(n))
    DO(true,  PUSH_NODE,         pushnode(n))
    DO(true,  EXPAND_NODE,         expandnode(n))
    DO(true,  REDRAW,              touchwin(stdscr); draw(t_root[t_root_index]); redrawwin(stdscr))
    DO(true,  CREATE_PANE,         create_pane())
    DO(true,  RESTORE_PANE,        restore_pane())
    DO(true,  NEXT_PANE,           next_pane())
    DO(true,  PANE_NEXT,           next_pane())
    DO(true,  PANE_PREV,           prev_pane())
    DO(true,  WINDOW_LABEL,        toggle_window_label())
    DO(true,  WINDOW_LABEL_EDIT,   window_label_edit(n))
    DO(true,  COMMAND_SHELL,       command_shell(n))
    DO(true,  SESSION_SAVE,       session_save(n))
    DO(true,  SCROLLUP,            scrollback(n))
    DO(true,  SCROLLDOWN,          scrollforward(n))
    DO(true,  RECENTER,            scrollbottom(n))
    DO(true,  KEY(commandkey),     SENDN(n, cmdstr, 1));
    char c[MB_LEN_MAX + 1] = {0};
    if (wctomb(c, k) > 0){
        scrollbottom(n);
        SEND(n, c);
    }
    return cmd = false, true;
}

static void
status_bar_clear()
{
   //const char *cur_pos_save    = "\033[s";
  const char *cur_pos_save    = "\0337";
  //const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
  //const char *cur_pos_restore = "\033[u";
  const char *cur_pos_restore = "\0338";

  char cur_set[256];
  sprintf(cur_set ,"\033[%d;%dH", LINES, 0 );

  int out = 0;
  safewrite(out, cur_pos_save,    strlen(cur_pos_save));
  safewrite(out, cur_set,         strlen(cur_set));
  safewrite(out, clear_line,      strlen(clear_line));
  safewrite(out, cur_pos_restore, strlen(cur_pos_restore));

}

/*
static void
status_bar_()
{
  char cur_set[256];
  char format[256];
  char format_expandroot[256];
  char status[256];
  char left_string[256];

   //const char *cur_pos_save    = "\033[s";
  const char *cur_pos_save    = "\0337";
  //const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
  //const char *cur_pos_restore = "\033[u";
  const char *cur_pos_restore = "\0338";

  //const char *left_string  = "LEFT TEST";
  sprintf(left_string ,"LEFT_TEST [%d/%d]", t_root_index, root_num() );
  const char *right_string = "RIGHT TEST";

  int left_len   = strlen( left_string);
  int right_len  = strlen( right_string);
  // https://www.mm2d.net/main/prog/c/console-02.html
  
  sprintf(cur_set ,"\033[%d;%dH", LINES, 0 );
  
  sprintf(format  ,"%s%d%s%d%s","\033[30m\033[44m%-", left_len, "s%", COLS - left_len ,"s\033[0m");
  sprintf(format_expandroot  ,"%s%d%s%d%s","\033[30m\033[43m%-", left_len, "s%", COLS - left_len ,"s\033[0m");


  if (t_root[t_root_index]->type == EXPANDROOT) {
     sprintf(status  ,format_expandroot, left_string, right_string);
  } else {
     sprintf(status  ,format, left_string, right_string);
  }

  int out = 0;
  safewrite(out, cur_pos_save,    strlen(cur_pos_save));
  safewrite(out, cur_set,         strlen(cur_set));
  //safewrite(out, clear_line,      strlen(clear_line));
  safewrite(out, status,      strlen(status));
  safewrite(out, cur_pos_restore, strlen(cur_pos_restore));

}
*/


static void
window_label(NODE *n);
static void
//draw_label(int x, int y, int color, char *string);
draw_label(int x, int y, int color, char *string, char *label_buf);
static void
window_label_clear(NODE *n);
static void
draw_label_clear(int x, int y, int color, char *string);

static void
status_bar()
{
  char cur_set[256];
  char format[256];
  char format_expandroot[256];
  char status[256];
  char left_string[256];
  char right_string[256];
  char right_string2[256];
  char pane_led[256];
  char *focused_node_type;

   //const char *cur_pos_save    = "\033[s";
  const char *cur_pos_save    = "\0337";
  //const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
  //const char *cur_pos_restore = "\033[u";
  const char *cur_pos_restore = "\0338";

  //const char *left_string  = "LEFT TEST";
  sprintf(left_string ,"LEFT_ [%d/%d]  %d,%d", t_root_index, root_num() , t_mouse_x, t_mouse_y);
                                                                       /*
									*   ---------------> x
									*   |
									*   |
									*    y
									*/

  int max = root_num();

  for ( int x = 0; x < max ; x++) {
        if ( x == t_root_index) {
            pane_led[x] ='T';
	} else {
            pane_led[x] ='.';
	}
  }
  pane_led[max] ='\0';


  //switch(focused->type){
  switch(t_focused[t_root_index]->type){
	  case ROOT:       focused_node_type ="ROOT"; break;
	  case CHILD:      focused_node_type ="CHILD"; break;
	  case EXPANDROOT: focused_node_type ="EXPANDROOT"; break;
	  default:         focused_node_type ="NONE";
  }
  //const char *right_string = "RIGHT TEST";
/*
  if (window_label_show) {
     sprintf(right_string ,"%s * %s", focused_node_type , pane_led);
  } else {
     sprintf(right_string ,"%s   %s", focused_node_type , pane_led);
  }
*/
  /*
  char *t_mouse_focus_true      = "M";
  char *t_mouse_focus_false     = "-";
  char *window_label_show_true  = "L"
  char *window_label_show_false = "-"
*/
  char pin_restore_mode = ' ';
  char pin_mouse_focus  = ' ';
  char pin_window_label = ' ';

  char * fmt_r_0;
  char * fmt_w_0;
  char * fmt_m_0;
  char * fmt_r_1;
  char * fmt_w_1;
  char * fmt_m_1;

  if (t_restore_mode) {
       pin_restore_mode = 'R';
       fmt_r_1 = "\033[43m%c\033[44m";
  } else {
       pin_restore_mode = '_';
       fmt_r_0 = "%c";
  }
  if (window_label_show) {
       pin_window_label = 'L';
       fmt_w_1 = "\033[43m%c\033[44m";
  } else {
       pin_window_label = '_';
       fmt_w_0 = "%c";
  }
  if (t_mouse_focus) {
       pin_mouse_focus = 'M';
       fmt_m_1 = "\033[43m%c\033[44m";
  } else {
       pin_mouse_focus = '_';
       fmt_m_0 = "%c";
  }

  sprintf(right_string ,"%s %c%c%c %s", focused_node_type ,pin_restore_mode,
		                                           pin_mouse_focus,
							   pin_window_label,
							   pane_led);

  char string2_fmt[256];
  sprintf(string2_fmt,"%s %s%s%s %s", "%s", fmt_r_1, fmt_w_0, fmt_m_0, "%s");
  //sprintf(right_string2 ,"%s \033[43m%c\033[44m%c%c %s", focused_node_type ,pin_restore_mode,
  //sprintf(right_string2 ,"%s %c%c%c %s", focused_node_type ,pin_restore_mode,
  sprintf(right_string2 ,string2_fmt, focused_node_type ,pin_restore_mode,
		                                           pin_mouse_focus,
							   pin_window_label,
							   pane_led);

  int left_len   = strlen( left_string);
  int right_len  = strlen( right_string);
  // https://www.mm2d.net/main/prog/c/console-02.html
  
  sprintf(cur_set ,"\033[%d;%dH", LINES, 0 );
  
  sprintf(format  ,"%s%d%s%d%s","\033[30m\033[44m%-", left_len, "s%", COLS - left_len ,"s\033[0m");
  //sprintf(format  ,"%s%d%s%d%s","\033[30m\033[44m%-", left_len, "s%", COLS - left_len+right_len ,"s\033[0m");
  sprintf(format_expandroot  ,"%s%d%s%d%s","\033[30m\033[43m%-", left_len, "s%", COLS - left_len ,"s\033[0m");


  if (t_root[t_root_index]->type == EXPANDROOT) {
     sprintf(status  ,format_expandroot, left_string, right_string);
  } else {
     sprintf(status  ,format, left_string, right_string);
  }

  int out = 0;
  safewrite(out, cur_pos_save,    strlen(cur_pos_save));
  safewrite(out, cur_set,         strlen(cur_set));
  //safewrite(out, clear_line,      strlen(clear_line));
  safewrite(out, status,      strlen(status));
  safewrite(out, cur_pos_restore, strlen(cur_pos_restore));

  if (window_label_show) {
     window_label(t_root[t_root_index]);
  }

}

static void
window_status(NODE *n) ;

static bool
is_fetch_window(NODE *n);

static void
window_label(NODE *n) 
{
    int c = 4; // color

    //if ( n == focused) c = 3;
    if ( n == t_focused[t_root_index]) c = 3;

    char *push = "push";
    char *fetch = "fetch";
    char *none  = "-";
    char *buf;

    buf = (char*)malloc(sizeof(char) * 64);

    if ( n->isfetch ) {
       if ( is_fetch_window(n) ) {
           sprintf(buf,"%s  -%s", n->label_buf, fetch);
       } else {
           sprintf(buf,"%s  -%s", n->label_buf, push);
       }
    } else {
	sprintf(buf,"%s  -%s", n->label_buf, none);
    }

    if ( n->x == 0 && n->y == 0 ) {
         draw_label(n->x    , n->y     , c,"00", buf);
    } else if ( n->x == 0 && n->y != 0 ) {
         draw_label(n->x    , n->y     , c,"10", buf);
    } else if ( n->x != 0 && n->y == 0 ) {
         draw_label(n->x + 1, n->y     , c,"01", buf);
    } else {
         draw_label(n->x + 1, n->y     , c,"11", buf);
    }

    if (n->c1) window_label(n->c1);
    if (n->c2) window_label(n->c2);

}

static void
window_label_old(NODE *n) 
{
    int c = 4; // color

    //if ( n == focused) c = 3;
    if ( n == t_focused[t_root_index]) c = 3;

    if ( n->x == 0 && n->y == 0 ) {
         draw_label(n->x    , n->y     , c,"00", n->label_buf);
    } else if ( n->x == 0 && n->y != 0 ) {
         draw_label(n->x    , n->y     , c,"10", n->label_buf);
    } else if ( n->x != 0 && n->y == 0 ) {
         draw_label(n->x + 1, n->y     , c,"01", n->label_buf);
    } else {
         draw_label(n->x + 1, n->y     , c,"11", n->label_buf);
    }

    if (n->c1) window_label(n->c1);
    if (n->c2) window_label(n->c2);

}


static bool
is_fetch_window(NODE *n) {

   if (t_root_index == n->root_index) {
         return false;
   } else {
         return true;
   }
  //NODE *p = n->p;
 /*  
  if (n->type == CHILD) {
     //NODE *p = n->p;
     if (n->fetch_parent == NULL) return false;

     NODE *p = n->fetch_parent;
     if (p->c1 == n) {
	 return p->c1_isfetch;
   
     } else if (p->c2 == n) {
	 return p->c2_isfetch;
   
     }

  } 

  return false;
*/
}

static void
window_status(NODE *n) 
{

    //int c = 4; // color
    int c = 2; // color
    char *fetch = "fetch";
    char *none  = "-";
    char *status_buf;

    status_buf = (char*)malloc(sizeof(char) * 64);

    if ( is_fetch_window(n) ) {
        strncpy(status_buf, fetch, strlen(fetch));
	status_buf[strlen(fetch)] = '\0';
	c = 1;
    } else {
        strncpy(status_buf, none, strlen(none));
	status_buf[strlen(none)] = '\0';
	c = 9;
    }

    if ( n->x == 0 && n->y == 0 ) {
         draw_label(n->x  +15  , n->y     , c,"00", status_buf);
    } else if ( n->x == 0 && n->y != 0 ) {
         draw_label(n->x  +15  , n->y     , c,"10", status_buf);
    } else if ( n->x != 0 && n->y == 0 ) {
         draw_label(n->x + 1 +15, n->y     , c,"01", status_buf);
    } else {
         draw_label(n->x + 1 +15, n->y     , c,"11", status_buf);
    }

    if (n->c1) window_status(n->c1);
    if (n->c2) window_status(n->c2);
}
static void
draw_label(int x, int y, int color, char *string, char *label_buf)
{
  char cur_set[256];
  char format[256];
  char label[256];

   //const char *cur_pos_save    = "\033[s";
  const char *cur_pos_save    = "\0337";
  //const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
  //const char *cur_pos_restore = "\033[u";
  const char *cur_pos_restore = "\0338";

  sprintf(cur_set ,"\033[%d;%dH", y, x );
  
  //sprintf(format  ,"%s","\033[30m\033[44m%s\033[0m");
  //sprintf(format  ,"%s","\033[30m\033[41m%s\033[0m");
  sprintf(format  ,"%s%d%s","\033[30m\033[4", color,"m%s\033[0m" );

  //sprintf(label  ,format, string);
  sprintf(label  ,format, label_buf);

  int out = 0;
  safewrite(out, cur_pos_save,    strlen(cur_pos_save));
  safewrite(out, cur_set,         strlen(cur_set));
  //safewrite(out, clear_line,      strlen(clear_line));

  safewrite(out, label,      strlen(label));
  //safewrite(out, string,      strlen(string));
  safewrite(out, cur_pos_restore, strlen(cur_pos_restore));

}
/*
static void
window_label_clear(NODE *n) 
{

    int c = 4; // color

    if ( n == focused) c = 3;

    if ( n->x == 0 && n->y == 0 ) {
         draw_label_clear(n->x    , n->y     , c,n->label_buf);
    } else if ( n->x == 0 && n->y != 0 ) {
         draw_label_clear(n->x    , n->y     , c,n->label_buf);
    } else if ( n->x != 0 && n->y == 0 ) {
         draw_label_clear(n->x + 1, n->y     , c,n->label_buf);
    } else {
         draw_label_clear(n->x + 1, n->y     , c,n->label_buf);
    }

    if (n->c1) window_label_clear(n->c1);
    if (n->c2) window_label_clear(n->c2);
}

static void
draw_label_clear(int x, int y, int color, char *string)
{
  char cur_set[256];
  char format[256];
  char label[256];

   //const char *cur_pos_save    = "\033[s";
  const char *cur_pos_save    = "\0337";
  //const char *cur_top_left    = "\033[0;0H";
  const char *clear_line      = "\033[2K";
  //const char *clear_box       = "\033[>3;0;0;10;3J";
  const char *clear_label       = "\033[3;0;2K";
  //const char *cur_pos_restore = "\033[u";
  const char *cur_pos_restore = "\0338";

  sprintf(cur_set ,"\033[%d;%dH", y, x );
  
  //sprintf(format  ,"%s","\033[30m\033[44m%s\033[0m");
  //sprintf(format  ,"%s","\033[30m\033[41m%s\033[0m");
  sprintf(format  ,"%s%d%s","\033[30m\033[4", color,"m%s\033[0m" );

  sprintf(label  ,format, string);

  int out = 0;
  
  safewrite(out, cur_pos_save,    strlen(cur_pos_save));
  safewrite(out, cur_set,         strlen(cur_set));
  //safewrite(out, clear_line,      strlen(clear_line));
  safewrite(out, clear_label,      strlen(clear_label));
  //safewrite(out, label,      strlen(label));
  //safewrite(out, string,      strlen(string));
  safewrite(out, cur_pos_restore, strlen(cur_pos_restore));

  //safewrite(out, clear_box, strlen(clear_box));

}

*/

static void
run(void) /* Run MTM. */
{
    t_root_change = 0;

        draw(t_root[t_root_index]);
        doupdate();
        fixcursor();
        //draw(focused);
        draw(t_focused[t_root_index]);
        window_label(t_root[t_root_index]);
        doupdate();

        status_bar();
    while (t_root[t_root_index]){
        //status_bar();
        //LOG_PRINT("mtm loop  \n" );
        wint_t w = 0;
        fd_set sfds = fds;
        if (select(nfds + 1, &sfds, NULL, NULL, NULL) < 0)
            FD_ZERO(&sfds);

        int r = wget_wch(t_focused[t_root_index]->s->win, &w);
        while (handlechar(r, w))
            r = wget_wch(t_focused[t_root_index]->s->win, &w);

	if (t_root_change) break;

        //if(!getinput(t_root[t_root_index], &sfds)) break;
        getinput(t_root[t_root_index], &sfds);

	if (t_root_change) break;

        draw(t_root[t_root_index]);
        doupdate();
        fixcursor();
        //status_bar();
        draw(t_focused[t_root_index]);
        status_bar();
        doupdate();
    }
}

static NODE*
append_pane()
{
  if (set_create_root_index())
  {
      t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS,NULL,NULL);
      t_root[t_root_index]->type = ROOT;
      t_root_enable[t_root_index] = 1;
      return t_root[t_root_index];
  }
  return 0;

}
static void
fd_set_print()
{
	char log[256];
	int i = 0;

	for ( i = 0 ; i < nfds ; i++) {
            //FD_SET(n->pt, &fds);
            if (FD_ISSET(i, &fds)) {
		    log[i] = '1';
	    } else {
		    log[i] = '0';
	    }

	}
    log[i] = '\0';
LOG_PRINT("FD_SET:%s \n",log);

}


static void dfs_parse_node(toml_table_t* curtab, int lv)
{
    int i;
    const char* key;
    const char* raw;  
    toml_array_t* arr;
    toml_table_t* tab;
    char *indent_fmt[256];
    char *indent[256];
    lv ++;
    
    sprintf(indent_fmt, "%s%d%s", "%-", lv*4 , "s");
    sprintf(indent, indent_fmt," ");
    //sprintf(indent, "%-10s"," ");

    toml_datum_t direct = toml_string_in(curtab, "direct");
    LOG_PRINT("%sdirect: %s\n", indent, direct.u.s);
    
    if ( 0 == strcmp( direct.u.s ,"VIEW") )
    {
       toml_datum_t cwd = toml_string_in(curtab, "cwd");
       toml_datum_t exe = toml_string_in(curtab, "exe");
       LOG_PRINT("%scwd: %s\n", indent,cwd.u.s);
       LOG_PRINT("%sexe: %s\n", indent,exe.u.s);
    }
    arr = toml_array_in(curtab, "CHILD");
    if (arr == 0 ) return;
    for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
        dfs_parse_node(tab , lv);
    }
}

static void build_node_tree(toml_table_t* curtab, int lv)
{
    int i;
    const char* key;
    const char* raw;  
    toml_array_t* arr;
    toml_table_t* tab;
    char *indent_fmt[256];
    char *indent[256];
    lv ++;
    
    sprintf(indent_fmt, "%s%d%s", "%-", lv*4 , "s");
    sprintf(indent, indent_fmt," ");
    //sprintf(indent, "%-10s"," ");

    toml_datum_t direct = toml_string_in(curtab, "direct");
    LOG_PRINT("%sdirect: %s\n", indent, direct.u.s);
    
    if ( 0 == strcmp( direct.u.s ,"VIEW") )
    {
       toml_datum_t cwd = toml_string_in(curtab, "cwd");
       toml_datum_t exe = toml_string_in(curtab, "exe");
       LOG_PRINT("%scwd: %s\n", indent,cwd.u.s);
       LOG_PRINT("%sexe: %s\n", indent,exe.u.s);
    }
    arr = toml_array_in(curtab, "CHILD");
    if (arr == 0 ) return;
    for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
        build_node_tree(tab , lv);
    }
}

static void parse_table_at(toml_table_t* curtab, int i)
{
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;

    arr = toml_array_in(curtab, "ROOT");
    tab = toml_table_at(arr, i);
    toml_datum_t index = toml_int_in(tab, "index");
    LOG_PRINT("ROOT index %d\n", (int)index.u.i);
    dfs_parse_node(tab ,0);
   // bfs_parse_node(tab ,0);
    
}

static toml_table_t* sarch_node(toml_table_t* curtab, int lv)
{
    int i;
    const char* key;
    const char* raw;  
    toml_array_t* arr;
    toml_table_t* tab;
    char *indent_fmt[256];
    char *indent[256];
    lv ++;
    
    sprintf(indent_fmt, "%s%d%s", "%-", lv*4 , "s");
    sprintf(indent, indent_fmt," ");

    toml_datum_t direct = toml_string_in(curtab, "direct");
    //LOG_PRINT("%sdirect: %s\n", indent, direct.u.s);
    
    if ( 0 == strcmp( direct.u.s ,"VIEW") )
    {
       return curtab;
       //toml_datum_t cwd = toml_string_in(curtab, "cwd");
       //toml_datum_t exe = toml_string_in(curtab, "exe");
       //LOG_PRINT("%scwd: %s\n", indent,cwd.u.s);
       //LOG_PRINT("%sexe: %s\n", indent,exe.u.s);
    }
    arr = toml_array_in(curtab, "CHILD");
    if (arr == 0 ) return NULL;
    tab = toml_table_at(arr, 0);
    if (tab == 0 ) return NULL;
    return sarch_node(tab , lv);
    //for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
    //    serch_node(tab , lv);
    //}
}

static void parse_table_s(toml_table_t* curtab, int i)
{
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;

    arr = toml_array_in(curtab, "ROOT");
    tab = toml_table_at(arr, i);
    toml_datum_t index = toml_int_in(tab, "index");

    LOG_PRINT("ROOT index %d\n", (int)index.u.i);
    //parse_node(tab ,0);
    toml_datum_t direct = toml_string_in(tab, "direct");
    LOG_PRINT("ROOT direct %s\n", direct.u.s);
    arr = toml_array_in(tab, "CHILD");
    toml_datum_t direct1 = toml_string_in(toml_table_at(arr,0), "direct");
    toml_datum_t direct2 = toml_string_in(toml_table_at(arr,1), "direct");
    LOG_PRINT("   CHILD direct1 %s\n", direct1.u.s);
    LOG_PRINT("   CHILD direct2 %s\n", direct2.u.s);
    toml_array_t* arr1 = toml_array_in(toml_table_at(arr,0), "CHILD");
    toml_array_t* arr2 = toml_array_in(toml_table_at(arr,1), "CHILD");
    toml_datum_t direct11 = toml_string_in(toml_table_at(arr1,0), "direct");
    toml_datum_t direct12 = toml_string_in(toml_table_at(arr1,1), "direct");
    LOG_PRINT("   CHILD direct11 %s\n", direct11.u.s);
    LOG_PRINT("   CHILD direct12 %s\n", direct12.u.s);
    toml_datum_t direct21 = toml_string_in(toml_table_at(arr2,0), "direct");
    toml_datum_t direct22 = toml_string_in(toml_table_at(arr2,1), "direct");
    LOG_PRINT("   CHILD direct21 %s\n", direct21.u.s);
    LOG_PRINT("   CHILD direct22 %s\n", direct22.u.s);

    // TODO
    //   top2面に対してFirst VIEWをサーチ
    //
    //toml_table_t* view = sarch_node(tab,0);
    arr = toml_array_in(tab, "CHILD");
    toml_table_t* tab1 = toml_table_at(arr,0);

    toml_table_t* view1 = sarch_node(tab1,0);

    toml_datum_t lv1  = toml_int_in(view1, "lv");
    toml_datum_t cwd1 = toml_string_in(view1, "cwd");
    toml_datum_t exe1 = toml_string_in(view1, "exe");
    LOG_PRINT("   lv  %d\n", lv1.u.i);
    LOG_PRINT("   cwd %s\n", cwd1.u.s);
    LOG_PRINT("   pwd %s\n", exe1.u.s);

    toml_table_t* tab2 = toml_table_at(arr,1);
    toml_table_t* view2 = sarch_node(tab2,0);

    toml_datum_t lv2  = toml_int_in(view2, "lv");
    toml_datum_t cwd2 = toml_string_in(view2, "cwd");
    toml_datum_t exe2 = toml_string_in(view2, "exe");
    LOG_PRINT("   lv  %d\n", lv2.u.i);
    LOG_PRINT("   cwd %s\n", cwd2.u.s);
    LOG_PRINT("   pwd %s\n", exe2.u.s);
    
}


static void
save_restore_next()
{
LOG_PRINT("nfds %d\n", nfds);
fd_set_print();

  split(t_root[t_root_index], VERTICAL,NULL,NULL);  // root left

}

static void
save_restore_tab()
{
   //parse_table_at(restore_tab, 0);
   parse_table_s(restore_tab, 0);
}

static void
save_restore()
{
   //  bash -i -c "vi /etc/passwd"
    //t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "/etc","/usr/bin/top");
    t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "~/tmp/mtm","nvim _log.txt");
    //t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "~/tmp/mtm","tail -f  _log.txt");
    //t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "~/tmp/mtm",NULL);
    if (!t_root[t_root_index])
            quit(EXIT_FAILURE, "could not open root window");
    t_root[t_root_index]->type = ROOT;
    t_root_enable[t_root_index] = 1;

    split(t_root[0], HORIZONTAL,"/",NULL);    //4 root 
    split(t_root[0]->c1, VERTICAL,"/usr",NULL);  //5 root left
    split(t_root[0]->c2, VERTICAL,NULL,"/usr/local/bin/fish");  //6 root right
    split(t_root[0]->c2->c1, HORIZONTAL, "/var/log",NULL);  //7 root right up

    t_root_index = 0;

}
/*
     75 typedef	struct __fd_set {
    76 #endif
    77 	long	fds_bits[__howmany(FD_SETSIZE, FD_NFDBITS)];
    78 } fd_set;  <typedef:fd_set>
    79 
    80 #define	FD_SET(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] |= \
    81 				    (1ul << ((__n) % FD_NFDBITS)))
    82 
    83 #define	FD_CLR(__n, __p)	((__p)->fds_bits[(__n)/FD_NFDBITS] &= \
    84 				    ~(1ul << ((__n) % FD_NFDBITS)))
    85 
    86 #define	FD_ISSET(__n, __p)	(((__p)->fds_bits[(__n)/FD_NFDBITS] & \
    87 				    (1ul << ((__n) % FD_NFDBITS))) != 0l)
    88 
    89 #ifdef _KERNEL
    90 #define	FD_ZERO(p)	bzero((p), sizeof (*(p)))
    91 #else
    92 #define	FD_ZERO(__p)	memset((void *)(__p), 0, sizeof (*(__p)))
    93 #endif 
 
 
 * */


int
main(int argc, char **argv)
{

    LOG_PRINT(">>> %d TEST START !!! \n", 99);
    ERR_LOG_PRINT(">>> %d err START %s !!! \n", 99, "abc");



    FD_SET(STDIN_FILENO, &fds);
    setlocale(LC_ALL, "");
    signal(SIGCHLD, SIG_IGN); /* automatically reap children */

    int c = 0;
    while ((c = getopt(argc, argv, "rc:T:t:")) != -1) switch (c){
        case 'r': t_restore_mode = true;      break;
        case 'c': commandkey = CTL(optarg[0]);      break;
        case 'T': setenv("TERM", optarg, 1);        break;
        case 't': term = optarg;                    break;
        default:  quit(EXIT_FAILURE, USAGE);        break;
    }

    if (!initscr())
        quit(EXIT_FAILURE, "could not initialize terminal");
    raw();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    start_color();
    use_default_colors();

    keypad(stdscr, TRUE); // xtermでマウスイベントの取得に必要
    //mousemask(ALL_MOUSE_EVENTS, NULL);//マウスイベントを取得
    //mousemask(BUTTON1_PRESSED, NULL);//マウスイベントを取得
    //mousemask(0, NULL);//マウスイベントを取得
    //mousemask(BUTTON1_CLICKED, NULL);//マウスイベントを取得

    //init_pair(LINE_PAIR, COLOR_RED, COLOR_MAGENTA);
    init_pair(LINE_PAIR,                  COLOR_BLUE,  COLOR_BLACK);
    init_pair(STATUS_BAR_PAIR,            COLOR_RED, COLOR_BLUE);
    init_pair(STATUS_BAR_EXPANDROOT_PAIR, COLOR_BLACK, COLOR_MAGENTA);
    start_pairs();

    for ( int n =0 ; n < PANE_MAX ; n++) {
        t_root[n] = NULL;
        t_root_enable[n] = 0;
    }

    //t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "/etc",NULL);
    //if (!t_root[t_root_index])
    //        quit(EXIT_FAILURE, "could not open root window");
    //t_root[t_root_index]->type = ROOT;
    //t_root_enable[t_root_index] = 1;

    //
    if (t_restore_mode) {
	     char  errbuf[200];
	    FILE* fp = fopen("./mtm_test.toml", "r");
            restore_tab = toml_parse_file(fp, errbuf, sizeof(errbuf));

	    save_restore_tab();
	    save_restore();

    } else {
       t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, NULL,NULL);
       if (!t_root[t_root_index])
               quit(EXIT_FAILURE, "could not open root window");
       t_root[t_root_index]->type = ROOT;
       t_root_enable[t_root_index] = 1;
    }

    focus(t_root[t_root_index]);
    draw(t_root[t_root_index]);

    while(1) {
        run();
	t_root_change = 0;

        //window_status(t_root[t_root_index]);

//	nfds = b_nfds ;
	if (t_restore_mode) {
		if (nfds < b_nfds) nfds++;
	}

	/*
	t_fd_set_init_count++;
	if (nfds < b_nfds) {
                nfds = nfds + t_fd_set[t_fd_set_init_count];
	}
*/

	
	//nfds = b_nfds;

	if (t_root_change_type == CREATE) {
		int old_index = t_root_index;
		if (set_create_root_index())
		{
                   t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS,NULL,NULL);
                   if (!t_root[t_root_index])
		   {  
		      t_root_index = old_index;
                      continue;
		   }
                   t_root[t_root_index]->type = ROOT;
                   t_root_enable[t_root_index] = 1;

                   focus(t_root[t_root_index]);
                   draw(t_root[t_root_index]);
		}

	} else if (t_root_change_type == RESTORE) {
		int old_index = t_root_index;
		if (set_create_root_index())
		{
                   t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS,NULL,NULL);
                   if (!t_root[t_root_index])
		   {  
		      t_root_index = old_index;
                      continue;
		   }
                   t_root[t_root_index]->type = ROOT;
                   t_root_enable[t_root_index] = 1;

                save_restore_next();

                   focus(t_root[t_root_index]);
                   draw(t_root[t_root_index]);
		}

	} else if (t_root_change_type == NEXT) {
		clear();
		set_next_root_index();
		reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                //focus(t_root[t_root_index]);
                focus(t_focused[t_root_index]);
                draw(t_root[t_root_index]);
		//focus(t_root[t_root_index]);

	} else if (t_root_change_type == PREV) {
		clear();
		set_prev_root_index();
		reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                //focus(t_root[t_root_index]);
                focus(t_focused[t_root_index]);
                draw(t_root[t_root_index]);
		//focus(t_root[t_root_index]);

	} else if (t_root_change_type == TOGGLE_LABEL) {
                   if (window_label_show) {
		      clear();
                      //draw(t_root[t_root_index]);
                      window_label(t_root[t_root_index]);
                   } else {
                      //reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                      //window_label_clear(t_root[t_root_index]);
                      //reshapeview(t_root[t_root_index],0,0);
                      //draw(t_root[t_root_index]);
		      //wnoutrefresh(stdscr);
		      //doupdate();
		      //refresh();
		      clear();
		      //reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                      draw(t_root[t_root_index]);

		   }
	} else if (t_root_change_type == EXPAND) {
		int old_index = t_root_index;
		if (set_create_root_index())
		{
                   t_root[t_root_index] = t_expand_node;
                   if (!t_root[t_root_index])
		   {  
		      t_root_index = old_index;
                      continue;
		   }
                   //t_root[t_root_index]->type = ROOT;
                   t_root[t_root_index]->type = EXPANDROOT;
                   t_root_enable[t_root_index] = 1;
		   reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                   focus(t_root[t_root_index]);
                   draw(t_root[t_root_index]);

		}
	} else if (t_root_change_type == FETCH) {
		clear();
		//set_next_root_index();
                //window_status(t_root[t_root_index]);
		reshape(t_root[t_root_index], 0, 0, LINES-1, COLS);
                focus(t_root[t_root_index]);
                draw(t_root[t_root_index]);
		//focus(t_root[t_root_index]);
	} else if (t_root_change_type == SWAP || t_root_change_type == FETCH) {
                  NODE *n = t_root[t_root_index];
                  reshape(n, n->y, n->x, n->h, n->w);
                  draw(n);
	}
    }



    quit(EXIT_SUCCESS, NULL);
    return EXIT_SUCCESS; /* not reached */
}
