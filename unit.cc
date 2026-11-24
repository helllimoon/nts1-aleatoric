#include "unit_osc.h"
#include <cmath>

#define MAX_VOICES 6
#define SAMPLE_RATE 48000.0f
#define INV_SAMPLE_RATE (1.0f / 48000.0f)
#define TWO_PI 6.28318530718f
#define LN_1000 6.90775527898f

enum {
    PARAM_RATE = 0,
    PARAM_SPAN,
    PARAM_MODE,
    PARAM_RHYT,
    PARAM_REST,
    PARAM_LEN,
    PARAM_LENR,
    PARAM_CHRD,
    PARAM_SCAL,
    PARAM_TONE,
    NUM_PARAMS
};

struct Voice {
    bool active;
    bool in_attack;
    float phase;
    float phase_inc;
    float env;
    float env_decay;
    float vel;
    float fm_depth;
};

static unit_runtime_desc_t s_desc;
static int32_t s_param_cache[NUM_PARAMS];

static Voice s_voices[MAX_VOICES];
static uint32_t s_rng = 0x12345678U;
static uint32_t s_step_timer = 0;
static float s_base_note = 60.0f;
static int32_t s_active_notes = 0;

static float p_rate = 0.5f;
static float p_span = 0.5f;
static int32_t p_mode = 0;
static float p_rhyt = 0.5f;
static float p_rest = 0.15f;
static float p_len  = 0.45f;
static float p_lenr = 0.70f;
static float p_chrd = 0.50f;
static int32_t p_scal = 2;
static float p_tone = 0.25f;

static const char * const s_mode_names[]  = {"FLOW", "ONE"};
static const char * const s_scale_names[] = {"MICR", "CHRM", "PENT", "MINR", "WHOL"};

static inline uint32_t rand_u32() {
    uint32_t x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}

static inline float rand_f() {
    return (float)(rand_u32() & 0x00FFFFFFU) * (1.0f / 16777216.0f);
}

static inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static float quantize_note(float raw_note, int32_t mode) {
    if (mode <= 0) return raw_note;
    int32_t note = (int32_t)roundf(raw_note);
    if (mode == 1) return (float)note;

    int32_t octave = note / 12;
    int32_t degree = note % 12;
    if (degree < 0) {
        degree += 12;
        octave -= 1;
    }

    static const int32_t pentatonic[] = {0, 0, 3, 3, 5, 5, 7, 7, 10, 10, 10, 12};
    static const int32_t minor_sc[]   = {0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 10, 10};
    static const int32_t wholetone[]  = {0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10};

    if (mode == 2) degree = pentatonic[degree];
    else if (mode == 3) degree = minor_sc[degree];
    else if (mode >= 4) degree = wholetone[degree];

    return (float)(octave * 12 + degree);
}

static inline float note_to_inc(float note) {
    float clamped = clampf(note, 12.0f, 127.0f);
    float freq = 440.0f * powf(2.0f, (clamped - 69.0f) * (1.0f / 12.0f));
    return freq * INV_SAMPLE_RATE;
}

static int32_t allocate_voice() {
    int32_t quietest_idx = 0;
    float min_env = 2.0f;

    for (int32_t i = 0; i < MAX_VOICES; i++) {
        if (!s_voices[i].active) {
            return i;
        }
        if (!s_voices[i].in_attack && s_voices[i].env < min_env) {
            min_env = s_voices[i].env;
            quietest_idx = i;
        }
    }
    return quietest_idx;
}

