/* See LICENSE file for copyright and license details. */
#include <X11/XF86keysym.h>

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=12" };
static const char dmenufont[]       = "monospace:size=11";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#f59542";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
};

/* tagging */
static const char *tags[] = { "", "", "", "", "₿", "", "", "", "", "" };
// static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
// static const char *tags[] = { "", "", "", "", "", "", "", "", "", "" };


static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

static int
is_tiled_layout(void)
{
    return selmon->lt[selmon->sellt]->arrange != NULL;
};

static void focusdir(const Arg *arg);
static Client *getclosest(int xdir, int ydir);

static Client *
getclosest(int dx, int dy)
{
    Client *c, *best = NULL;
    int bestdist = INT_MAX;

    if (!selmon->sel)
        return NULL;

    int sx = selmon->sel->x;
    int sy = selmon->sel->y;

    for (c = selmon->clients; c; c = c->next) {
        if (c == selmon->sel || c->isfloating)
            continue;

        int cx = c->x;
        int cy = c->y;

        int xdiff = cx - sx;
        int ydiff = cy - sy;

        // enforce direction constraint
        if ((dx < 0 && xdiff >= 0) ||
            (dx > 0 && xdiff <= 0) ||
            (dy < 0 && ydiff >= 0) ||
            (dy > 0 && ydiff <= 0))
            continue;

        int dist = abs(xdiff * xdiff + ydiff * ydiff);

        if (dist < bestdist) {
            bestdist = dist;
            best = c;
        }
    }

    return best;
};

static void
focusdir(const Arg *arg)
{
    Client *c = NULL;

    if (selmon->lt[selmon->sellt]->arrange) {
        switch (arg->i) {
        case 0: c = getclosest(-1, 0); break;
        case 1: c = getclosest(1, 0);  break;
        case 2: c = getclosest(0, -1); break;
        case 3: c = getclosest(0, 1);  break;
        }

        if (c) {
            focus(c);
            restack(selmon);
            return;
        }
    }

    // fallback
    switch (arg->i) {
    case 0:
    case 2:
        focusstack(&((Arg){.i = -1}));
        break;

    case 1:
    case 3:
        focusstack(&((Arg){.i = 1}));
        break;
    }

    return;
};

/* View next/previous occupied tag on current monitor */
static void
viewnexttag(const Arg *arg)
{
    unsigned int i, nt = 0;
    for (i = 0; i < LENGTH(tags); i++)
        if (selmon->tagset[selmon->seltags] & (1 << i))
            nt = i;

    for (i = (nt + 1) % LENGTH(tags); i != nt; i = (i + 1) % LENGTH(tags)) {
        if (selmon->tagset[selmon->seltags ^ (1 << i)] & (1 << i) || /* current tag */
            (selmon->clients && (selmon->tagset[selmon->seltags] & (1 << i)) == 0)) {
            if (selmon->clients) {
                Client *c;
                for (c = selmon->clients; c; c = c->next)
                    if (c->tags & (1 << i))
                        goto found;
            }
        }
    }
    return;

found:
    view(&((Arg) { .ui = 1 << i }));
}

static void
viewprevtag(const Arg *arg)
{
    unsigned int i, nt = 0;
    for (i = 0; i < LENGTH(tags); i++)
        if (selmon->tagset[selmon->seltags] & (1 << i))
            nt = i;

    for (i = (nt + LENGTH(tags) - 1) % LENGTH(tags); i != nt; i = (i + LENGTH(tags) - 1) % LENGTH(tags)) {
        if (selmon->clients) {
            Client *c;
            for (c = selmon->clients; c; c = c->next)
                if (c->tags & (1 << i))
                    goto found;
        }
    }
    return;

found:
    view(&((Arg) { .ui = 1 << i }));
}

static void
viewnexttag_simple(const Arg *arg)
{
    int i, cur = 0;

    for (i = 0; i < LENGTH(tags); i++)
        if (selmon->tagset[selmon->seltags] & (1 << i))
            cur = i;

    cur = (cur + 1) % LENGTH(tags);
    view(&(Arg){ .ui = 1 << cur });
}

static void
viewprevtag_simple(const Arg *arg)
{
    int i, cur = 0;

    for (i = 0; i < LENGTH(tags); i++)
        if (selmon->tagset[selmon->seltags] & (1 << i))
            cur = i;

    cur = (cur + LENGTH(tags) - 1) % LENGTH(tags);
    view(&(Arg){ .ui = 1 << cur });
}

