#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "music_keyboard/config.h"
#include "music_keyboard/platform/storage_hal.h"

typedef struct {
    int16_t *frames;
    uint32_t frame_count;
    uint32_t sample_rate_hz;
} mk_host_wav_sample_t;

static bool mk_host_should_ignore_name(const char *name) {
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

static bool mk_host_has_wav_extension(const char *name) {
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

static bool mk_host_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return false;
    }

    fclose(file);
    return true;
}

static bool mk_host_directory_exists(const char *path) {
    DIR *directory = opendir(path);

    if (directory == NULL) {
        return false;
    }

    closedir(directory);
    return true;
}

static const char *mk_host_find_first_wav_in_dir(const char *directory_path) {
    static char resolved_path[512];
    DIR *directory;
    struct dirent *entry;

    directory = opendir(directory_path);
    if (directory == NULL) {
        return NULL;
    }

    while ((entry = readdir(directory)) != NULL) {
        int write_result;

        if (mk_host_should_ignore_name(entry->d_name) || !mk_host_has_wav_extension(entry->d_name)) {
            continue;
        }

        write_result = snprintf(
            resolved_path,
            sizeof(resolved_path),
            "%s/%s",
            directory_path,
            entry->d_name
        );
        if (write_result <= 0 || (size_t)write_result >= sizeof(resolved_path)) {
            continue;
        }

        closedir(directory);
        return resolved_path;
    }

    closedir(directory);
    return NULL;
}

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

static int32_t mk_host_decode_pcm_sample(const uint8_t *bytes, uint16_t bits_per_sample) {
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

static bool mk_host_load_wav_file(const char *path, mk_host_wav_sample_t *out_sample) {
    FILE *file = NULL;
    uint8_t header[12];
    uint16_t audio_format = 0;
    uint16_t channel_count = 0;
    uint16_t bits_per_sample = 0;
    uint32_t sample_rate_hz = 0;
    uint8_t *data_bytes = NULL;
    uint32_t data_size = 0;

    memset(out_sample, 0, sizeof(*out_sample));

    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }

    if (memcmp(header, "RIFF", 4) != 0 || memcmp(&header[8], "WAVE", 4) != 0) {
        fclose(file);
        return false;
    }

    while (!feof(file)) {
        uint8_t chunk_header[8];
        uint32_t chunk_size;

        if (fread(chunk_header, 1, sizeof(chunk_header), file) != sizeof(chunk_header)) {
            break;
        }

        chunk_size = mk_read_le_u32(&chunk_header[4]);

        if (memcmp(chunk_header, "fmt ", 4) == 0) {
            uint8_t *fmt_chunk = (uint8_t *)malloc(chunk_size);
            if (fmt_chunk == NULL) {
                fclose(file);
                return false;
            }

            if (fread(fmt_chunk, 1, chunk_size, file) != chunk_size) {
                free(fmt_chunk);
                fclose(file);
                return false;
            }

            if (chunk_size >= 16u) {
                audio_format = mk_read_le_u16(&fmt_chunk[0]);
                channel_count = mk_read_le_u16(&fmt_chunk[2]);
                sample_rate_hz = mk_read_le_u32(&fmt_chunk[4]);
                bits_per_sample = mk_read_le_u16(&fmt_chunk[14]);
            }

            free(fmt_chunk);
        } else if (memcmp(chunk_header, "data", 4) == 0) {
            data_bytes = (uint8_t *)malloc(chunk_size);
            if (data_bytes == NULL) {
                fclose(file);
                return false;
            }

            if (fread(data_bytes, 1, chunk_size, file) != chunk_size) {
                free(data_bytes);
                fclose(file);
                return false;
            }
            data_size = chunk_size;
        } else if (fseek(file, (long)chunk_size, SEEK_CUR) != 0) {
            fclose(file);
            return false;
        }

        if ((chunk_size & 1u) != 0u && fseek(file, 1L, SEEK_CUR) != 0) {
            fclose(file);
            return false;
        }
    }

    fclose(file);

    if (audio_format != 1u || data_bytes == NULL || data_size == 0u || sample_rate_hz == 0u) {
        free(data_bytes);
        return false;
    }

    if (channel_count == 0u || (bits_per_sample != 8u && bits_per_sample != 16u && bits_per_sample != 24u)) {
        free(data_bytes);
        return false;
    }

    {
        uint32_t bytes_per_sample = (uint32_t)bits_per_sample / 8u;
        uint32_t bytes_per_frame = (uint32_t)channel_count * bytes_per_sample;
        uint32_t frame_count = data_size / bytes_per_frame;
        int16_t *mono_frames = (int16_t *)malloc(sizeof(int16_t) * frame_count);

        if (mono_frames == NULL) {
            free(data_bytes);
            return false;
        }

        for (uint32_t frame = 0; frame < frame_count; ++frame) {
            int32_t mix = 0;

            for (uint16_t channel = 0; channel < channel_count; ++channel) {
                uint32_t byte_index = (frame * bytes_per_frame) + (channel * bytes_per_sample);
                mix += mk_host_decode_pcm_sample(&data_bytes[byte_index], bits_per_sample);
            }

            mono_frames[frame] = (int16_t)(mix / (int32_t)channel_count);
        }

        free(data_bytes);
        out_sample->frames = mono_frames;
        out_sample->frame_count = frame_count;
        out_sample->sample_rate_hz = sample_rate_hz;
    }

    return true;
}

