#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <tremor/ivorbisfile.h>

#include "../env.h"

#define OUT_RATE 48000
#define GRAIN 1024
#define MAX_CLIPS 64
#define MAX_VOICES 16
#define MAX_STREAMS 4
#define MAX_WAV_BYTES (64u * 1024u * 1024u)
#define STREAM_CACHE 4096
#define AUDIO_BUFFER_COUNT 2

typedef struct
{
    int used;
    int16_t *samples;
    uint32_t frames;
} Clip;

typedef struct
{
    int active;
    int clip;
    uint32_t pos;
    float vol;
    int loop;
} Voice;

typedef struct
{
    uint16_t format;
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    long data_off;
    uint32_t data_size;
} WavInfo;

typedef enum
{
    STREAM_NONE = 0,
    STREAM_WAV = 1,
    STREAM_OGG = 2
} StreamType;

typedef struct
{
    int used;
    int playing;
    int paused;
    int loop;

    StreamType type;

    /* WAV only. OGG FILE* is owned by ov_open()/ov_clear(). */
    FILE *f;
    WavInfo info;

    OggVorbis_File ogg;
    int ogg_open;
    ogg_int64_t ogg_decode_frame;

    uint32_t rate;
    uint16_t channels;
    ogg_int64_t frames;

    double pos;
    float vol;

    ogg_int64_t cache_start;
    uint32_t cache_frames;
    int16_t cache[STREAM_CACHE * 2];
} Stream;

static Clip clips[MAX_CLIPS];
static Voice voices[MAX_VOICES];
static Stream streams[MAX_STREAMS];
static int16_t ogg_decode_buffer[STREAM_CACHE * 2];

/*
 * SceAudioOut requires the output buffer size and starting address to be
 * 64-byte aligned. Keep two hardware-facing buffers and rotate them exactly
 * like established Vita audio backends do.
 */
static int16_t audio_buffers[AUDIO_BUFFER_COUNT][GRAIN * 2]
    __attribute__((aligned(64)));

static pthread_mutex_t mutex;
static pthread_t thread;
static int mutex_init;
static int initialized;
static int port = -1;
static volatile int running;
static float master = 1.0f;

static uint16_t u16(FILE *f)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2)
        return 0;

    return b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t u32(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4)
        return 0;

    return
        b[0] |
        ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24);
}

static int16_t clamp16(int32_t v)
{
    if (v > 32767)
        return 32767;
    if (v < -32768)
        return -32768;
    return (int16_t)v;
}

static float clampv(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static int parse_wav(FILE *f, WavInfo *i, int enforce_max)
{
    char id[4];
    memset(i, 0, sizeof(*i));

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4))
        return -1;

    (void)u32(f);

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4))
        return -1;

    int fmt_found = 0;
    int data_found = 0;

    while (!fmt_found || !data_found)
    {
        if (fread(id, 1, 4, f) != 4)
            break;

        uint32_t n = u32(f);
        long p = ftell(f);

        if (!memcmp(id, "fmt ", 4))
        {
            if (n < 16)
                return -1;

            i->format = u16(f);
            i->channels = u16(f);
            i->rate = u32(f);
            (void)u32(f);
            (void)u16(f);
            i->bits = u16(f);
            fmt_found = 1;
        }
        else if (!memcmp(id, "data", 4))
        {
            i->data_off = p;
            i->data_size = n;
            data_found = 1;
        }

        if (fseek(f, p + n + (n & 1), SEEK_SET) != 0)
            return -1;
    }

    if (!fmt_found ||
        !data_found ||
        i->format != 1 ||
        i->bits != 16 ||
        (i->channels != 1 && i->channels != 2) ||
        !i->rate ||
        !i->data_size)
    {
        return -2;
    }

    if (enforce_max && i->data_size > MAX_WAV_BYTES)
        return -3;

    return 0;
}

