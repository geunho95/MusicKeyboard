#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ff.h"

#include "music_keyboard/config.h"
#include "music_keyboard/platform/storage_hal.h"

#define MK_RP2350_FALLBACK_SAMPLE_FRAMES 12000u
#define MK_RP2350_MAX_SAMPLE_FRAMES 65536u

static int16_t g_fallback_sample[MK_RP2350_FALLBACK_SAMPLE_FRAMES];
static int16_t g_sd_sample_frames[MK_RP2350_MAX_SAMPLE_FRAMES];
static bool g_fallback_ready = false;
static FATFS g_fatfs;
static bool g_storage_mounted = false;

typedef struct {
    const int16_t *frames;
    uint32_t frame_count;
    uint32_t sample_rate_hz;
    bool from_sd;
} mk_rp2350_sample_t;

static uint16_t mk_read_le_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static uint32_t mk_read_le_u32(const uint8_t *bytes) {
    return (uint32_t)(
        bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24)
    );
}

static uint32_t mk_min_u32(uint32_t lhs, uint32_t rhs) {
    return (lhs < rhs) ? lhs : rhs;
}

static int32_t mk_decode_pcm_sample(const uint8_t *bytes, uint16_t bits_per_sample) {
    if (bits_per_sample == 24u) {
        int32_t sample_value =
            (int32_t)(bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16));
        if ((sample_value & 0x800000) != 0) {
            sample_value |= ~0x00ffffff;
        }
        return sample_value >> 8;
    }

    if (bits_per_sample == 16u) {
        return (int16_t)mk_read_le_u16(bytes);
    }

    return ((int32_t)bytes[0] - 128) << 8;
}

static bool mk_should_ignore_name(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return true;
    }

    if (name[0] == '.') {
        return true;
    }

    if (strncmp(name, "._", 2) == 0) {
        return true;
    }

    return false;
}

static bool mk_has_wav_extension(const char *name) {
    size_t length;

    if (name == NULL) {
        return false;
    }

    length = strlen(name);
    if (length < 4u) {
        return false;
    }

    return strcmp(&name[length - 4u], ".wav") == 0 || strcmp(&name[length - 4u], ".WAV") == 0;
}

static bool mk_read_exact(FIL *file, void *buffer, uint32_t byte_count) {
    UINT bytes_read = 0u;

    if (f_read(file, buffer, byte_count, &bytes_read) != FR_OK) {
        return false;
    }

    return bytes_read == byte_count;
}

static bool mk_find_wav_path(char *path_buffer, size_t path_buffer_size) {
    static const char *preferred_paths[] = {
        "0:/source.wav",
        "0:/recording.wav",
        "0:/po33.wav",
        "0:/sample.wav",
        "0:/samples/source.wav",
        "0:/samples/recording.wav",
        "0:/samples/sound-1.wav",
    };
    FILINFO info;
    DIR directory;
    FRESULT result;

    for (size_t i = 0; i < (sizeof(preferred_paths) / sizeof(preferred_paths[0])); ++i) {
        FIL file;

        result = f_open(&file, preferred_paths[i], FA_READ);
        if (result == FR_OK) {
            f_close(&file);
            snprintf(path_buffer, path_buffer_size, "%s", preferred_paths[i]);
            return true;
        }
    }

    memset(&info, 0, sizeof(info));
    result = f_opendir(&directory, "0:/");
    if (result != FR_OK) {
        return false;
    }

    while (true) {
        const char *candidate_name;
        int written;

        memset(&info, 0, sizeof(info));
        result = f_readdir(&directory, &info);
        if (result != FR_OK || info.fname[0] == '\0') {
            break;
        }

        if ((info.fattrib & (AM_DIR | AM_HID | AM_SYS)) != 0u) {
            continue;
        }

        candidate_name = info.fname;
        if (candidate_name[0] == '\0') {
            candidate_name = info.altname;
        }

        if (mk_should_ignore_name(candidate_name) || !mk_has_wav_extension(candidate_name)) {
            continue;
        }

        written = snprintf(path_buffer, path_buffer_size, "0:/%s", candidate_name);
        if (written > 0 && (size_t)written < path_buffer_size) {
            f_closedir(&directory);
            return true;
        }
    }

    f_closedir(&directory);
    return false;
}

