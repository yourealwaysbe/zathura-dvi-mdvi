/*
 * zathura-dvi.c: entry point for implementation of Zathura DVI plugin
 * Matthew Hague <matthewhague@zoho.com>
 *
 * Based heavily on Evince's DVI backend, which is:
 * Copyright (C) 2005, Nickolay V. Shmyrev <nshmyrev@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "texmfcnf.h"

#include "mdvi-lib/color.h"
#include "mdvi-lib/mdvi.h"
#include "fonts.h"
#include "cairo-device.h"

#include <zathura/plugin-api.h>

#include <ctype.h>
# include <sys/wait.h>
#include <stdlib.h>

static GMutex dvi_context_mutex;

typedef struct _DviDocument DviDocument;

zathura_error_t 
plugin_document_open (zathura_document_t *document);

zathura_error_t
plugin_document_free (zathura_document_t* document, 
                      void *dvi_document);

zathura_error_t
plugin_page_init(zathura_page_t* page);

zathura_error_t
plugin_page_clear(zathura_page_t *page, void *notused);

zathura_error_t
plugin_page_render_cairo(zathura_page_t *page, 
                         void *notused, 
                         cairo_t* cairo, 
                         bool printing);

void
register_functions(zathura_plugin_functions_t* functions)
{
  functions->document_open     = plugin_document_open;
  functions->document_free     = plugin_document_free;
  functions->page_init         = plugin_page_init;
  functions->page_clear        = plugin_page_clear;
  functions->page_render_cairo = plugin_page_render_cairo;
}

ZATHURA_PLUGIN_REGISTER(
  "dvi",
  0, 1, 0,
  register_functions,
  ZATHURA_PLUGIN_MIMETYPES({
    "application/x-dvi"
  })
)



enum {
    PROP_0,
    PROP_TITLE
};

struct _DviDocument
{
    DviContext *context;
    DviPageSpec *spec;
    DviParams *params;
    
    /* To let document scale we should remember width and height */
    double base_width;
    double base_height;
    
    const char* path;
};

static void
dvi_document_init_params (DviDocument *dvi_document);
static void
dvi_document_free (DviDocument *dvi_document);

zathura_error_t
plugin_document_free(zathura_document_t* document, 
                     void *dvi_document)
{
    DviDocument *doc = (DviDocument*)dvi_document;
    dvi_document_free (doc);
    return ZATHURA_ERROR_OK;
}

static void dvi_document_do_color_special (DviContext *dvi,
                                           const char *prefix,
                                           const char *arg);

static void dvi_document_free (DviDocument *doc)
{
    if (!doc)
        return; 

    g_mutex_lock (&dvi_context_mutex);
    if (doc->context) {
        mdvi_cairo_device_free (&doc->context->device);
        mdvi_destroy_context (doc->context);
    }
    g_mutex_unlock (&dvi_context_mutex);

    if (doc->params)
        g_free (doc->params);

    if (doc->spec)
        g_free (doc->spec);

    g_free (doc);
}


zathura_error_t
plugin_document_open (zathura_document_t *document)
{
    gchar *texmfcnf = get_texmfcnf();
    mdvi_init_kpathsea ("zathura", MDVI_MFMODE, MDVI_FALLBACK_FONT, MDVI_DPI, texmfcnf);
    g_free(texmfcnf);

    mdvi_register_special ("Color", "color", NULL, dvi_document_do_color_special, 1);
    mdvi_register_fonts ();

    DviDocument *dvi_document = g_new0 (DviDocument, 1);

    dvi_document->context = NULL;
    dvi_document_init_params (dvi_document);

    const char* path = zathura_document_get_path(document);

    g_mutex_lock (&dvi_context_mutex);
    dvi_document->context = mdvi_init_context(dvi_document->params, 
                                              dvi_document->spec, 
                                              path);
    g_mutex_unlock (&dvi_context_mutex);

    if (!dvi_document->context) {
        dvi_document_free (dvi_document);
        return ZATHURA_ERROR_UNKNOWN;
    }

    zathura_document_set_data(document, dvi_document);

    zathura_document_set_number_of_pages(document, 
                                         dvi_document->context->npages);
   
    mdvi_cairo_device_init (&dvi_document->context->device);
    
    dvi_document->base_width = dvi_document->context->dvi_page_w * dvi_document->context->params.conv 
        + 2 * unit2pix(dvi_document->params->dpi, MDVI_HMARGIN) / dvi_document->params->hshrink;
    
    dvi_document->base_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv 
            + 2 * unit2pix(dvi_document->params->vdpi, MDVI_VMARGIN) / dvi_document->params->vshrink;
    
    dvi_document->path = path;
    
    return ZATHURA_ERROR_OK;
}