static int load_clip(const char *path, int16_t **out, uint32_t *outframes)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -10;

    WavInfo i;
    int r = parse_wav(f, &i, 1);
    if (r < 0)
    {
        fclose(f);
        return r;
    }

    uint32_t input_frames = i.data_size / (i.channels * 2);
    if (!input_frames || fseek(f, i.data_off, SEEK_SET))
    {
        fclose(f);
        return -11;
    }

    size_t input_samples = (size_t)input_frames * i.channels;
    int16_t *input = malloc(input_samples * sizeof(int16_t));
    if (!input)
    {
        fclose(f);
        return -12;
    }

    if (fread(input, sizeof(int16_t), input_samples, f) != input_samples)
    {
        free(input);
        fclose(f);
        return -13;
    }

    fclose(f);

    uint32_t output_frames = (uint32_t)(
        ((uint64_t)input_frames * OUT_RATE + i.rate - 1) / i.rate);

    int16_t *output = malloc((size_t)output_frames * 2 * sizeof(int16_t));
    if (!output)
    {
        free(input);
        return -14;
    }

    /*
     * Resampling is done once at load time for RAM-resident SFX, never in the
     * real-time mixer path.
     */
    for (uint32_t x = 0; x < output_frames; x++)
    {
        double source_pos = (double)x * i.rate / OUT_RATE;
        uint32_t a = (uint32_t)source_pos;
        if (a >= input_frames)
            a = input_frames - 1;

        uint32_t b = a + 1 < input_frames ? a + 1 : a;
        double q = source_pos - a;

        int16_t l0;
        int16_t r0;
        int16_t l1;
        int16_t r1;

        if (i.channels == 1)
        {
            l0 = r0 = input[a];
            l1 = r1 = input[b];
        }
        else
        {
            l0 = input[a * 2];
            r0 = input[a * 2 + 1];
            l1 = input[b * 2];
            r1 = input[b * 2 + 1];
        }

        output[x * 2] = clamp16((int32_t)(l0 + (l1 - l0) * q));
        output[x * 2 + 1] = clamp16((int32_t)(r0 + (r1 - r0) * q));
    }

    free(input);

    *out = output;
    *outframes = output_frames;
    return 0;
}

static void stream_close_native(Stream *s)
{
    if (!s)
        return;

    if (s->type == STREAM_OGG && s->ogg_open)
    {
        ov_clear(&s->ogg);
        s->ogg_open = 0;
        s->f = NULL;
    }
    else if (s->f)
    {
        fclose(s->f);
        s->f = NULL;
    }
}

static int stream_fill_wav(Stream *s, ogg_int64_t frame)
{
    if (!s->f || frame < 0 || frame >= s->frames)
        return -1;

    uint32_t want = (uint32_t)(s->frames - frame);
    if (want > STREAM_CACHE)
        want = STREAM_CACHE;

    if (fseek(
            s->f,
            s->info.data_off + (long)((uint64_t)frame * s->channels * 2),
            SEEK_SET) != 0)
    {
        return -1;
    }

    if (s->channels == 2)
    {
        size_t got = fread(
            s->cache,
            sizeof(int16_t),
            (size_t)want * 2,
            s->f);

        s->cache_frames = (uint32_t)(got / 2);
    }
    else
    {
        size_t got = fread(
            s->cache,
            sizeof(int16_t),
            want,
            s->f);

        s->cache_frames = (uint32_t)got;

        /* Expand backwards so mono input can be converted in-place. */
        for (uint32_t i = s->cache_frames; i > 0; i--)
        {
            int16_t v = s->cache[i - 1];
            s->cache[(i - 1) * 2] = v;
            s->cache[(i - 1) * 2 + 1] = v;
        }
    }

    s->cache_start = frame;
    return s->cache_frames ? 0 : -1;
}

