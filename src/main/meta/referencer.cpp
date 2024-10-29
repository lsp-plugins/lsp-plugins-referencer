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

#include <lsp-plug.in/common/status.h>
#include <lsp-plug.in/plug-fw/meta/ports.h>
#include <lsp-plug.in/shared/meta/developers.h>
#include <private/meta/referencer.h>

#define LSP_PLUGINS_REFERENCER_VERSION_MAJOR       1
#define LSP_PLUGINS_REFERENCER_VERSION_MINOR       0
#define LSP_PLUGINS_REFERENCER_VERSION_MICRO       0

#define LSP_PLUGINS_REFERENCER_VERSION  \
    LSP_MODULE_VERSION( \
        LSP_PLUGINS_REFERENCER_VERSION_MAJOR, \
        LSP_PLUGINS_REFERENCER_VERSION_MINOR, \
        LSP_PLUGINS_REFERENCER_VERSION_MICRO  \
    )

namespace lsp
{
    namespace meta
    {
        //-------------------------------------------------------------------------
        // Plugin metadata
        static const port_item_t source_selectors[] =
        {
            { "Mix",            "referencer.source.mix"             },
            { "Reference",      "referencer.source.reference"       },
            { "Both",           "referencer.source.both"            },
            { NULL, NULL }
        };

        static const port_item_t mode_selectors[] =
        {
            { "Stereo",         "referencer.mode.stereo"            },
            { "Inverse Stereo", "referencer.mode.inverse_stereo"    },
            { "Mono",           "referencer.mode.mono"              },
            { "Side",           "referencer.mode.side"              },
            { "Sides",          "referencer.mode.sides"             },
            { "Mid/Side",       "referencer.mode.mid_side"          },
            { "Side/Mid",       "referencer.mode.side_mid"          },
            { "Left Only",      "referencer.mode.left_only"         },
            { "Left",           "referencer.mode.left"              },
            { "Right",          "referencer.mode.right"             },
            { "Right Only",     "referencer.mode.right_only"        },
            { NULL, NULL }
        };

        static const port_item_t tab_selectors[] =
        {
            { "Samples",        "referencer.tab.samples"            },
            { "Spectrum",       "referencer.tab.spectrum"           },
            { "Dynamics",       "referencer.tab.dynamics"           },
            { "Stereo",         "referencer.tab.stereo"             },
            { NULL, NULL }
        };

        static const port_item_t sample_selectors[] =
        {
            { "Sample 1",       "referencer.sample.1"               },
            { "Sample 2",       "referencer.sample.2"               },
            { "Sample 3",       "referencer.sample.3"               },
            { "Sample 4",       "referencer.sample.4"               },
            { NULL, NULL }
        };

        static const port_item_t loop_selectors[] =
        {
            { "Loop 1",         "referencer.loop.1"                 },
            { "Loop 2",         "referencer.loop.2"                 },
            { "Loop 3",         "referencer.loop.3"                 },
            { "Loop 4",         "referencer.loop.4"                 },
            { NULL, NULL }
        };

        #define REF_LOOP(id, name) \
            CONTROL("lb" id, name " start", U_SEC, referencer::SAMPLE_LENGTH), \
            CONTROL("le" id, name " end", U_SEC, referencer::SAMPLE_LENGTH),  \
            METER("pp" id, name " play position", U_SEC, referencer::SAMPLE_PLAYBACK)

        #define REF_SAMPLE(id, name) \
            PATH("sf" id, name " file"), \
            STATUS("fs" id, name " load status"), \
            METER("fl" id, name " length", U_SEC, referencer::SAMPLE_LENGTH), \
            MESH("fm" id, name " mesh data", referencer::CHANNELS_MAX, referencer::FILE_MESH_SIZE), \
            AMP_GAIN("sg" id, name " gain", GAIN_AMP_0_DB, GAIN_AMP_P_24_DB), \
            COMBO("ls" id, name " loop selector", 0, loop_selectors), \
            REF_LOOP(id "_1", name " loop 1"), \
            REF_LOOP(id "_2", name " loop 2"), \
            REF_LOOP(id "_3", name " loop 3"), \
            REF_LOOP(id "_4", name " loop 4")

