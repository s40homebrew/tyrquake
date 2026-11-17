/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "firmware_s40.h"
#include "quakedef.h"
#include "sound.h"
#include "s40/audio.h"
#include "s40/audio_clock.h"
#include "audio/services.h"
#include "audio/commgr/driver.h"
#include "firmware_api.h"
#include <string.h>

static int16_t ring[AUDIO_BUFFER_SIZE];
static unsigned submitted;
static audio_abi audio_services;
static int resolved_open(const s40_audio_format *format) {
    return audio_cm_open(&audio_services, format);
}
static const s40_audio_driver resolved_driver = {
    resolved_open, audio_cm_write, audio_cm_queued, audio_cm_status, audio_cm_close
};
static int bind_audio(void) {
    s40_audio_bind(NULL);
    if (!audio_cm_available() || !s40_result ||
            s40_result[FW_API_DESCRIPTOR + API_AUDIO_ABI] != FW_AUDIO_ABI_DEVICE_MEDIA ||
            !audio_abi_from_services(s40_result + FW_API_DESCRIPTOR, &audio_services))
        return 0;
    /* Keep this port's established WAV stream and native volume units. The
     * shared resolver supplies addresses; transport choice belongs to the port. */
    audio_services.transport = AUDIO_TRANSPORT_WAV;
    audio_services.volume_percent = 0;
    audio_services.route = AUDIO_ROUTE_EXISTING;
    s40_audio_bind(&resolved_driver);
    return 1;
}
static s40_audio_clock play_clock;

qboolean SNDDMA_Init(dma_t *dma) {
    const s40_audio_format format={11025,2,60,160};
    if (!bind_audio()) {
        printf("Audio services unavailable; continuing silently\n");
        return false;
    }
    int status=s40_audio_open(&format);
    if (status<0) {
        printf("Audio unavailable; continuing silently (stage %d)\n",-status);
        return false;
    }
    shm=dma; shm->speed=format.rate; shm->samplepos=0;
    shm->submission_chunk=1; shm->buffer=(unsigned char *)ring;
    submitted=0;
    s40_audio_clock_reset(&play_clock,fw_os_current_time_read_in_ms(),0);
    memset(ring,0,sizeof(ring));
    printf("Native PCM: 11025 Hz, 16-bit stereo; paced, 120 ms mix / 160 ms source cap\n");
    return true;
}
int SNDDMA_GetDMAPos(void) {
    unsigned played=s40_audio_clock_update(&play_clock,
        fw_os_current_time_read_in_ms(),11025,submitted,s40_audio_queued());
    return (played*2)&(AUDIO_BUFFER_SIZE-1);
}
void SNDDMA_Submit(void) {
    if (!shm || s40_audio_status()!=1) return;
    /* The original mixer writes a stereo ring. Queue only newly painted
     * samples, including wraparound; firmware takes copies, not ring pointers. */
    unsigned end=(unsigned)paintedtime;
    if (end<submitted) { /* upstream long-session reset */
        submitted=end;
        s40_audio_clock_reset(&play_clock,fw_os_current_time_read_in_ms(),end);
    }
    while (submitted<end) {
        unsigned offset=submitted & ((AUDIO_BUFFER_SIZE/2)-1);
        unsigned count=AUDIO_BUFFER_SIZE/2-offset;
        if (count>end-submitted) count=end-submitted;
        int written=s40_audio_write(ring+offset*2,count);
        if (written<=0) break;
        submitted+=written;
    }
}
void SNDDMA_Shutdown(void) { s40_audio_close(); }