static int stream_fill_ogg(Stream *s, ogg_int64_t frame)
{
    if (!s->ogg_open || frame < 0 || frame >= s->frames)
        return -1;

    uint32_t want = (uint32_t)(s->frames - frame);
    if (want > STREAM_CACHE)
        want = STREAM_CACHE;

    if (s->ogg_decode_frame != frame)
    {
        if (ov_pcm_seek(&s->ogg, frame) < 0)
            return -1;

        s->ogg_decode_frame = frame;
    }

    s->cache_start = frame;
    s->cache_frames = 0;

    int bitstream = 0;

    while (s->cache_frames < want)
    {
        uint32_t remaining = want - s->cache_frames;
        int bytes_requested = (int)(
            (size_t)remaining * s->channels * sizeof(int16_t));

        if (bytes_requested > (int)sizeof(ogg_decode_buffer))
            bytes_requested = (int)sizeof(ogg_decode_buffer);

        long bytes = ov_read(
            &s->ogg,
            (char *)ogg_decode_buffer,
            bytes_requested,
            &bitstream);

        if (bytes == 0)
            break;

        if (bytes < 0)
        {
            if (bytes == OV_HOLE)
                continue;
            return -1;
        }

        uint32_t sample_count = (uint32_t)bytes / sizeof(int16_t);
        uint32_t decoded_frames = sample_count / s->channels;

        if (!decoded_frames)
            continue;

        if (decoded_frames > remaining)
            decoded_frames = remaining;

        uint32_t base = s->cache_frames;

        if (s->channels == 1)
        {
            for (uint32_t i = 0; i < decoded_frames; i++)
            {
                int16_t v = ogg_decode_buffer[i];
                s->cache[(base + i) * 2] = v;
                s->cache[(base + i) * 2 + 1] = v;
            }
        }
        else
        {
            memcpy(
                &s->cache[base * 2],
                ogg_decode_buffer,
                (size_t)decoded_frames * 2 * sizeof(int16_t));
        }

        s->cache_frames += decoded_frames;
        s->ogg_decode_frame += decoded_frames;
    }

    return s->cache_frames ? 0 : -1;
}

static int stream_fill(Stream *s, ogg_int64_t frame)
{
    if (frame < 0 || frame >= s->frames)
        return -1;

    if (s->cache_frames &&
        frame >= s->cache_start &&
        frame < s->cache_start + s->cache_frames)
    {
        return 0;
    }

    if (s->type == STREAM_OGG)
        return stream_fill_ogg(s, frame);

    if (s->type == STREAM_WAV)
        return stream_fill_wav(s, frame);

    return -1;
}

static int stream_sample(Stream *s, ogg_int64_t frame, int16_t *l, int16_t *r)
{
    if (stream_fill(s, frame) < 0)
        return -1;

    ogg_int64_t offset = frame - s->cache_start;
    if (offset < 0 || offset >= s->cache_frames)
        return -1;

    uint32_t idx = (uint32_t)offset;
    *l = s->cache[idx * 2];
    *r = s->cache[idx * 2 + 1];

    return 0;
}

static void mix_stream(Stream *s, int32_t *left, int32_t *right, float master_volume)
{
    if (!s->used || !s->playing || s->paused)
        return;

    if (s->pos >= (double)s->frames)
    {
        if (s->loop)
        {
            s->pos = 0;
            s->cache_frames = 0;

            if (s->type == STREAM_OGG)
                s->ogg_decode_frame = -1;
        }
        else
        {
            s->playing = 0;
            return;
        }
    }

    ogg_int64_t a = (ogg_int64_t)s->pos;
    ogg_int64_t b = a + 1 < s->frames ? a + 1 : a;

    int16_t l0;
    int16_t r0;
    int16_t l1;
    int16_t r1;

    if (stream_sample(s, a, &l0, &r0) < 0)
    {
        s->playing = 0;
        return;
    }

    if (stream_sample(s, b, &l1, &r1) < 0)
    {
        l1 = l0;
        r1 = r0;
    }

    double q = s->pos - (double)a;
    float gain = s->vol * master_volume;

    *left += (int32_t)((l0 + (l1 - l0) * q) * gain);
    *right += (int32_t)((r0 + (r1 - r0) * q) * gain);

    s->pos += (double)s->rate / OUT_RATE;
}

static void set_audio_thread_priority(void)
{
    SceUID thid = sceKernelGetThreadId();
    SceKernelThreadInfo info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);

    if (sceKernelGetThreadInfo(thid, &info) == 0)
    {
        /* Lower numerical value = higher priority on Vita. */
        if (info.currentPriority > 0)
            (void)sceKernelChangeThreadPriority(thid, info.currentPriority - 1);
    }
}

