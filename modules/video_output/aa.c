/*****************************************************************************
 * aa.c: "vout display" module using aalib
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include "keythread.h"

#include <assert.h>
#include <aalib.h>

/* TODO
 * - what about RGB palette ?
 */
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("ASCII Art"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_description(N_("ASCII-art video output"))
    set_capability("vout display", 10)
    add_shortcut("aalib")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_pool_t *Pool   (vout_display_t *, unsigned);
static void            Prepare(vout_display_t *, picture_t *);
static void            Display(vout_display_t *, picture_t *);
static int             Control(vout_display_t *, int, va_list);

/* */
static void Manage(vout_display_t *);

/* */
struct vout_display_sys_t {
    struct aa_context*  aa_context;
    aa_palette          palette;

    vout_display_cfg_t  state;
    picture_pool_t      *pool;
    key_thread_t        *keys;
};

/**
 * This function allocates and initializes a aa vout method.
 */
static int Open(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys;

    /* Allocate structure */
    vd->sys = sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    /* Don't parse any options, but take $AAOPTS into account */
    aa_parseoptions(NULL, NULL, NULL, NULL);

    /* */
    sys->aa_context = aa_autoinit(&aa_defparams);
    if (!sys->aa_context) {
        msg_Err(vd, "cannot initialize aalib");
        goto error;
    }

    aa_autoinitkbd(sys->aa_context, 0);
    aa_autoinitmouse(sys->aa_context, AA_MOUSEALLMASK);

    /* */
    video_format_t fmt = vd->fmt;
    fmt.i_chroma = VLC_CODEC_RGB8;
    fmt.i_width  = aa_imgwidth(sys->aa_context);
    fmt.i_height = aa_imgheight(sys->aa_context);

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = true;

    /* Setup vout_display now that everything is fine */
    vd->fmt = fmt;
    vd->info = info;

    vd->pool    = Pool;
    vd->prepare = Prepare;
    vd->display = Display;
    vd->control = Control;
    vd->manage  = Manage;

    /* Inspect initial configuration and send correction events
     * FIXME how to handle aspect ratio with aa ? */
    sys->state = *vd->cfg;
    sys->state.is_fullscreen = false;
    vout_display_SendEventFullscreen(vd, false);
    vout_display_SendEventDisplaySize(vd, fmt.i_width, fmt.i_height, false);

    sys->keys = vlc_CreateKeyThread(vd);
    return VLC_SUCCESS;

error:
    if (sys && sys->aa_context)
        aa_close(sys->aa_context);
    free(sys);
    return VLC_EGENERIC;
}

/**
 * Close a aa video output method
 */
static void Close(vlc_object_t *object)
{
    vout_display_t *vd = (vout_display_t *)object;
    vout_display_sys_t *sys = vd->sys;

    vlc_DestroyKeyTread(sys->keys);
    if (sys->pool)
        picture_pool_Delete(sys->pool);
    aa_close(sys->aa_context);
    free(sys);
}

/**
 * Return a pool of direct buffers
 */
static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(count);

    if (!sys->pool) {
        picture_resource_t rsc;

        memset(&rsc, 0, sizeof(rsc));
        rsc.p[0].p_pixels = aa_image(sys->aa_context);
        rsc.p[0].i_pitch = aa_imgwidth(sys->aa_context);
        rsc.p[0].i_lines = aa_imgheight(sys->aa_context);

        picture_t *p_picture = picture_NewFromResource(&vd->fmt, &rsc);
        if (!p_picture)
            return NULL;

        sys->pool = picture_pool_New(1, &p_picture);
    }
    return sys->pool;
}

/**
 * Prepare a picture for display */
static void Prepare(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    assert(vd->fmt.i_width  == aa_imgwidth(sys->aa_context) &&
           vd->fmt.i_height == aa_imgheight(sys->aa_context));

#if 0
    if (picture->format.p_palette) {
        for (int i = 0; i < 256; i++) {
            aa_setpalette(vd->sys->palette, 256 - i,
                           red[ i ], green[ i ], blue[ i ]);
        }
    }
#else
    VLC_UNUSED(picture);
#endif

    aa_fastrender(sys->aa_context, 0, 0,
                  vd->fmt.i_width, vd->fmt.i_height);
}

/**
 * Display a picture
 */
static void Display(vout_display_t *vd, picture_t *picture)
{
    vout_display_sys_t *sys = vd->sys;

    aa_flush(sys->aa_context);
    picture_Release(picture);
}

/**
 * Control for vout display
 */
static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(args);
    vout_display_sys_t *sys = vd->sys;

    switch (query) {
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        /* We have to ignore what is requested */
        vout_display_SendEventPicturesInvalid(vd);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_RESET_PICTURES:
        if (sys->pool)
            picture_pool_Delete(sys->pool);
        sys->pool = NULL;

        vd->fmt.i_width  = aa_imgwidth(sys->aa_context);
        vd->fmt.i_height = aa_imgheight(sys->aa_context);
        return VLC_SUCCESS;

    case VOUT_DISPLAY_HIDE_MOUSE:
        aa_hidemouse(sys->aa_context);
        return VLC_SUCCESS;

    default:
        msg_Err(vd, "Unsupported query in vout display aalib");
        return VLC_EGENERIC;
    }
}


/**
 * Proccess pending event
 */
static void Manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;

    for (;;) {
        const int event = aa_getevent(sys->aa_context, 0);
        if (!event)
            return;

        switch (event) {
        case AA_MOUSE: {
            int x, y;
            int button;
            int vlc;
            aa_getmouse(sys->aa_context, &x, &y, &button);

            vlc = 0;
            if (button & AA_BUTTON1)
                vlc |= 1 << MOUSE_BUTTON_LEFT;
            if (button & AA_BUTTON2)
                vlc |= 1 << MOUSE_BUTTON_CENTER;
            if (button & AA_BUTTON3)
                vlc |= 1 << MOUSE_BUTTON_RIGHT;

            vout_display_SendEventMouseState(vd, x, y, vlc);

            aa_showcursor(sys->aa_context); /* Not perfect, we show it on click too */
            break;
        }

        case AA_RESIZE:
            aa_resize(sys->aa_context);
            vout_display_SendEventDisplaySize(vd,
                                              aa_imgwidth(sys->aa_context),
                                              aa_imgheight(sys->aa_context), false);
            break;

        /* TODO keys support to complete */
        case AA_UP:
            vlc_EmitKey(sys->keys, KEY_UP);
            break;
        case AA_DOWN:
            vlc_EmitKey(sys->keys, KEY_DOWN);
            break;
        case AA_RIGHT:
            vlc_EmitKey(sys->keys, KEY_RIGHT);
            break;
        case AA_LEFT:
            vlc_EmitKey(sys->keys, KEY_LEFT);
            break;
        case AA_BACKSPACE:
            vlc_EmitKey(sys->keys, KEY_BACKSPACE);
            break;
        case AA_ESC:
            vlc_EmitKey(sys->keys, KEY_ESC);
            break;
        default:
            if (event >= 0x20 && event <= 0x7f)
                vlc_EmitKey(sys->keys, event);
            break;
        }
    }
}