static bool mk_host_generate_fallback_sample(mk_host_wav_sample_t *out_sample) {
    const uint32_t frame_count = 12000u;
    int16_t *frames = (int16_t *)malloc(sizeof(int16_t) * frame_count);

    if (frames == NULL) {
        return false;
    }

    for (uint32_t i = 0; i < frame_count; ++i) {
        uint32_t period = 64u;
        int32_t square = ((i % period) < (period / 2u)) ? 15000 : -15000;
        int32_t env = (int32_t)((frame_count - i) * 32767u / frame_count);
        frames[i] = (int16_t)((square * env) / 32767);
    }

    out_sample->frames = frames;
    out_sample->frame_count = frame_count;
    out_sample->sample_rate_hz = 24000u;
    return true;
}

static void mk_host_assign_sample_to_slot(
    mk_app_t *app,
    uint8_t slot,
    const char *name,
    const mk_host_wav_sample_t *sample
) {
    if (slot >= MK_SAMPLE_SLOT_COUNT || sample == NULL) {
        return;
    }

    if (name != NULL && name[0] != '\0') {
        snprintf(app->sample_bank.slots[slot].name, sizeof(app->sample_bank.slots[slot].name), "%s", name);
    }

    app->sample_bank.slots[slot].occupied = true;
    app->sample_bank.slots[slot].pcm_frames = sample->frames;
    app->sample_bank.slots[slot].owns_pcm_frames = true;
    app->sample_bank.slots[slot].frame_count = sample->frame_count;
    app->sample_bank.slots[slot].sample_rate_hz = sample->sample_rate_hz;
    app->sample_bank.slots[slot].root_note = (slot < MK_LOADED_MELODIC_SAMPLE_COUNT) ? 61 : 36;
}

static void mk_host_assign_shared_sample_to_all_slots(
    mk_app_t *app,
    const char *name,
    const mk_host_wav_sample_t *sample
) {
    for (uint8_t slot = 0; slot < MK_SAMPLE_SLOT_COUNT; ++slot) {
        if (name != NULL && name[0] != '\0') {
            snprintf(app->sample_bank.slots[slot].name, sizeof(app->sample_bank.slots[slot].name), "%s", name);
        }

        app->sample_bank.slots[slot].occupied = true;
        app->sample_bank.slots[slot].pcm_frames = sample->frames;
        app->sample_bank.slots[slot].owns_pcm_frames = (slot == 0u);
        app->sample_bank.slots[slot].frame_count = sample->frame_count;
        app->sample_bank.slots[slot].sample_rate_hz = sample->sample_rate_hz;
        app->sample_bank.slots[slot].root_note = (slot < MK_LOADED_MELODIC_SAMPLE_COUNT) ? 61 : 36;
    }
}

static const char *mk_host_find_po33_root(void) {
    static const char *candidate_roots[] = {
        "/Users/tvd/dev/Po-33",
        "../Po-33",
        "../../Po-33",
        "../../../Po-33",
        "../../../../Po-33",
    };
    const char *env_root = getenv("MK_PO33_ROOT");
    char probe_path[512];

    if (env_root != NULL && env_root[0] != '\0' && mk_host_directory_exists(env_root)) {
        snprintf(probe_path, sizeof(probe_path), "%s/wav/sound-1.wav", env_root);
        if (mk_host_file_exists(probe_path)) {
            return env_root;
        }
    }

    for (size_t i = 0; i < (sizeof(candidate_roots) / sizeof(candidate_roots[0])); ++i) {
        snprintf(probe_path, sizeof(probe_path), "%s/wav/sound-1.wav", candidate_roots[i]);
        if (mk_host_file_exists(probe_path)) {
            return candidate_roots[i];
        }
    }

    return NULL;
}