static void *mixthread(void *arg)
{
    (void)arg;

    set_audio_thread_priority();

    int buffer_index = 0;

    while (running)
    {
        int16_t *buffer = audio_buffers[buffer_index];
        memset(buffer, 0, sizeof(audio_buffers[buffer_index]));

        pthread_mutex_lock(&mutex);

        float master_volume = master;

        for (int frame = 0; frame < GRAIN; frame++)
        {
            int32_t left = 0;
            int32_t right = 0;

            for (int v = 0; v < MAX_VOICES; v++)
            {
                Voice *voice = &voices[v];

                if (!voice->active)
                    continue;

                if (voice->clip < 0 ||
                    voice->clip >= MAX_CLIPS ||
                    !clips[voice->clip].used)
                {
                    voice->active = 0;
                    continue;
                }

                Clip *clip = &clips[voice->clip];

                if (voice->pos >= clip->frames)
                {
                    if (voice->loop && clip->frames)
                    {
                        voice->pos = 0;
                    }
                    else
                    {
                        voice->active = 0;
                        continue;
                    }
                }

                size_t sample_index = (size_t)voice->pos * 2;
                float gain = voice->vol * master_volume;

                left += (int32_t)(clip->samples[sample_index] * gain);
                right += (int32_t)(clip->samples[sample_index + 1] * gain);

                voice->pos++;
            }

            for (int s = 0; s < MAX_STREAMS; s++)
                mix_stream(&streams[s], &left, &right, master_volume);

            buffer[frame * 2] = clamp16(left);
            buffer[frame * 2 + 1] = clamp16(right);
        }

        pthread_mutex_unlock(&mutex);

        int result = sceAudioOutOutput(port, buffer);
        if (result < 0)
        {
            running = 0;
            break;
        }

        buffer_index = (buffer_index + 1) % AUDIO_BUFFER_COUNT;
    }

    return NULL;
}

static int ensure(void)
{
    if (initialized)
        return 0;

    /* Belt-and-suspenders runtime check for the hardware DMA requirement. */
    if (((uintptr_t)&audio_buffers[0][0] & 63u) != 0)
        return -3;

    if (!mutex_init)
    {
        if (pthread_mutex_init(&mutex, NULL))
            return -1;
        mutex_init = 1;
    }

    port = sceAudioOutOpenPort(
        SCE_AUDIO_OUT_PORT_TYPE_MAIN,
        GRAIN,
        OUT_RATE,
        SCE_AUDIO_OUT_MODE_STEREO);

    if (port < 0)
        return port;

    int volumes[2] = {
        SCE_AUDIO_VOLUME_0DB,
        SCE_AUDIO_VOLUME_0DB
    };

    (void)sceAudioOutSetVolume(
        port,
        SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
        volumes);

    memset(clips, 0, sizeof(clips));
    memset(voices, 0, sizeof(voices));
    memset(streams, 0, sizeof(streams));
    memset(audio_buffers, 0, sizeof(audio_buffers));

    for (int i = 0; i < MAX_VOICES; i++)
        voices[i].clip = -1;

    master = 1.0f;
    running = 1;

    if (pthread_create(&thread, NULL, mixthread, NULL))
    {
        sceAudioOutReleasePort(port);
        port = -1;
        running = 0;
        return -2;
    }

    initialized = 1;
    return 0;
}

static int freeclip(void)
{
    for (int i = 0; i < MAX_CLIPS; i++)
    {
        if (!clips[i].used)
            return i;
    }

    return -1;
}

static int freevoice(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!voices[i].active)
            return i;
    }

    return -1;
}

static int freestream(void)
{
    for (int i = 0; i < MAX_STREAMS; i++)
    {
        if (!streams[i].used)
            return i;
    }

    return -1;
}

static JSValue initerr(JSContext *ctx, int result)
{
    return JS_ThrowInternalError(
        ctx,
        "Unable to initialize PS Vita audio: 0x%08X",
        (unsigned int)result);
}

static int getid(JSContext *ctx, JSValueConst value, int max, const char *name)
{
    int32_t id;

    if (JS_ToInt32(ctx, &id, value))
        return -2;

    if (id < 0 || id >= max)
    {
        JS_ThrowRangeError(ctx, "Invalid %s id", name);
        return -2;
    }

    return id;
}

