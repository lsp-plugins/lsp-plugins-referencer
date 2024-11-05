/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-referencer
 * Created on: 16 окт 2024 г.
 *
 * lsp-plugins-referencer is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-referencer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-referencer. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRIVATE_META_REFERENCER_H_
#define PRIVATE_META_REFERENCER_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

#include <lsp-plug.in/stdlib/math.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct referencer
        {
//            static constexpr inline float post_step(float min, float max)
//            {
//                return logf(max/min) * 0.001f;
//            }

            static constexpr size_t CHANNELS_MAX        = 2;                    // Maximum audio channels
            static constexpr size_t AUDIO_SAMPLES       = 4;                    // Number of samples
            static constexpr size_t AUDIO_LOOPS         = 4;                    // Number of loops per sample
            static constexpr size_t FILE_MESH_SIZE      = 640;                  // Audio file mesh size
            static constexpr size_t DYNA_MESH_SIZE      = 640;                  // Dynamics graph mesh size
            static constexpr size_t DYNA_SUBSAMPLING    = 64;                   // Dynamics graph mesh sub-sampling
            static constexpr size_t POST_BANDS          = 6;                    // Number of post-filter bands
            static constexpr size_t POST_SPLITS         = POST_BANDS - 1;       // Number of post-filter frequency splits
            static constexpr size_t EQ_RANK             = 12;                   // Equalizer rank
            static constexpr float  CROSSFADE_TIME      = 5.0f;                 // Cross-fade time in milliseconds

            static constexpr float  SAMPLE_LENGTH_MIN   = 0.0f;                 // Minimum length (s)
            static constexpr float  SAMPLE_LENGTH_MAX   = 1000.0f;              // Maximum sample length (s)
            static constexpr float  SAMPLE_LENGTH_DFL   = 0.0f;                 // Sample length (s)
            static constexpr float  SAMPLE_LENGTH_STEP  = 0.01f;                // Sample step (s)

            static constexpr float  SAMPLE_PLAYBACK_MIN = -1.0f;                // Minimum playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_MAX = 1000.0f;              // Maximum playback posotion (s)
            static constexpr float  SAMPLE_PLAYBACK_DFL = -1.0f;                // Default playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_STEP = 0.01f;               // Playback step (s)

            static constexpr size_t SAMPLE_SELECTOR_MIN = 1;                    // Minimum sample selector
            static constexpr size_t SAMPLE_SELECTOR_MAX = AUDIO_SAMPLES;        // Maximum sample selector
            static constexpr size_t SAMPLE_SELECTOR_DFL = SAMPLE_SELECTOR_MIN;// Default sample selector
            static constexpr size_t SAMPLE_SELECTOR_STEP= 1;                    // Sample selector step

            static constexpr size_t LOOP_SELECTOR_MIN   = 1;                    // Minimum loop selector
            static constexpr size_t LOOP_SELECTOR_MAX   = AUDIO_LOOPS;          // Maximum loop selector
            static constexpr size_t LOOP_SELECTOR_DFL   = LOOP_SELECTOR_MIN;// Default loop selector
            static constexpr size_t LOOP_SELECTOR_STEP  = 1;                    // Sample loop step

            static constexpr float  POST_SUB_BASS_MIN   = 20.0f;                // Sub-bass minimium frequency
            static constexpr float  POST_SUB_BASS_MAX   = 80.0f;                // Sub-bass maximium frequency
            static constexpr float  POST_SUB_BASS_DFL   = 60.0f;                // Sub-bass default frequency
            static constexpr float  POST_SUB_BASS_STEP  = 0.00139f;             // post_step(POST_SUB_BASS_MIN, POST_SUB_BASS_MAX);

            static constexpr float  POST_BASS_MIN       = POST_SUB_BASS_MAX;    // Bass minimium frequency
            static constexpr float  POST_BASS_MAX       = 350.0f;               // Bass maximium frequency
            static constexpr float  POST_BASS_DFL       = 250.0f;               // Bass default frequency
            static constexpr float  POST_BASS_STEP      = 0.00148f;             // post_step(POST_BASS_MIN, POST_BASS_MAX);

            static constexpr float  POST_LOW_MID_MIN    = POST_BASS_MAX;        // Low-mid minimium frequency
            static constexpr float  POST_LOW_MID_MAX    = 1000.0f;              // Low-mid maximium frequency
            static constexpr float  POST_LOW_MID_DFL    = 500.0f;               // Low-mid default frequency
            static constexpr float  POST_LOW_MID_STEP   = 0.00105f;             // post_step(POST_LOW_MID_MIN, POST_LOW_MID_MAX);

            static constexpr float  POST_MID_MIN        = POST_LOW_MID_MAX;     // Mid minimium frequency
            static constexpr float  POST_MID_MAX        = 4000.0f;              // Mid maximium frequency
            static constexpr float  POST_MID_DFL        = 2000.0f;              // Mid default frequency
            static constexpr float  POST_MID_STEP       = 0.00139f;             // post_step(POST_MID_MIN, POST_MID_MAX);

            static constexpr float  POST_HIGH_MID_MIN   = POST_MID_MAX;         // High-mid minimium frequency
            static constexpr float  POST_HIGH_MID_MAX   = 12000.0f;             // High-mid maximium frequency
            static constexpr float  POST_HIGH_MID_DFL   = 6000.0f;              // High-mid default frequency
            static constexpr float  POST_HIGH_MID_STEP  = 0.00110f;             // post_step(POST_HIGH_MID_MIN, POST_HIGH_MID_MAX);

            static constexpr float  DYNA_TIME_MIN       = 2.0f;                 // Minimum dynamics time
            static constexpr float  DYNA_TIME_MAX       = 20.0f;                // Maximum dynamics time
            static constexpr float  DYNA_TIME_DFL       = 5.0f;                 // Default dynamics time
            static constexpr float  DYNA_TIME_STEP      = 0.01f;                // Dynamics time step
        } referencer;

        // Plugin type metadata
        extern const plugin_t referencer_mono;
        extern const plugin_t referencer_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_REFERENCER_H_ */