static size_t mk_host_load_po33_bank(mk_app_t *app, const char *po33_root) {
    size_t loaded_count = 0;
    char path[512];

    if (po33_root == NULL) {
        return 0;
    }

    for (uint8_t melodic = 0; melodic < MK_LOADED_MELODIC_SAMPLE_COUNT; ++melodic) {
        mk_host_wav_sample_t sample = {0};

        snprintf(path, sizeof(path), "%s/wav/sound-%u.wav", po33_root, (unsigned)(melodic + 1u));
        if (!mk_host_load_wav_file(path, &sample)) {
            printf("[host/storage] missing melodic wav path=%s\n", path);
            continue;
        }

        mk_host_assign_sample_to_slot(app, melodic, NULL, &sample);
        ++loaded_count;
    }

    for (uint8_t kit = 0; kit < MK_LOADED_DRUM_KIT_COUNT; ++kit) {
        for (uint8_t slice = 0; slice < MK_DRUM_SLICES_PER_KIT; ++slice) {
            mk_host_wav_sample_t sample = {0};
            uint8_t slot = (uint8_t)(MK_DRUM_SAMPLE_SLOT_BASE + (kit * MK_DRUM_SLICES_PER_KIT) + slice);

            snprintf(
                path,
                sizeof(path),
                "%s/wav/sound-%u-%u.wav",
                po33_root,
                (unsigned)(9u + kit),
                (unsigned)(slice + 1u)
            );

            if (!mk_host_load_wav_file(path, &sample)) {
                printf("[host/storage] missing drum wav path=%s\n", path);
                continue;
            }

            mk_host_assign_sample_to_slot(app, slot, NULL, &sample);
            ++loaded_count;
        }
    }

    return loaded_count;
}

static const char *mk_host_find_sample_path(void) {
    static const char *candidate_paths[] = {
        "samples/source.wav",
        "samples/recording.wav",
        "samples/po33.wav",
        "../samples/source.wav",
        "../samples/recording.wav",
        "../../samples/source.wav",
        "../../samples/recording.wav",
    };
    static const char *candidate_directories[] = {
        "samples",
        "../samples",
        "../../samples",
    };
    const char *env_path = getenv("MK_SAMPLE_WAV");

    if (env_path != NULL && env_path[0] != '\0' && mk_host_file_exists(env_path)) {
        return env_path;
    }

    for (size_t i = 0; i < (sizeof(candidate_paths) / sizeof(candidate_paths[0])); ++i) {
        if (mk_host_file_exists(candidate_paths[i])) {
            return candidate_paths[i];
        }
    }

    for (size_t i = 0; i < (sizeof(candidate_directories) / sizeof(candidate_directories[0])); ++i) {
        const char *resolved = mk_host_find_first_wav_in_dir(candidate_directories[i]);
        if (resolved != NULL) {
            return resolved;
        }
    }

    return NULL;
}

bool mk_storage_hal_init(void) {
    puts("[host/storage] init");
    return true;
}

bool mk_storage_hal_load_project(mk_app_t *app) {
    const char *po33_root = mk_host_find_po33_root();
    size_t loaded_slots = 0;

    if (po33_root != NULL) {
        loaded_slots = mk_host_load_po33_bank(app, po33_root);
        printf("[host/storage] po33_root=%s loaded_slots=%zu\n", po33_root, loaded_slots);
    }

    if (loaded_slots == 0u) {
        mk_host_wav_sample_t loaded = {0};
        const char *sample_path = mk_host_find_sample_path();

        if (sample_path != NULL && mk_host_load_wav_file(sample_path, &loaded)) {
            printf(
                "[host/storage] loaded shared wav path=%s sample_rate=%u frames=%u\n",
                sample_path,
                loaded.sample_rate_hz,
                loaded.frame_count
            );
        } else {
            if (!mk_host_generate_fallback_sample(&loaded)) {
                puts("[host/storage] failed to load wav and failed to generate fallback sample");
                return false;
            }
            puts("[host/storage] wav not found, generated fallback sample");
        }

        mk_host_assign_shared_sample_to_all_slots(app, "fallback", &loaded);
    }

    return true;
}

bool mk_storage_hal_save_project(const mk_app_t *app) {
    (void)app;
    puts("[host/storage] save project (stub)");
    return true;
}
