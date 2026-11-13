/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef S40_UI_H
#define S40_UI_H
#include "wad.h"
#define S40_HUD_HEIGHT 48
void S40_MenuCharacter(int x,int y,int c);
void S40_MenuText(int x,int y,const char *text,int gold);
void S40_MenuPicture(int x,int y,const qpic_t *pic,int transparent);
void S40_MenuTranslatedPicture(int x,int y,const qpic_t *pic,const byte *translation);
void S40_DrawHelp(void);
void S40_DrawMenu(void);
void S40_DrawStatusBar(void);
void S40_DrawIntermission(void);
void S40_DrawFinale(void);
void S40_DrawFPS(unsigned tenths);
void S40_CenterMessage(const char *text,int y,int remaining);
#endif