static int getvol(JSContext *ctx, JSValueConst value, float *out)
{
    double d;

    if (JS_ToFloat64(ctx, &d, value))
        return -1;

    *out = clampv((float)d);
    return 0;
}

static JSValue loadwav(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "load_wav(path) expects one argument");

    int init_result = ensure();
    if (init_result < 0)
        return initerr(ctx, init_result);

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    int16_t *samples = NULL;
    uint32_t frames = 0;
    int result = load_clip(path, &samples, &frames);

    JS_FreeCString(ctx, path);

    if (result < 0)
        return JS_ThrowInternalError(ctx, "Unable to load WAV (error %d)", result);

    pthread_mutex_lock(&mutex);

    int id = freeclip();
    if (id >= 0)
    {
        clips[id].used = 1;
        clips[id].samples = samples;
        clips[id].frames = frames;
    }

    pthread_mutex_unlock(&mutex);

    if (id < 0)
    {
        free(samples);
        return JS_ThrowInternalError(ctx, "Audio clip limit reached");
    }

    return JS_NewInt32(ctx, id);
}

static JSValue freewav(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "free(clipId) expects one argument");

    int id = getid(ctx, argv[0], MAX_CLIPS, "clip");
    if (id < 0)
        return JS_EXCEPTION;

    if (!initialized)
        return JS_UNDEFINED;

    pthread_mutex_lock(&mutex);

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].active && voices[i].clip == id)
            voices[i].active = 0;
    }

    if (clips[id].used)
        free(clips[id].samples);

    memset(&clips[id], 0, sizeof(clips[id]));

    pthread_mutex_unlock(&mutex);

    return JS_UNDEFINED;
}

static JSValue play(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc < 1 || argc > 3)
        return JS_ThrowSyntaxError(ctx, "play(clipId[,volume[,loop]]) expects 1-3 arguments");

    int init_result = ensure();
    if (init_result < 0)
        return initerr(ctx, init_result);

    int id = getid(ctx, argv[0], MAX_CLIPS, "clip");
    if (id < 0)
        return JS_EXCEPTION;

    float volume = 1.0f;
    if (argc >= 2 && getvol(ctx, argv[1], &volume) < 0)
        return JS_EXCEPTION;

    int loop = argc >= 3 ? JS_ToBool(ctx, argv[2]) : 0;
    if (loop < 0)
        return JS_EXCEPTION;

    pthread_mutex_lock(&mutex);

    if (!clips[id].used)
    {
        pthread_mutex_unlock(&mutex);
        return JS_ThrowRangeError(ctx, "Invalid or freed clip id");
    }

    int voice_id = freevoice();
    if (voice_id >= 0)
    {
        voices[voice_id].active = 1;
        voices[voice_id].clip = id;
        voices[voice_id].pos = 0;
        voices[voice_id].vol = volume;
        voices[voice_id].loop = loop ? 1 : 0;
    }

    pthread_mutex_unlock(&mutex);

    return JS_NewInt32(ctx, voice_id);
}

static JSValue stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "stop(voiceId) expects one argument");

    int id = getid(ctx, argv[0], MAX_VOICES, "voice");
    if (id < 0)
        return JS_EXCEPTION;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);
        voices[id].active = 0;
        voices[id].clip = -1;
        voices[id].pos = 0;
        pthread_mutex_unlock(&mutex);
    }

    return JS_UNDEFINED;
}

static JSValue stopall(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "stop_all() expects no arguments");

    if (initialized)
    {
        pthread_mutex_lock(&mutex);

        for (int i = 0; i < MAX_VOICES; i++)
            voices[i].active = 0;

        for (int i = 0; i < MAX_STREAMS; i++)
            streams[i].playing = 0;

        pthread_mutex_unlock(&mutex);
    }

    return JS_UNDEFINED;
}

static JSValue isplaying(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "is_playing(voiceId) expects one argument");

    int id = getid(ctx, argv[0], MAX_VOICES, "voice");
    if (id < 0)
        return JS_FALSE;

    int playing = 0;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);
        playing = voices[id].active;
        pthread_mutex_unlock(&mutex);
    }

    return JS_NewBool(ctx, playing);
}

