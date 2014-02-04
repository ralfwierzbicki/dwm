/* See LICENSE file for copyright and license details. */
#include "grid.c"
#include <X11/XF86keysym.h>

/* appearance */
static const char font[]            = "Terminus 12";
static const char normbordercolor[] = "#444444";
static const char normbgcolor[]     = "#222222";
static const char normfgcolor[]     = "#bbbbbb";
static const char selbordercolor[]  = "#005577";
static const char selbgcolor[]      = "#005577";
static const char selfgcolor[]      = "#eeeeee";
static const char floatnormbordercolor[] = "#005577";
static const char floatselbordercolor[]  = "#0687BA";
static const char taskfg[] = "#eeeeee";
static const char taskbg[]  = "#0687BA";
static const char urgbordercolor[]  = "#ff0000";
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int systrayspacing = 2;   /* systray spacing */
static const unsigned int gappx     = 3;        /* gap pixel between windows */
static const Bool showsystray       = True;     /* False means no systray */
static const Bool showbar           = True;     /* False means no bar */
static const Bool topbar            = True;     /* False means bottom bar */
static const Bool statusmarkup      = True;     /* True means use pango markup in status message */

static const char *volumedown[] = { "amixer", "-q", "set", "Master", "2%-", "unmute", NULL };
static const char *volumeup[]   = { "amixer", "-q", "set", "Master", "2%+", "unmute", NULL };
static const char *mute[]       = { "amixer", "-q", "set", "Master", "toggle", NULL };

/* tagging */
static const char *tags[] = {
	"1",
	"2\356\235\206",
	"3\356\235\206",
	"4\356\235\206",
	"5\356\235\206",
	"6\356\235\206",
	"7\356\235\206",
	"8\356\235\206",
	"(9)",
};

static const Rule rules[] = {
	/* class      instance    title       tags mask     isfloating   monitor 	role*/
	{ "Gimp",     NULL,       NULL,       0,            True,        -1,	NULL },
	{ "ck2",      NULL,       NULL,       0,            True,        -1,	NULL },
	{ NULL,       NULL,       "File Operation Progress",0,True,      -1,    NULL },
	{ "Transmission-gtk",NULL,NULL,       0,            True,        -1,    NULL },
	{ NULL,       NULL,       NULL,       0,            True,        -1,    "compose" },
	{ NULL,       NULL,       NULL,       0,            True,        -1,    "About" },
	{ NULL,       NULL,       NULL,       0,            True,        -1,    "about" },
	{ NULL,       NULL,       NULL,       0,            True,        -1,    "epiphany-preferences" },
	{ NULL,       NULL,       NULL,       0,            True,        -1,    "epiphany-cookie-manager" },
};

/* layout(s) */
static const float mfact      = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster      = 1;    /* number of clients in master area */
static const Bool resizehints = True; /* True means respect size hints in tiled resizals */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "<b>[]=</b>",      tile },    /* first entry is default */
	{ "<b>[F]</b>",      NULL },    /* no layout function means floating behavior */
	{ "<b>[M]</b>",      monocle },
	{ "<b>[G]</b>",      grid },
};

/* key definitions */
#define MODKEY Mod1Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *dmenucmd[] = { "dmenu_run", "-fn", font, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor, "-hist", "/home/ralf/.dmenu_history", NULL };
static const char *termcmd[]  = { "gnome-terminal", NULL };
//static const char *quitcmd[]  = { "gnome-session-quit", "--logout", NULL };
static const char *deskmenu[] = { "compiz-boxmenu", NULL };

static Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_p,      spawn,          {.v = dmenucmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY,                       XK_Tab,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY,                       XK_Left,   setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_Right,  setmfact,       {.f = +0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	/* { MODKEY,                       XK_Tab,    view,           {0} }, */
	{ MODKEY,                       XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_g,      setlayout,      {.v = &layouts[3]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ 0, XF86XK_AudioLowerVolume,              spawn,          { .v = volumedown } },
	{ 0, XF86XK_AudioRaiseVolume,              spawn,          { .v = volumeup } }, 
        { 0, XF86XK_AudioMute,                     spawn,          { .v = mute } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY,		        XK_q,      quit,           {0} },
};

/* button definitions */
/* click can be ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button3,        killclient,     {0} },
	{ ClkStatusText,        0,              Button3,        spawn,          {.v = deskmenu } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkWinTitle,          0,              Button1,        focusonclick,   {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
	{ ClkRootWin,           0,              Button3,        spawn,          {.v = deskmenu } },
};

