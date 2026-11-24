#include "unit_osc.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x55534552U,
    .unit_id = 0x00000001U,
    .version = 0x00010000U,
    .name = "ALEATORIC",
    .num_params = 10,
    .params = {
        {0, 1023, 0, 512, k_unit_param_type_none,    0, 0, 0, {"RATE"}},
        {0, 1023, 0, 512, k_unit_param_type_none,    0, 0, 0, {"SPAN"}},
        {0, 1,    0, 0,   k_unit_param_type_strings, 0, 0, 0, {"MODE"}},
        {0, 100,  0, 50,  k_unit_param_type_percent, 0, 0, 0, {"RHYT"}},
        {0, 95,   0, 15,  k_unit_param_type_percent, 0, 0, 0, {"REST"}},
        {0, 100,  0, 45,  k_unit_param_type_percent, 0, 0, 0, {"LEN"}},
        {0, 100,  0, 70,  k_unit_param_type_percent, 0, 0, 0, {"LENR"}},
        {0, 100,  0, 50,  k_unit_param_type_percent, 0, 0, 0, {"CHRD"}},
        {0, 4,    0, 2,   k_unit_param_type_strings, 0, 0, 0, {"SCAL"}},
        {0, 100,  0, 25,  k_unit_param_type_percent, 0, 0, 0, {"TONE"}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}
    }
};