static JSValue voicevol(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 2)
        return JS_ThrowSyntaxError(ctx, "set_voice_volume(voiceId,volume) expects two arguments");

    int id = getid(ctx, argv[0], MAX_VOICES, "voice");
    if (id < 0)
        return JS_EXCEPTION;

    float volume;
    if (getvol(ctx, argv[1], &volume) < 0)
        return JS_EXCEPTION;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);
        voices[id].vol = volume;
        pthread_mutex_unlock(&mutex);
    }

    return JS_UNDEFINED;
}

static JSValue masterm(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "set_master_volume(volume) expects one argument");

    float volume;
    if (getvol(ctx, argv[0], &volume) < 0)
        return JS_EXCEPTION;

    int result = ensure();
    if (result < 0)
        return initerr(ctx, result);

    pthread_mutex_lock(&mutex);
    master = volume;
    pthread_mutex_unlock(&mutex);

    return JS_UNDEFINED;
}

static JSValue getmaster(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "get_master_volume() expects no arguments");

    float volume = master;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);
        volume = master;
        pthread_mutex_unlock(&mutex);
    }

    return JS_NewFloat64(ctx, volume);
}

static JSValue openstream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "open_stream(path) expects one argument");

    int init_result = ensure();
    if (init_result < 0)
        return initerr(ctx, init_result);

    const char *path = JS_ToCString(ctx, argv[0]);
    if (!path)
        return JS_EXCEPTION;

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "Unable to open audio stream");
    }

    uint8_t magic[4] = {0};
    if (fread(magic, 1, sizeof(magic), f) != sizeof(magic))
    {
        fclose(f);
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(ctx, "Unable to read audio stream header");
    }

    rewind(f);

    Stream candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.used = 1;
    candidate.vol = 1.0f;
    candidate.ogg_decode_frame = -1;

    if (!memcmp(magic, "OggS", 4))
    {
        int result = ov_open(f, &candidate.ogg, NULL, 0);
        if (result < 0)
        {
            fclose(f);
            JS_FreeCString(ctx, path);
            return JS_ThrowInternalError(
                ctx,
                "Unable to open OGG/Vorbis stream (error %d)",
                result);
        }

        candidate.type = STREAM_OGG;
        candidate.ogg_open = 1;
        candidate.f = NULL;

        vorbis_info *info = ov_info(&candidate.ogg, -1);
        if (!info ||
            (info->channels != 1 && info->channels != 2) ||
            info->rate <= 0)
        {
            ov_clear(&candidate.ogg);
            JS_FreeCString(ctx, path);
            return JS_ThrowInternalError(
                ctx,
                "Unsupported OGG/Vorbis stream; only mono/stereo is supported");
        }

        ogg_int64_t total = ov_pcm_total(&candidate.ogg, -1);
        if (total <= 0)
        {
            ov_clear(&candidate.ogg);
            JS_FreeCString(ctx, path);
            return JS_ThrowInternalError(
                ctx,
                "Unable to determine OGG/Vorbis stream length");
        }

        candidate.rate = (uint32_t)info->rate;
        candidate.channels = (uint16_t)info->channels;
        candidate.frames = total;
    }
    else if (!memcmp(magic, "RIFF", 4))
    {
        WavInfo info;
        int result = parse_wav(f, &info, 0);

        if (result < 0)
        {
            fclose(f);
            JS_FreeCString(ctx, path);
            return JS_ThrowInternalError(
                ctx,
                "Unsupported WAV stream (error %d)",
                result);
        }

        candidate.type = STREAM_WAV;
        candidate.f = f;
        candidate.info = info;
        candidate.rate = info.rate;
        candidate.channels = info.channels;
        candidate.frames = (ogg_int64_t)info.data_size / (info.channels * 2);
    }
    else
    {
        fclose(f);
        JS_FreeCString(ctx, path);
        return JS_ThrowInternalError(
            ctx,
            "Unsupported stream format; expected PCM WAV or OGG/Vorbis");
    }

    JS_FreeCString(ctx, path);

    pthread_mutex_lock(&mutex);

    int id = freestream();
    if (id >= 0)
        streams[id] = candidate;

    pthread_mutex_unlock(&mutex);

    if (id < 0)
    {
        stream_close_native(&candidate);
        return JS_ThrowInternalError(
            ctx,
            "Audio stream limit reached (%d)",
            MAX_STREAMS);
    }

    return JS_NewInt32(ctx, id);
}

