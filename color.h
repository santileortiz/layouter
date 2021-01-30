/*
 * Copyright (C) 2020 Santiago León O.
 */

#define RGBA DVEC4
#define RGB(r,g,b) DVEC4(r,g,b,1)
#define ARGS_RGBA(c) (c).r, (c).g, (c).b, (c).a
#define ARGS_RGB(c) (c.r), (c).g, (c).b
#define RGB_HEX(hex) DVEC4(((double)(((hex)&0xFF0000) >> 16))/255, \
                           ((double)(((hex)&0x00FF00) >>  8))/255, \
                           ((double)((hex)&0x0000FF))/255, 1)

#define PHI_INV 0.618033988749895
#define COLOR_OFFSET 0.4

void rgba_to_hsla (dvec4 *rgba, dvec4 *hsla)
{
    double max = MAX(MAX(rgba->r,rgba->g),rgba->b);
    double min = MIN(MIN(rgba->r,rgba->g),rgba->b);

    // NOTE: This is the same for HSV
    if (max == min) {
        // NOTE: we define this, in reality H is undefined
        // in this case.
        hsla->h = INFINITY;
    } else if (max == rgba->r) {
        hsla->h = (60*(rgba->r-rgba->b)/(max-min) + 360);
        if (hsla->h > 360) {
            hsla->h -= 360;
        }
    } else if (max == rgba->g) {
        hsla->h = (60*(rgba->b-rgba->r)/(max-min) + 120);
    } else { // if (max == rgba->b)
        hsla->h = (60*(rgba->r-rgba->g)/(max-min) + 240);
    }

    hsla->h /= 360; // h\in[0,1]

    hsla->l = (max+min)/2;

    if (max == min) {
        hsla->s = 0;
    } else if (hsla->l <= 0.5) {
        hsla->s = (max-min)/(2*hsla->l);
    } else {
        hsla->s = (max-min)/(2-2*hsla->l);
    }

    hsla->a = rgba->a;
}

void hsla_to_rgba (dvec4 *hsla, dvec4 *rgba)
{
    double h_pr = hsla->h*6;
    double chroma = (1-fabs(2*hsla->l-1)) * hsla->s;
    // Alternative version can be:
    //
    //double chroma;
    //if (hsla->l <= 0.5) {
    //    chroma = (2*hsla->l)*hsla->s;
    //} else {
    //    chroma = (2-2*hsla->l)*hsla->s;
    //}

    double h_pr_mod_2 = ((int)h_pr)%2+(h_pr-(int)h_pr);
    double x = chroma*(1-fabs(h_pr_mod_2-1));
    double m = hsla->l - chroma/2;

    *rgba = DVEC4(0,0,0,0);
    if (h_pr == INFINITY) {
        rgba->r = 0;
        rgba->g = 0;
        rgba->b = 0;
    } else if (h_pr < 1) {
        rgba->r = chroma;
        rgba->g = x;
        rgba->b = 0;
    } else if (h_pr < 2) {
        rgba->r = x;
        rgba->g = chroma;
        rgba->b = 0;
    } else if (h_pr < 3) {
        rgba->r = 0;
        rgba->g = chroma;
        rgba->b = x;
    } else if (h_pr < 4) {
        rgba->r = 0;
        rgba->g = x;
        rgba->b = chroma;
    } else if (h_pr < 5) {
        rgba->r = x;
        rgba->g = 0;
        rgba->b = chroma;
    } else if (h_pr < 6) {
        rgba->r = chroma;
        rgba->g = 0;
        rgba->b = x;
    }
    rgba->r += m;
    rgba->g += m;
    rgba->b += m;
    rgba->a = hsla->a;
}

static inline
dvec4 shade (dvec4 *in, double f)
{
    dvec4 temp;
    rgba_to_hsla (in, &temp);

    temp.l *= f;
    temp.l = temp.l>1? 1 : temp.l;
    temp.s *= f;
    temp.s = temp.s>1? 1 : temp.s;

    dvec4 ret;
    hsla_to_rgba (&temp, &ret);
    return ret;
}

static inline
dvec4 alpha (dvec4 c, double f)
{
    dvec4 ret = c;
    ret.a *= f;
    return ret;
}

static inline
dvec4 mix (dvec4 *c1, dvec4 *c2, double f)
{
    dvec4 ret;
    int i;
    for (i=0; i<4; i++) {
        ret.E[i] = CLAMP (c1->E[i] + (c2->E[i]-c1->E[i])*f, 0, 1);
    }
    return ret;
}

void hsv_to_rgb (dvec3 hsv, dvec3 *rgb)
{
    double h = hsv.E[0];
    double s = hsv.E[1];
    double v = hsv.E[2];
    assert (h <= 1 && s <= 1 && v <= 1);

    int h_i = (int)(h*6);
    double frac = h*6 - h_i;

    double m = v * (1 - s);
    double desc = v * (1 - frac*s);
    double asc = v * (1 - (1 - frac) * s);

    switch (h_i) {
        case 0:
            rgb->r = v;
            rgb->g = asc;
            rgb->b = m;
            break;
        case 1:
            rgb->r = desc;
            rgb->g = v;
            rgb->b = m;
            break;
        case 2:
            rgb->r = m;
            rgb->g = v;
            rgb->b = asc;
            break;
        case 3:
            rgb->r = m;
            rgb->g = desc;
            rgb->b = v;
            break;
        case 4:
            rgb->r = asc;
            rgb->g = m;
            rgb->b = v;
            break;
        case 5:
            rgb->r = v;
            rgb->g = m;
            rgb->b = desc;
            break;
    }
}

dvec3 color_palette[] = {
    {{0.058824, 0.615686, 0.345098}}, //Green
    {{0.254902, 0.517647, 0.952941}}, //Blue
    {{0.858824, 0.266667, 0.215686}}, //Red
    {{0.956863, 0.705882, 0.000000}}, //Yellow
    {{0.666667, 0.274510, 0.733333}}, //Purple
    {{1.000000, 0.435294, 0.258824}}, //Deep Orange
    {{0.615686, 0.611765, 0.137255}}, //Lime
    {{0.937255, 0.380392, 0.568627}}, //Pink
    {{0.356863, 0.415686, 0.749020}}, //Indigo
    {{0.000000, 0.670588, 0.752941}}, //Teal
    {{0.756863, 0.090196, 0.352941}}, //Deep Pink
    {{0.619608, 0.619608, 0.619608}}, //Gray
    {{0.000000, 0.470588, 0.415686}}};//Deep Teal

void get_next_color (dvec3 *color)
{
    static double h = COLOR_OFFSET;
    static uint32_t palette_idx = 0;

    // Reset color palette
    if (color == NULL) {
        palette_idx = 0;
        h = COLOR_OFFSET;
        return;
    }

    if (palette_idx < ARRAY_SIZE(color_palette)) {
        *color = color_palette[palette_idx];
        palette_idx++;
    } else {
        h += PHI_INV;
        if (h>1) {
            h -= 1;
        }
        hsv_to_rgb (DVEC3(h, 0.8, 0.7), color);
    }
}

