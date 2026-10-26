// Modified e-paper initialisation for InkBlink, derived from the board manufacturer's
// sample driver. Changes: initialisation for the JD79661 controller of hardware V1.2 and
// EPD_DisplayImageFast() (partial refresh).
// This modified version is not made, checked or supported by the board manufacturer.
// Origin and permission: see THIRD_PARTY_NOTICES.md.
#ifndef _EPD_INIT_H_
#define _EPD_INIT_H_

#include "spi.h"

/* e-paper display orientation */
#define USE_HORIZONTIAL 2 /* 0 1 2 3 */
#if USE_HORIZONTIAL == 0 | USE_HORIZONTIAL == 2
#define EPD_W 250
#define EPD_H 128
#define ALLSCREEN_BYTES ((EPD_H % 8) ? (EPD_H / 8 + 1) : (EPD_H / 8)) * EPD_W
#else
#define EPD_W 122
#define EPD_H 250
#define ALLSCREEN_BYTES ((EPD_W % 8) ? (EPD_W / 8 + 1) : (EPD_W / 8)) * EPD_H
#endif
/* colour values */
/* R24H: black pixel = 0, white pixel = 1 */
#define WHITE 0xFF
#define BLACK 0x00

/* waveform LUTs (declared here, defined at the end of the .cpp file) */
extern const unsigned char lut_R20_GC[];
extern const unsigned char lut_R21_GC[];
extern const unsigned char lut_R22_GC[];
extern const unsigned char lut_R23_GC[];
extern const unsigned char lut_R24_GC[];

extern const unsigned char lut_R20_DU[];
extern const unsigned char lut_R21_DU[];
extern const unsigned char lut_R22_DU[];
extern const unsigned char lut_R23_DU[];
extern const unsigned char lut_R24_DU[];

/* functions */
void EPD_READBUSY(void);
void EPD_HW_SW_RESET(void);
void EPD_Sleep(void);
void EPD_Update(void);
void EPD_Update_Fast(void);
void EPD_PartUpdate(void);
void EPD_Clear(void);
void EPD_ALL_Fill(uint8_t color);
void EPD_DisplayImage(const uint8_t *ImageBW);
void EPD_DisplayImageFast(const uint8_t *newImg, const uint8_t *oldImg);   /* szybkie odswiezenie (przebieg DU) */
void EPD_Init(void);
void EPD_HW_Init_Fast(void);
void lut_GC(void);                                   
void lut_DU(void);                                     
#endif