/* layout(s) */
static const float mfact     = 0.50; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */
static const int refreshrate = 120;  /* refresh rate (per second) for client move/resize */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

static void
togglefullscreen(const Arg *arg)
{
    if (selmon->lt[selmon->sellt] == &layouts[0])
        setlayout(&(Arg){ .v = &layouts[2] });
    else
        setlayout(&(Arg){ .v = &layouts[0] });
}

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[2] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *volupcmd[] = {
	"/bin/sh", "-c",
	"amixer set Master 5%+ unmute && paplay /usr/share/sounds/freedesktop/stereo/audio-volume-change.oga",
	NULL
};

static const char *voldowncmd[] = {
	"/bin/sh", "-c",
	"amixer set Master 5%- unmute && paplay /usr/share/sounds/freedesktop/stereo/audio-volume-change.oga",
	NULL
};

static const char *mutecmd[] = {
	"/bin/sh", "-c",
	"amixer set Master toggle && paplay /usr/share/sounds/freedesktop/stereo/audio-volume-change.oga",
	NULL
};

static const char *dmenucmd[] = { "dmenu_run", "-m", dmenumon, "-fn", dmenufont, "-nb", col_gray1, "-nf", col_gray3, "-sb", col_cyan, "-sf", col_gray4, NULL };
static const char *termcmd[]  = { "alacritty", NULL };
static const char *filebrowsercmd[]  = { "thunar", NULL };
static const char *browsercmd[]  = { "brave-browser", NULL };
static const char *bluetoothcmd[]  = { "blueman-manager", NULL };
static const char *spotifycmd[] = {
    "/bin/sh",
    "-c",
    "LD_PRELOAD=/usr/local/lib/spotify-adblock.so spotify & "
    "while ! playerctl -l | grep -q spotify; do sleep 0.5; done; "
    "~/.local/bin/spotify-notify",
    NULL
};
static const char *spotifytogglecmd[]  = { "/bin/sh", "-c", "playerctl -p spotify play-pause", NULL };
static const char *launchercmd[]  = { "/bin/sh", "-c", "rofi -show drun -show-icons -theme android_notification", NULL };
static const char *walletcmd[]  = { "/bin/sh", "-c", "/opt/sparrowwallet/bin/Sparrow", NULL };

static const Key keys[] = {
	/* modifier                     key        function        argument */
    { 0, XF86XK_AudioRaiseVolume, spawn, {.v = volupcmd } },
    { 0, XF86XK_AudioLowerVolume, spawn, {.v = voldowncmd } },
    { 0, XF86XK_AudioMute,        spawn, {.v = mutecmd } },
	{ MODKEY,                       XK_p,      spawn,          {.v = spotifycmd } },
	{ MODKEY,                       XK_u,      spawn,          {.v = spotifytogglecmd } },
	{ MODKEY,                       XK_s,      spawn,          {.v = walletcmd } },
	{ MODKEY,                       XK_Return, spawn,          {.v = termcmd } },
	{ MODKEY,                       XK_b,      spawn,          {.v = browsercmd } },
	{ MODKEY,                       XK_e,      spawn,          {.v = filebrowsercmd } },
	{ MODKEY,                       XK_w,      spawn,          {.v = bluetoothcmd } },
    { Mod1Mask,                     XK_space,  spawn,          {.v = launchercmd} },
    { Mod1Mask,                     XK_l,      spawn,          SHCMD("xsecurelock") },
	// { MODKEY,                       XK_b,      togglebar,      {0} },
	// { MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	// { MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	// { MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	// { MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	// { MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
    { MODKEY, XK_h, focusdir, {.i = 0 } },
    { MODKEY, XK_l, focusdir, {.i = 1 } },
    { MODKEY, XK_k, focusdir, {.i = 2 } },
    { MODKEY, XK_j, focusdir, {.i = 3 } },
        /* Ctrl + Super + h/l : previous/next occupied tag */
    { ControlMask|MODKEY, XK_h, viewprevtag, {0} },
    { ControlMask|MODKEY, XK_l, viewnexttag, {0} },
	{ MODKEY|ShiftMask,             XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	// { MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY,                       XK_q,      killclient,     {0} },
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
    { MODKEY,                       XK_f,                   togglefullscreen, {0} },
	//{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	//{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	//{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
    { MODKEY, XK_comma,  viewprevtag_simple, {0} },
    { MODKEY, XK_period, viewnexttag_simple, {0} },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_e,      quit,           {0} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};