static JSValue closestream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "close_stream(streamId) expects one argument");

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);
        stream_close_native(&streams[id]);
        memset(&streams[id], 0, sizeof(streams[id]));
        pthread_mutex_unlock(&mutex);
    }

    return JS_UNDEFINED;
}

static JSValue playstream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc < 1 || argc > 3)
        return JS_ThrowSyntaxError(ctx, "play_stream(streamId[,volume[,loop]]) expects 1-3 arguments");

    if (!initialized)
        return JS_ThrowInternalError(ctx, "Audio is not initialized; open a stream first");

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    float volume = 1.0f;
    if (argc >= 2 && getvol(ctx, argv[1], &volume) < 0)
        return JS_EXCEPTION;

    int loop = argc >= 3 ? JS_ToBool(ctx, argv[2]) : 0;
    if (loop < 0)
        return JS_EXCEPTION;

    pthread_mutex_lock(&mutex);

    if (!streams[id].used)
    {
        pthread_mutex_unlock(&mutex);
        return JS_ThrowRangeError(ctx, "Invalid stream id");
    }

    streams[id].playing = 1;
    streams[id].paused = 0;
    streams[id].loop = loop ? 1 : 0;
    streams[id].vol = volume;

    pthread_mutex_unlock(&mutex);

    return JS_UNDEFINED;
}

static JSValue streamcmd(JSContext *ctx, int argc, JSValueConst *argv, int cmd)
{
    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "stream command expects one streamId");

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    if (initialized)
    {
        pthread_mutex_lock(&mutex);

        Stream *stream = &streams[id];

        if (cmd == 0)
        {
            stream->paused = 1;
        }
        else if (cmd == 1)
        {
            stream->paused = 0;
        }
        else if (cmd == 2)
        {
            stream->playing = 0;
            stream->paused = 0;
            stream->pos = 0;
            stream->cache_frames = 0;

            if (stream->type == STREAM_OGG)
                stream->ogg_decode_frame = -1;
        }

        pthread_mutex_unlock(&mutex);
    }

    return JS_UNDEFINED;
}

static JSValue pausestream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return streamcmd(ctx, argc, argv, 0);
}

static JSValue resumestream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return streamcmd(ctx, argc, argv, 1);
}

static JSValue stopstream(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    return streamcmd(ctx, argc, argv, 2);
}

static JSValue streamplaying(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "is_stream_playing(streamId) expects one argument");

    if (!initialized)
        return JS_FALSE;

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_FALSE;

    pthread_mutex_lock(&mutex);
    int playing = streams[id].used && streams[id].playing && !streams[id].paused;
    pthread_mutex_unlock(&mutex);

    return JS_NewBool(ctx, playing);
}

static JSValue streamvol(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 2)
        return JS_ThrowSyntaxError(ctx, "set_stream_volume(streamId,volume) expects two arguments");

    if (!initialized)
        return JS_ThrowInternalError(ctx, "Audio is not initialized; open a stream first");

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    float volume;
    if (getvol(ctx, argv[1], &volume) < 0)
        return JS_EXCEPTION;

    pthread_mutex_lock(&mutex);
    streams[id].vol = volume;
    pthread_mutex_unlock(&mutex);

    return JS_UNDEFINED;
}

static JSValue streamseek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;

    if (argc != 2)
        return JS_ThrowSyntaxError(ctx, "seek_stream(streamId,seconds) expects two arguments");

    if (!initialized)
        return JS_ThrowInternalError(ctx, "Audio is not initialized; open a stream first");

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    double seconds;
    if (JS_ToFloat64(ctx, &seconds, argv[1]))
        return JS_EXCEPTION;

    if (seconds < 0)
        seconds = 0;

    pthread_mutex_lock(&mutex);

    Stream *stream = &streams[id];
    double position = seconds * stream->rate;

    if (position > (double)stream->frames)
        position = (double)stream->frames;

    stream->pos = position;
    stream->cache_frames = 0;

    if (stream->type == STREAM_OGG)
        stream->ogg_decode_frame = -1;

    pthread_mutex_unlock(&mutex);

    return JS_UNDEFINED;
}

