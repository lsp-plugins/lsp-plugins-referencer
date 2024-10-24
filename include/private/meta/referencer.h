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

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct referencer
        {
            static constexpr float  SAMPLE_LENGTH_MIN   = 0.0f;         // Minimum length (s)
            static constexpr float  SAMPLE_LENGTH_MAX   = 1000.0f;      // Maximum sample length (s)
            static constexpr float  SAMPLE_LENGTH_DFL   = 0.0f;         // Sample length (s)
            static constexpr float  SAMPLE_LENGTH_STEP  = 0.01f;        // Sample step (s)

            static constexpr float  SAMPLE_PLAYBACK_MIN = -1.0f;        // Minimum playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_MAX = 1000.0f;      // Maximum playback posotion (s)
            static constexpr float  SAMPLE_PLAYBACK_DFL = -1.0f;        // Default playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_STEP = 0.01f;       // Playback step (s)

            static constexpr size_t CHANNELS_MAX        = 2;            // Maximum audio channels
            static constexpr size_t AUDIO_SAMPLES       = 4;            // Number of samples
            static constexpr size_t AUDIO_LOOPS         = 4;            // Number of loops per sample
            static constexpr size_t FILE_MESH_SIZE      = 640;          // Audio file mesh size
        } referencer;

        // Plugin type metadata
        extern const plugin_t referencer_mono;
        extern const plugin_t referencer_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_REFERENCER_H_ */