static bool mk_load_wav_from_fatfs(const char *path, mk_rp2350_sample_t *out_sample) {
    uint8_t riff_header[12];
    uint16_t audio_format = 0u;
    uint16_t channel_count = 0u;
    uint16_t bits_per_sample = 0u;
    uint32_t sample_rate_hz = 0u;
    uint32_t data_size = 0u;
    FSIZE_t data_offset = 0u;
    FIL file;
    FRESULT result;

    memset(out_sample, 0, sizeof(*out_sample));

    result = f_open(&file, path, FA_READ);
    if (result != FR_OK) {
        return false;
    }

    if (!mk_read_exact(&file, riff_header, sizeof(riff_header))) {
        f_close(&file);
        return false;
    }

    if (memcmp(riff_header, "RIFF", 4) != 0 || memcmp(&riff_header[8], "WAVE", 4) != 0) {
        f_close(&file);
        return false;
    }

    while (f_tell(&file) + 8u <= f_size(&file)) {
        uint8_t chunk_header[8];
        uint32_t chunk_size;
        FSIZE_t chunk_data_offset;
        uint32_t padded_chunk_size;

        if (!mk_read_exact(&file, chunk_header, sizeof(chunk_header))) {
            break;
        }

        chunk_size = mk_read_le_u32(&chunk_header[4]);
        padded_chunk_size = chunk_size + (chunk_size & 1u);
        chunk_data_offset = f_tell(&file);

        if (memcmp(chunk_header, "fmt ", 4) == 0 && chunk_size >= 16u) {
            uint8_t fmt_header[16];

            if (!mk_read_exact(&file, fmt_header, sizeof(fmt_header))) {
                f_close(&file);
                return false;
            }

            audio_format = mk_read_le_u16(&fmt_header[0]);
            channel_count = mk_read_le_u16(&fmt_header[2]);
            sample_rate_hz = mk_read_le_u32(&fmt_header[4]);
            bits_per_sample = mk_read_le_u16(&fmt_header[14]);

            if (padded_chunk_size > sizeof(fmt_header) &&
                f_lseek(&file, chunk_data_offset + padded_chunk_size) != FR_OK) {
                f_close(&file);
                return false;
            }
        } else if (memcmp(chunk_header, "data", 4) == 0) {
            data_size = chunk_size;
            data_offset = chunk_data_offset;

            if (f_lseek(&file, chunk_data_offset + padded_chunk_size) != FR_OK) {
                f_close(&file);
                return false;
            }
        } else if (f_lseek(&file, chunk_data_offset + padded_chunk_size) != FR_OK) {
            f_close(&file);
            return false;
        }
    }

    if (audio_format != 1u || channel_count == 0u || sample_rate_hz == 0u ||
        (bits_per_sample != 8u && bits_per_sample != 16u && bits_per_sample != 24u) ||
        data_size == 0u || data_offset == 0u) {
        f_close(&file);
        return false;
    }

    {
        uint32_t bytes_per_sample = (uint32_t)bits_per_sample / 8u;
        uint32_t bytes_per_frame = (uint32_t)channel_count * bytes_per_sample;
        uint32_t frame_count = data_size / bytes_per_frame;
        uint32_t frames_to_load = mk_min_u32(frame_count, MK_RP2350_MAX_SAMPLE_FRAMES);
        uint32_t remaining_frames = frames_to_load;
        uint8_t raw_buffer[512];

        if (f_lseek(&file, data_offset) != FR_OK) {
            f_close(&file);
            return false;
        }

        while (remaining_frames > 0u) {
            uint32_t max_frames_in_chunk = sizeof(raw_buffer) / bytes_per_frame;
            uint32_t chunk_frames = mk_min_u32(remaining_frames, max_frames_in_chunk);
            uint32_t bytes_to_read = chunk_frames * bytes_per_frame;
            UINT bytes_read = 0u;

            if (f_read(&file, raw_buffer, bytes_to_read, &bytes_read) != FR_OK || bytes_read != bytes_to_read) {
                f_close(&file);
                return false;
            }

            for (uint32_t frame = 0; frame < chunk_frames; ++frame) {
                int32_t mixed = 0;

                for (uint16_t channel = 0; channel < channel_count; ++channel) {
                    uint32_t byte_index = (frame * bytes_per_frame) + (channel * bytes_per_sample);
                    int32_t sample_value;

                    sample_value = mk_decode_pcm_sample(&raw_buffer[byte_index], bits_per_sample);
                    mixed += sample_value;
                }

                g_sd_sample_frames[frames_to_load - remaining_frames + frame] =
                    (int16_t)(mixed / (int32_t)channel_count);
            }

            remaining_frames -= chunk_frames;
        }

        f_close(&file);

        out_sample->frames = g_sd_sample_frames;
        out_sample->frame_count = frames_to_load;
        out_sample->sample_rate_hz = sample_rate_hz;
        out_sample->from_sd = true;

        if (frames_to_load < frame_count) {
            printf(
                "[rp2350/storage] wav truncated path=%s loaded=%lu total=%lu\n",
                path,
                (unsigned long)frames_to_load,
                (unsigned long)frame_count
            );
        }
    }

    return true;
}