        #define REF_SAMPLES \
            COMBO("ssel", "Sample Selector", 0, sample_selectors), \
            REF_SAMPLE("_1", "Sample 1"), \
            REF_SAMPLE("_2", "Sample 2"), \
            REF_SAMPLE("_3", "Sample 3"), \
            REF_SAMPLE("_4", "Sample 4")

        #define REF_COMMON \
            SWITCH("play", "Playback", 0), \
            INT_CONTROL("pssel", "Playback sample selector", U_NONE, referencer::SAMPLE_SELECTOR), \
            INT_CONTROL("plsel", "Playback loop selector", U_NONE, referencer::LOOP_SELECTOR), \
            COMBO("source", "Audio source", 0, source_selectors), \
            COMBO("section", "Tab section selector", 0, tab_selectors)

        #define REF_COMMON_STEREO \
            COMBO("mode", "Output mode", 0, mode_selectors)

        static const port_t referencer_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,

            BYPASS,
            REF_COMMON,
            REF_SAMPLES,

            PORTS_END
        };

        static const port_t referencer_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,

            BYPASS,
            REF_COMMON,
            REF_COMMON_STEREO,
            REF_SAMPLES,

            PORTS_END
        };

        static const int plugin_classes[]       = { C_UTILITY, -1 };
        static const int clap_features_mono[]   = { CF_AUDIO_EFFECT, CF_UTILITY, CF_MONO, -1 };
        static const int clap_features_stereo[] = { CF_AUDIO_EFFECT, CF_UTILITY, CF_STEREO, -1 };

        const meta::bundle_t referencer_bundle =
        {
            "referencer",
            "Referencer",
            B_UTILITIES,
            "", // TODO: provide ID of the video on YouTube
            "Referencer plugin allows you to load your preferred reference files and compare them with your mix"
        };

        const plugin_t referencer_mono =
        {
            "Referencer Mono",
            "Referencer Mono",
            "Referencer Mono",
            "R1M",
            &developers::v_sadovnikov,
            "referencer_mono",
            {
                LSP_LV2_URI("referencer_mono"),
                LSP_LV2UI_URI("referencer_mono"),
                "rf1m",
                LSP_VST3_UID("rf1m    rf1m"),
                LSP_VST3UI_UID("rf1m    rf1m"),
                0,
                NULL,
                LSP_CLAP_URI("referencer_mono"),
                LSP_GST_UID("referencer_mono"),
            },
            LSP_PLUGINS_REFERENCER_VERSION,
            plugin_classes,
            clap_features_mono,
            E_DUMP_STATE | E_FILE_PREVIEW,
            referencer_mono_ports,
            "utils/referencer.xml",
            NULL,
            mono_plugin_port_groups,
            &referencer_bundle
        };

        const plugin_t referencer_stereo =
        {
            "Referencer Stereo",
            "Referencer Stereo",
            "Referencer Stereo",
            "R1S",
            &developers::v_sadovnikov,
            "referencer_stereo",
            {
                LSP_LV2_URI("referencer_stereo"),
                LSP_LV2UI_URI("referencer_stereo"),
                "rf1s",
                LSP_VST3_UID("rf1s    rf1s"),
                LSP_VST3UI_UID("rf1s    rf1s"),
                0,
                NULL,
                LSP_CLAP_URI("referencer_stereo"),
                LSP_GST_UID("referencer_stereo"),
            },
            LSP_PLUGINS_REFERENCER_VERSION,
            plugin_classes,
            clap_features_stereo,
            E_DUMP_STATE | E_FILE_PREVIEW,
            referencer_stereo_ports,
            "utils/referencer.xml",
            NULL,
            stereo_plugin_port_groups,
            &referencer_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



