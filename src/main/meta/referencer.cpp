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
        static const port_item_t mode_selectors[] =
        {
            { "Mix",        "referencer.mode.mix"               },
            { "Reference",  "referencer.mode.reference"         },
            { "Both",       "referencer.mode.both"              },
            { NULL, NULL }
        };

        static const port_item_t image_selectors[] =
        {
            { "Stereo",         "referencer.image.stereo"       },
            { "Mono",           "referencer.image.mono"         },
            { "Side",           "referencer.image.side"         },
            { "Sides",          "referencer.image.sides"        },
            { "Left Only",      "referencer.image.left_only"    },
            { "Left",           "referencer.image.left"         },
            { "Right",          "referencer.image.right"        },
            { "Right Only",     "referencer.image.right_only"   },
            { NULL, NULL }
        };

        static const port_item_t tab_selectors[] =
        {
            { "Samples",    "referencer.tab.samples"        },
            { "Spectrum",   "referencer.tab.spectrum"       },
            { "Dynamics",   "referencer.tab.dynamics"       },
            { "Stereo",     "referencer.tab.stereo"         },
            { NULL, NULL }
        };

        static const port_item_t sample_selectors[] =
        {
            { "Sample 1",   "referencer.sample.1"           },
            { "Sample 2",   "referencer.sample.2"           },
            { "Sample 3",   "referencer.sample.3"           },
            { "Sample 4",   "referencer.sample.4"           },
            { NULL, NULL }
        };

        static const port_item_t loop_selectors[] =
        {
            { "Loop 1",     "referencer.loop.1"             },
            { "Loop 2",     "referencer.loop.2"             },
            { "Loop 3",     "referencer.loop.3"             },
            { "Loop 4",     "referencer.loop.4"             },
            { NULL, NULL }
        };

        #define REF_LOOP(id, name) \
            CONTROL("lb", name " loop region start" id, U_MSEC, referencer::SAMPLE_LENGTH), \
            CONTROL("le", name " loop region end" id, U_MSEC, referencer::SAMPLE_LENGTH),  \
            METER("lp", name " play position" id, U_MSEC, referencer::SAMPLE_PLAYBACK)

        #define REF_SAMPLE(id, name) \
            PATH("sf" id, name " file"), \
            COMBO("ls" id, name " loop selector", 0, loop_selectors), \
            REF_LOOP(id "_1", name), \
            REF_LOOP(id "_2", name), \
            REF_LOOP(id "_3", name), \
            REF_LOOP(id "_4", name)

        #define REF_SAMPLES \
            COMBO("ss", "Sample Selector", 0, sample_selectors), \
            REF_SAMPLE("1", "Sample 1"), \
            REF_SAMPLE("2", "Sample 2"), \
            REF_SAMPLE("3", "Sample 3"), \
            REF_SAMPLE("4", "Sample 4")

        #define REF_COMMON \
            COMBO("mode", "Comparison mode", 0, mode_selectors), \
            COMBO("section", "Tab Section Selector", 0, tab_selectors)

        #define REF_COMMON_STEREO \
            COMBO("image", "Stereo image", 0, image_selectors)

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
            E_DUMP_STATE,
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
            E_DUMP_STATE,
            referencer_stereo_ports,
            "utils/referencer.xml",
            NULL,
            stereo_plugin_port_groups,
            &referencer_bundle
        };
    } /* namespace meta */
} /* namespace lsp */