static void mk_storage_generate_fallback_sample(void) {
    if (g_fallback_ready) {
        return;
    }

    for (uint32_t i = 0; i < MK_RP2350_FALLBACK_SAMPLE_FRAMES; ++i) {
        uint32_t period = 64u;
        int32_t square = ((i % period) < (period / 2u)) ? 15000 : -15000;
        int32_t env = (int32_t)((MK_RP2350_FALLBACK_SAMPLE_FRAMES - i) * 32767u /
                                MK_RP2350_FALLBACK_SAMPLE_FRAMES);
        g_fallback_sample[i] = (int16_t)((square * env) / 32767);
    }

    g_fallback_ready = true;
}

bool mk_storage_hal_init(void) {
    mk_storage_generate_fallback_sample();

    if (f_mount(&g_fatfs, "0:", 1) == FR_OK) {
        g_storage_mounted = true;
        puts("[rp2350/storage] TF card mounted");
    } else {
        g_storage_mounted = false;
        puts("[rp2350/storage] TF mount failed, using fallback sample");
    }

    return true;
}

bool mk_storage_hal_load_project(mk_app_t *app) {
    mk_rp2350_sample_t sample = {
        .frames = g_fallback_sample,
        .frame_count = MK_RP2350_FALLBACK_SAMPLE_FRAMES,
        .sample_rate_hz = MK_DEFAULT_AUDIO_SAMPLE_RATE,
        .from_sd = false,
    };
    char sample_path[300];

    if (g_storage_mounted && mk_find_wav_path(sample_path, sizeof(sample_path))) {
        if (mk_load_wav_from_fatfs(sample_path, &sample)) {
            printf(
                "[rp2350/storage] loaded wav path=%s sample_rate=%lu frames=%lu\n",
                sample_path,
                (unsigned long)sample.sample_rate_hz,
                (unsigned long)sample.frame_count
            );
        } else {
            printf("[rp2350/storage] failed to parse wav path=%s, using fallback\n", sample_path);
        }
    }

    for (uint8_t slot = 0; slot < MK_SAMPLE_SLOT_COUNT; ++slot) {
        app->sample_bank.slots[slot].occupied = true;
        app->sample_bank.slots[slot].pcm_frames = sample.frames;
        app->sample_bank.slots[slot].owns_pcm_frames = false;
        app->sample_bank.slots[slot].frame_count = sample.frame_count;
        app->sample_bank.slots[slot].sample_rate_hz = sample.sample_rate_hz;
        app->sample_bank.slots[slot].root_note = (slot < MK_LOADED_MELODIC_SAMPLE_COUNT) ? 61 : 36;
    }

    if (!sample.from_sd) {
        puts("[rp2350/storage] loaded fallback project");
    }

    return true;
}

bool mk_storage_hal_save_project(const mk_app_t *app) {
    (void)app;
    puts("[rp2350/storage] save stub");
    return true;
}

uint8_t mk_storage_hal_list_samples(char names[][32], uint8_t max_count) {
    if (!g_storage_mounted) return 0;
    DIR dir;
    FILINFO info;
    uint8_t count = 0;
    if (f_opendir(&dir, "0:/samples") != FR_OK) return 0;
    while (count < max_count) {
        if (f_readdir(&dir, &info) != FR_OK || info.fname[0] == '\0') break;
        if ((info.fattrib & (AM_DIR | AM_HID | AM_SYS)) != 0u) continue;
        if (!mk_has_wav_extension(info.fname)) continue;
        strncpy(names[count], info.fname, 31);
        names[count][31] = '\0';
        count++;
    }
    f_closedir(&dir);
    return count;
}

bool mk_storage_hal_load_sample_by_name(mk_app_t *app, const char *filename) {
    char path[64];
    snprintf(path, sizeof(path), "0:/samples/%s", filename);
    mk_rp2350_sample_t sample = {
        .frames = g_fallback_sample,
        .frame_count = MK_RP2350_FALLBACK_SAMPLE_FRAMES,
        .sample_rate_hz = MK_DEFAULT_AUDIO_SAMPLE_RATE,
        .from_sd = false,
    };
    if (!mk_load_wav_from_fatfs(path, &sample)) {
        printf("[rp2350/storage] failed to load %s\n", path);
        return false;
    }
    printf("[rp2350/storage] loaded %s frames=%lu\n", path, (unsigned long)sample.frame_count);
    for (uint8_t slot = 0; slot < MK_SAMPLE_SLOT_COUNT; ++slot) {
        app->sample_bank.slots[slot].occupied = true;
        app->sample_bank.slots[slot].pcm_frames = sample.frames;
        app->sample_bank.slots[slot].owns_pcm_frames = false;
        app->sample_bank.slots[slot].frame_count = sample.frame_count;
        app->sample_bank.slots[slot].sample_rate_hz = sample.sample_rate_hz;
        app->sample_bank.slots[slot].root_note = (slot < MK_LOADED_MELODIC_SAMPLE_COUNT) ? 61 : 36;
    }
    return true;
}
