/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "quakedef.h"
#include "sound.h"
#include "cdaudio.h"
#include "bgmusic.h"
cvar_t bgm_extmusic={"bgm_extmusic","0"};
int CDAudio_Init(void) { return -1; }
int CDAudio_Play(byte t,qboolean l) { return -1; }
void CDAudio_Stop(void) {}
void CDAudio_Pause(void) {}
void CDAudio_Resume(void) {}
void CDAudio_InvalidateDisk(void) {}
void CDAudio_Shutdown(void) {}
void CDAudio_Update(void) {}
qboolean BGM_Init(void) { return false; }
void BGM_Shutdown(void) {}
void BGM_Play(const char *s) {}
void BGM_Stop(void) {}
void BGM_Update(void) {}
void BGM_Pause(void) {}
void BGM_Resume(void) {}
void BGM_PlayCDtrack(byte t,qboolean l) {}
