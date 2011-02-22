/*****************************************************************************
 * common.h : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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
#include <string>
#include <list>
#include <deque>
using namespace std;

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   ifdef _WINGDI_
#      undef _WINGDI_
#   endif
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   ifndef _I64_MAX
#     define _I64_MAX LONG_LONG_MAX
#   endif
#   define LONGLONG long long
#endif

#include <dshow.h>

typedef struct dshow_stream_t dshow_stream_t;

/****************************************************************************
 * Crossbar stuff
 ****************************************************************************/
#define MAX_CROSSBAR_DEPTH 10

typedef struct CrossbarRouteRec
{
    IAMCrossbar *pXbar;
    LONG        VideoInputIndex;
    LONG        VideoOutputIndex;
    LONG        AudioInputIndex;
    LONG        AudioOutputIndex;

} CrossbarRoute;

void DeleteCrossbarRoutes( access_sys_t * );
HRESULT FindCrossbarRoutes( vlc_object_t *, access_sys_t *,
                            IPin *, LONG, int = 0 );

/****************************************************************************
 * Access descriptor declaration
 ****************************************************************************/
struct access_sys_t
{
    /* These 2 must be left at the beginning */
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    IFilterGraph           *p_graph;
    ICaptureGraphBuilder2  *p_capture_graph_builder2;
    IMediaControl          *p_control;

    int                     i_crossbar_route_depth;
    CrossbarRoute           crossbar_routes[MAX_CROSSBAR_DEPTH];

    /* list of elementary streams */
    dshow_stream_t **pp_streams;
    int            i_streams;
    int            i_current_stream;

    /* misc properties */
    int            i_width;
    int            i_height;
    int            i_chroma;
    bool           b_chroma; /* Force a specific chroma on the dshow input */
};

const GUID IID_IAMBufferNegotiation = {0x56ed71a0, 0xaf5f, 0x11d0, {0xb3, 0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5}};
const GUID IID_IAMTVAudio      = {0x83EC1C30, 0x23D1, 0x11d1, {0x99, 0xE6, 0x00, 0xA0, 0xC9, 0x56, 0x02, 0x66}};
const GUID IID_IAMCrossbar     = {0xC6E13380, 0x30AC, 0x11d0, {0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56}};
const GUID IID_IAMTVTuner      = {0x211A8766, 0x03AC, 0x11d1, {0x8D, 0x13, 0x00, 0xAA, 0x00, 0xBD, 0x83, 0x39}};

/* http://msdn.microsoft.com/en-us/library/dd373441%28v=vs.85%29.aspx */
typedef enum tagAMTunerModeType {
    AMTUNER_MODE_DEFAULT    = 0x0000,
    AMTUNER_MODE_TV         = 0x0001,
    AMTUNER_MODE_FM_RADIO   = 0x0002,
    AMTUNER_MODE_AM_RADIO   = 0x0004,
    AMTUNER_MODE_DSS        = 0x0008 
} AMTunerModeType;

typedef enum tagAMTunerSubChannel {
    AMTUNER_SUBCHAN_NO_TUNE = -2,
    AMTUNER_SUBCHAN_DEFAULT = -1
} AMTunerSubChannel;

/* http://msdn.microsoft.com/en-us/library/dd407232%28v=vs.85%29.aspx */
typedef enum tagTunerInputType {
    TunerInputCable = 0,
    TunerInputAntenna = TunerInputCable + 1
} TunerInputType;

#define AMPROPERTY_PIN_CATEGORY 0