static JSValue streamtime(
    JSContext *ctx,
    JSValueConst this_val,
    int argc,
    JSValueConst *argv,
    int duration)
{
    (void)this_val;

    if (argc != 1)
        return JS_ThrowSyntaxError(ctx, "stream getter expects one streamId");

    if (!initialized)
        return JS_NewFloat64(ctx, 0);

    int id = getid(ctx, argv[0], MAX_STREAMS, "stream");
    if (id < 0)
        return JS_EXCEPTION;

    pthread_mutex_lock(&mutex);

    Stream *stream = &streams[id];
    double value = stream->rate
        ? (duration
            ? (double)stream->frames / stream->rate
            : stream->pos / stream->rate)
        : 0;

    pthread_mutex_unlock(&mutex);

    return JS_NewFloat64(ctx, value);
}

static JSValue streampos(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return streamtime(ctx, this_val, argc, argv, 0);
}

static JSValue streamdur(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return streamtime(ctx, this_val, argc, argv, 1);
}

static void shutdown_audio(void)
{
    if (!initialized)
        return;

    running = 0;
    pthread_join(thread, NULL);

    pthread_mutex_lock(&mutex);

    for (int i = 0; i < MAX_CLIPS; i++)
    {
        if (clips[i].used)
            free(clips[i].samples);
    }

    for (int i = 0; i < MAX_STREAMS; i++)
    {
        if (streams[i].used)
            stream_close_native(&streams[i]);
    }

    memset(clips, 0, sizeof(clips));
    memset(voices, 0, sizeof(voices));
    memset(streams, 0, sizeof(streams));

    pthread_mutex_unlock(&mutex);

    if (port >= 0)
        sceAudioOutReleasePort(port);

    port = -1;
    initialized = 0;

    if (mutex_init)
    {
        pthread_mutex_destroy(&mutex);
        mutex_init = 0;
    }
}

static JSValue term(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    (void)this_val;
    (void)argv;

    if (argc != 0)
        return JS_ThrowSyntaxError(ctx, "term() expects no arguments");

    shutdown_audio();
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry funcs[] = {
    JS_CFUNC_DEF("load_wav", 1, loadwav),
    JS_CFUNC_DEF("free", 1, freewav),
    JS_CFUNC_DEF("play", 3, play),
    JS_CFUNC_DEF("stop", 1, stop),
    JS_CFUNC_DEF("stop_all", 0, stopall),
    JS_CFUNC_DEF("is_playing", 1, isplaying),
    JS_CFUNC_DEF("set_voice_volume", 2, voicevol),
    JS_CFUNC_DEF("set_master_volume", 1, masterm),
    JS_CFUNC_DEF("get_master_volume", 0, getmaster),
    JS_CFUNC_DEF("open_stream", 1, openstream),
    JS_CFUNC_DEF("close_stream", 1, closestream),
    JS_CFUNC_DEF("play_stream", 3, playstream),
    JS_CFUNC_DEF("pause_stream", 1, pausestream),
    JS_CFUNC_DEF("resume_stream", 1, resumestream),
    JS_CFUNC_DEF("stop_stream", 1, stopstream),
    JS_CFUNC_DEF("is_stream_playing", 1, streamplaying),
    JS_CFUNC_DEF("set_stream_volume", 2, streamvol),
    JS_CFUNC_DEF("seek_stream", 2, streamseek),
    JS_CFUNC_DEF("get_stream_position", 1, streampos),
    JS_CFUNC_DEF("get_stream_duration", 1, streamdur),
    JS_CFUNC_DEF("term", 0, term)
};

static int initmod(JSContext *ctx, JSModuleDef *module)
{
    return JS_SetModuleExportList(ctx, module, funcs, countof(funcs));
}

JSModuleDef *vitajs_audio_init(JSContext *ctx)
{
    return vitajs_push_module(
        ctx,
        initmod,
        funcs,
        countof(funcs),
        "Audio");
}