static void trigger_event(bool allow_rest) {
    if (allow_rest && (rand_f() < p_rest)) {
        return;
    }

    bool is_chord = (rand_f() < p_chrd);
    int32_t num_notes = is_chord ? (2 + (int32_t)(rand_u32() % 5U)) : 1;
    float spread_semitones = p_span * 24.0f;

    float rate_len_scale = (p_mode == 1) ? (0.4f + (1.0f - p_rate) * 1.2f) : 1.0f;

    for (int32_t n = 0; n < num_notes; n++) {
        int32_t v = allocate_voice();

        float offset = (rand_f() * 2.0f - 1.0f) * spread_semitones;
        float final_note = quantize_note(s_base_note + offset, p_scal);

        s_voices[v].active = true;
        s_voices[v].in_attack = true;
        s_voices[v].phase = 0.0f;
        s_voices[v].phase_inc = note_to_inc(final_note);
        s_voices[v].env = 0.0f;
        s_voices[v].vel = 0.45f + rand_f() * 0.55f;
        s_voices[v].fm_depth = rand_f() * p_tone * 4.5f;

        float rand_factor = 1.0f + (rand_f() * 2.0f - 1.0f) * p_lenr * 0.85f;
        float base_sec = (0.025f + (p_len * p_len) * 2.2f) * rate_len_scale;
        float dur_sec = clampf(base_sec * rand_factor, 0.015f, 3.5f);

        float decay = 1.0f - (LN_1000 / (dur_sec * SAMPLE_RATE));
        s_voices[v].env_decay = clampf(decay, 0.90f, 0.99999f);
    }
}

__unit_callback int8_t unit_init(const unit_runtime_desc_t * desc) {
    if (!desc) return k_unit_err_undef;
    if (desc->target != unit_header.target) return k_unit_err_target;
    if (!UNIT_API_IS_COMPAT(desc->api)) return k_unit_err_api_version;
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (desc->output_channels != 1) return k_unit_err_geometry;

    s_desc = *desc;
    s_active_notes = 0;
    s_step_timer = 0;

    for (int32_t i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
        s_voices[i].in_attack = false;
        s_voices[i].phase = 0.0f;
        s_voices[i].phase_inc = 0.0f;
        s_voices[i].env = 0.0f;
        s_voices[i].env_decay = 0.999f;
        s_voices[i].vel = 0.0f;
        s_voices[i].fm_depth = 0.0f;
    }

    s_param_cache[PARAM_RATE] = 512;
    s_param_cache[PARAM_SPAN] = 512;
    s_param_cache[PARAM_MODE] = 0;
    s_param_cache[PARAM_RHYT] = 50;
    s_param_cache[PARAM_REST] = 15;
    s_param_cache[PARAM_LEN]  = 45;
    s_param_cache[PARAM_LENR] = 70;
    s_param_cache[PARAM_CHRD] = 50;
    s_param_cache[PARAM_SCAL] = 2;
    s_param_cache[PARAM_TONE] = 25;

    return k_unit_err_none;
}

__unit_callback void unit_teardown() {}

__unit_callback void unit_reset() {
    s_active_notes = 0;
    s_step_timer = 0;
    for (int32_t i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
        s_voices[i].env = 0.0f;
    }
}

__unit_callback void unit_resume() {}
__unit_callback void unit_suspend() {}