zathura_error_t
plugin_page_init(zathura_page_t* page)
{
    if (page == NULL) {
        return ZATHURA_ERROR_INVALID_ARGUMENTS;
    }
  
    zathura_document_t* document = zathura_page_get_document(page);
    DviDocument* dvi_document = zathura_document_get_data(document);
  
    if (dvi_document == NULL) {
        return ZATHURA_ERROR_UNKNOWN;
    }
  
    zathura_page_set_data(page, NULL);
  
    zathura_page_set_width(page, dvi_document->base_width);
    zathura_page_set_height(page, dvi_document->base_height);
  
    return ZATHURA_ERROR_OK;
}


zathura_error_t
plugin_page_clear(zathura_page_t *page, void *notused)
{
  return ZATHURA_ERROR_OK;
}

// TODO: printing
zathura_error_t
plugin_page_render_cairo(zathura_page_t *page, 
                         void *notused, 
                         cairo_t* cairo, 
                         bool printing)
{
    zathura_document_t* document = zathura_page_get_document (page);
    if (document == NULL) {
      return ZATHURA_ERROR_INVALID_ARGUMENTS;
    }

    DviDocument* dvi_document = zathura_document_get_data (document);
    
    g_mutex_lock (&dvi_context_mutex);

    mdvi_setpage (dvi_document->context, 
                  zathura_page_get_index(page));
   
    /* calculate sizes */
    gdouble scale = zathura_document_get_scale(document);

    unsigned int page_width  = ceil(scale * zathura_page_get_width(page));
    unsigned int page_height = ceil(scale * zathura_page_get_height(page));

    unsigned int proposed_width =  dvi_document->context->dvi_page_w * dvi_document->context->params.conv;
    unsigned int proposed_height = dvi_document->context->dvi_page_h * dvi_document->context->params.vconv;

    mdvi_set_shrink (dvi_document->context, 
                     (int)((dvi_document->params->hshrink - 1) / scale) + 1,
                     (int)((dvi_document->params->vshrink - 1) / scale) + 1);

    unsigned int xmargin = 0;
    unsigned int ymargin = 0;

    if (page_width >= proposed_width)
        xmargin = (page_width - proposed_width) / 2;
    if (page_height >= proposed_height)
        ymargin = (page_height - proposed_height) / 2;
        
    mdvi_cairo_device_set_margins (&dvi_document->context->device, 
                                   xmargin, 
                                   ymargin);
    mdvi_cairo_device_set_scale (&dvi_document->context->device, 
                                 1.0/scale, 
                                 1.0/scale);
    mdvi_cairo_device_render (dvi_document->context, cairo);

    g_mutex_unlock (&dvi_context_mutex);

    return ZATHURA_ERROR_OK;
}

static void
dvi_document_init_params (DviDocument *dvi_document)
{    
    dvi_document->params = g_new0 (DviParams, 1);    

    dvi_document->params->dpi      = MDVI_DPI;
    dvi_document->params->vdpi     = MDVI_VDPI;
    dvi_document->params->mag      = MDVI_MAGNIFICATION;
    dvi_document->params->density  = MDVI_DEFAULT_DENSITY;
    dvi_document->params->gamma    = MDVI_DEFAULT_GAMMA;
    dvi_document->params->flags    = MDVI_PARAM_ANTIALIASED;
    dvi_document->params->hdrift   = 0;
    dvi_document->params->vdrift   = 0;
    dvi_document->params->hshrink  =  MDVI_SHRINK_FROM_DPI(dvi_document->params->dpi);
    dvi_document->params->vshrink  =  MDVI_SHRINK_FROM_DPI(dvi_document->params->vdpi);
    dvi_document->params->orientation = MDVI_ORIENT_TBLR;

    dvi_document->spec = NULL;
    
    dvi_document->params->bg = 0xffffffff;
    dvi_document->params->fg = 0xff000000;
}


#define RGB2ULONG(r,g,b) ((0xFF<<24)|(r<<16)|(g<<8)|(b))

