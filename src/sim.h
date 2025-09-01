#ifndef SIM_H
#define SIM_H

typedef struct
{
    float x, y, r;                // posición y radio (px)
    unsigned char r8, g8, b8, a8; // color/alpha 8-bit
} DrawItem;

#endif