__unit_callback void unit_render(const float * in, float * out, uint32_t frames) {
    (void)in;
    const unit_runtime_osc_context_t *ctxt = static_cast<const unit_runtime_osc_context_t *>(s_desc.hooks.runtime_context);

    if (ctxt && ctxt->pitch > 0 && s_active_notes > 0) {
        s_base_note = (float)(ctxt->pitch >> 8) + (float)(ctxt->pitch & 0xFFU) * (1.0f / 256.0f);
    }

    for (uint32_t f = 0; f < frames; f++) {
        if (p_mode == 0 && s_active_notes > 0) {
            if (s_step_timer == 0) {
                trigger_event(true);
                float base_sec = 0.04f + (1.0f - p_rate) * (1.0f - p_rate) * 1.25f;
                float jitter = 1.0f + (rand_f() * 2.0f - 1.0f) * p_rhyt * 0.9f;
                uint32_t next_samples = (uint32_t)(base_sec * jitter * SAMPLE_RATE);
                s_step_timer = (next_samples < 480U) ? 480U : next_samples;
            } else {
                s_step_timer--;
            }
        }

        float mix = 0.0f;
        for (int32_t v = 0; v < MAX_VOICES; v++) {
            if (!s_voices[v].active) continue;

            if (s_voices[v].in_attack) {
                s_voices[v].env += 0.01f;
                if (s_voices[v].env >= 1.0f) {
                    s_voices[v].env = 1.0f;
                    s_voices[v].in_attack = false;
                }
            } else {
                s_voices[v].env *= s_voices[v].env_decay;
                if (s_voices[v].env < 0.001f) {
                    s_voices[v].active = false;
                    continue;
                }
            }

            float p = s_voices[v].phase;
            float sine = sinf(p * TWO_PI);
            float saw = 2.0f * p - 1.0f;
            float base_wave = sine * (1.0f - p_tone) + saw * p_tone;
            float wave = (p_tone > 0.35f) ? sinf((p * TWO_PI) + base_wave * s_voices[v].fm_depth) : base_wave;

            mix += wave * s_voices[v].env * s_voices[v].vel * 0.26f;

            s_voices[v].phase += s_voices[v].phase_inc;
            if (s_voices[v].phase >= 1.0f) s_voices[v].phase -= 1.0f;
        }

        out[f] = tanhf(mix);
    }
}

__unit_callback void unit_set_param_value(uint8_t index, int32_t value) {
    if (index >= NUM_PARAMS) return;
    s_param_cache[index] = value;

    switch (index) {
        case PARAM_RATE: p_rate = clampf((float)value * (1.0f / 1023.0f), 0.0f, 1.0f); break;
        case PARAM_SPAN: p_span = clampf((float)value * (1.0f / 1023.0f), 0.0f, 1.0f); break;
        case PARAM_MODE: p_mode = (value <= 0) ? 0 : 1; break;
        case PARAM_RHYT: p_rhyt = clampf((float)value * 0.01f, 0.0f, 1.0f); break;
        case PARAM_REST: p_rest = clampf((float)value * 0.01f, 0.0f, 0.95f); break;
        case PARAM_LEN:  p_len  = clampf((float)value * 0.01f, 0.0f, 1.0f); break;
        case PARAM_LENR: p_lenr = clampf((float)value * 0.01f, 0.0f, 1.0f); break;
        case PARAM_CHRD: p_chrd = clampf((float)value * 0.01f, 0.0f, 1.0f); break;
        case PARAM_SCAL: p_scal = (value < 0) ? 0 : ((value > 4) ? 4 : value); break;
        case PARAM_TONE: p_tone = clampf((float)value * 0.01f, 0.0f, 1.0f); break;
        default: break;
    }
}

__unit_callback int32_t unit_get_param_value(uint8_t index) {
    if (index >= NUM_PARAMS) return 0;
    return s_param_cache[index];
}

__unit_callback const char * unit_get_param_str_value(uint8_t index, int32_t value) {
    if (index == PARAM_MODE && value >= 0 && value < 2) {
        return s_mode_names[value];
    }
    if (index == PARAM_SCAL && value >= 0 && value < 5) {
        return s_scale_names[value];
    }
    return nullptr;
}

__unit_callback void unit_set_tempo(uint32_t tempo) { (void)tempo; }
__unit_callback void unit_tempo_4ppqn_tick(uint32_t counter) { (void)counter; }

__unit_callback void unit_note_on(uint8_t note, uint8_t velocity) {
    (void)velocity;
    s_base_note = (float)note;
    s_active_notes++;

    if (p_mode == 0) {
        s_step_timer = 0;
    } else {
        trigger_event(false);
    }
}

__unit_callback void unit_note_off(uint8_t note) {
    (void)note;
    if (s_active_notes > 0) {
        s_active_notes--;
    }
}

__unit_callback void unit_all_note_off() {
    s_active_notes = 0;
    for (int32_t i = 0; i < MAX_VOICES; i++) {
        s_voices[i].active = false;
    }
}