static gboolean
hsb2rgb (float h, float s, float v, guchar *red, guchar *green, guchar *blue)
{
        float f, p, q, t, r, g, b;
        int i;

        s /= 100;
        v /= 100;
        h /= 60;
        i = floor (h);
        if (i == 6)
                i = 0;
        else if ((i > 6) || (i < 0))
                return FALSE;
        f = h - i;
        p = v * (1 - s);
        q = v * (1 - (s * f));
        t = v * (1 - (s * (1 - f)));

    if (i == 0) {
        r = v;
        g = t;
        b = p;
    } else if (i == 1) {
        r = q;
        g = v;
        b = p;
    } else if (i == 2) {
        r = p;
        g = v;
        b = t;
    } else if (i == 3) {
        r = p;
        g = q;
        b = v;
    } else if (i == 4) {
        r = t;
        g = p;
        b = v;
    } else if (i == 5) {
        r = v;
        g = p;
        b = q;
    }

        *red   = (guchar)floor(r * 255.0);
        *green = (guchar)floor(g * 255.0);
        *blue  = (guchar)floor(b * 255.0);
    
        return TRUE;
}



static void
parse_color (const gchar *ptr,
         gdouble     *color,
         gint         n_color)
{
    gchar *p = (gchar *)ptr;
    gint   i;

    for (i = 0; i < n_color; i++) {
        while (isspace (*p)) p++;
        color[i] = g_ascii_strtod (p, NULL);
        while (!isspace (*p) && *p != '\0') p++;
        if (*p == '\0')
            break;
    }
}


static void
dvi_document_do_color_special (DviContext *dvi, const char *prefix, const char *arg)
{
    if (strncmp (arg, "pop", 3) == 0) {
        mdvi_pop_color (dvi);
    } else if (strncmp (arg, "push", 4) == 0) {
        /* Find color source: Named, CMYK or RGB */
        const char *tmp = arg + 4;
        
        while (isspace (*tmp)) tmp++;

        if (!strncmp ("rgb", tmp, 3)) {
            gdouble rgb[3];
            guchar red, green, blue;

            parse_color (tmp + 4, rgb, 3);
            
            red = 255 * rgb[0];
            green = 255 * rgb[1];
            blue = 255 * rgb[2];

            mdvi_push_color (dvi, RGB2ULONG (red, green, blue), 0xFFFFFFFF);
        } else if (!strncmp ("hsb", tmp, 4)) {
            gdouble hsb[3];
            guchar red, green, blue;

            parse_color (tmp + 4, hsb, 3);
            
            if (hsb2rgb (hsb[0], hsb[1], hsb[2], &red, &green, &blue))
            mdvi_push_color (dvi, RGB2ULONG (red, green, blue), 0xFFFFFFFF);
        } else if (!strncmp ("cmyk", tmp, 4)) {
            gdouble cmyk[4];
            double r, g, b;
            guchar red, green, blue;
            
            parse_color (tmp + 5, cmyk, 4);

            r = 1.0 - cmyk[0] - cmyk[3];
            if (r < 0.0)
                r = 0.0;
            g = 1.0 - cmyk[1] - cmyk[3];
            if (g < 0.0)
                g = 0.0;
            b = 1.0 - cmyk[2] - cmyk[3];
            if (b < 0.0)
                b = 0.0;

            red = r * 255 + 0.5;
            green = g * 255 + 0.5;
            blue = b * 255 + 0.5;
            
            mdvi_push_color (dvi, RGB2ULONG (red, green, blue), 0xFFFFFFFF);
        } else if (!strncmp ("gray ", tmp, 5)) {
            gdouble gray;
            guchar rgb;

            parse_color (tmp + 5, &gray, 1);

            rgb = gray * 255 + 0.5;

            mdvi_push_color (dvi, RGB2ULONG (rgb, rgb, rgb), 0xFFFFFFFF);
        } 
//        else {
//            GdkColor color;
//            
//            if (gdk_color_parse (tmp, &color)) {
//                guchar red, green, blue;
//
//                red = color.red * 255 / 65535.;
//                green = color.green * 255 / 65535.;
//                blue = color.blue * 255 / 65535.;
//
//                mdvi_push_color (dvi, RGB2ULONG (red, green, blue), 0xFFFFFFFF);
//            }
//        }
    }
}


