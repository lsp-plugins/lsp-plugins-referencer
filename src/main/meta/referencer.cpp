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

        static const port_item_t graph_selectors[] =
        {
            { "Spectrum",       "referencer.graph.spectrum"         },
            { "History",        "referencer.graph.history"          },
            { NULL, NULL }
        };

        static const port_item_t stereo_selectors[] =
        {
            { "L/R Panorama",   "referencer.stereo.lr_panorama"     },
            { "M/S Balance",    "referencer.stereo.ms_balance"      },
            { NULL, NULL }
        };

        static const port_item_t mono_tab_selectors[] =
        {
            { "Overview",       "referencer.tab.overview"           },
            { "Samples",        "referencer.tab.samples"            },
            { "Loudness",       "referencer.tab.loudness"           },
            { "Waveform",       "referencer.tab.waveform"           },
            { "Spectrum",       "referencer.tab.spectrum"           },
            { NULL, NULL }
        };

        static const port_item_t stereo_tab_selectors[] =
        {
            { "Overview",       "referencer.tab.overview"           },
            { "Samples",        "referencer.tab.samples"            },
            { "Loudness",       "referencer.tab.loudness"           },
            { "Waveform",       "referencer.tab.waveform"           },
            { "Spectrum",       "referencer.tab.spectrum"           },
            { "Dynamics",       "referencer.tab.dynamics"           },
            { "Correlation",    "referencer.tab.correlation"        },
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

        static const port_item_t filter_positions[] =
        {
            { "Pre-eq",         "eq.pos.pre_eq"                     },
            { "Post-eq",        "eq.pos.post_eq"                    },
            { NULL, NULL }
        };

        static const port_item_t filter_slopes[] =
        {
            { "12 dB/oct",      "eq.slope.12dbo"                    },
            { "24 dB/oct",      "eq.slope.24dbo"                    },
            { "36 dB/oct",      "eq.slope.36dbo"                    },
            { "48 dB/oct",      "eq.slope.48dbo"                    },
            { "60 dB/oct",      "eq.slope.60dbo"                    },
            { "72 dB/oct",      "eq.slope.72dbo"                    },
            { "84 dB/oct",      "eq.slope.84dbo"                    },
            { "96 dB/oct",      "eq.slope.96dbo"                    },
            { NULL, NULL }
        };

        static const port_item_t filter_modes[] =
        {
            { "IIR",            "eq.type.iir"                       },
            { "FIR",            "eq.type.fir"                       },
            { "FFT",            "eq.type.fft"                       },
            { "SPM",            "eq.type.spm"                       },
            { NULL, NULL }
        };

        static const port_item_t filter_selector[] =
        {
            { "Off",            "referencer.filter.off"             },
            { "Sub Bass",       "referencer.filter.sub_bass"        },
            { "Bass",           "referencer.filter.bass"            },
            { "Low Mid",        "referencer.filter.low_mid"         },
            { "Mid",            "referencer.filter.mid"             },
            { "High Mid",       "referencer.filter.high_mid"        },
            { "High",           "referencer.filter.high"            },
            { NULL, NULL }
        };

        static const port_item_t psr_hyst_mode[] =
        {
            { "Density",        "referencer.psr.density"            },
            { "Frequency",      "referencer.psr.frequency"          },
            { "Normalized",     "referencer.psr.normalized"         },
            { NULL, NULL }
        };

        static const port_item_t gain_matching[] =
        {
            { "None",           "referencer.matching.none"          },
            { "Reference",      "referencer.matching.reference"     },
            { "Mix",            "referencer.matching.mix"           },
            { NULL, NULL }
        };

        static const port_item_t fft_tolerance[] =
        {
            { "1024", NULL },
            { "2048", NULL },
            { "4096", NULL },
            { "8192", NULL },
            { "16384", NULL },
            { NULL, NULL }
        };

        static const port_item_t fft_chan_selectors_mono[] =
        {
            { "Mix",                "referencer.fft.mix"            },
            { "Reference",          "referencer.fft.ref"            },
            { NULL, NULL }
        };

        static const port_item_t fft_chan_selectors_stereo[] =
        {
            { "Mix Left",           "referencer.fft.mix_left"       },
            { "Mix Right",          "referencer.fft.mix_right"      },
            { "Mix Mid",            "referencer.fft.mix_mid"        },
            { "Mix Side",           "referencer.fft.mix_side"       },
            { "Reference Left",     "referencer.fft.ref_left"       },
            { "Reference Right",    "referencer.fft.ref_right"      },
            { "Reference Mid",      "referencer.fft.ref_mid"        },
            { "Reference Side",     "referencer.fft.ref_side"       },
            { NULL, NULL }
        };



        #define REF_LOOP(id, name) \
            CONTROL("lb" id, name " start", U_SEC, referencer::LOOP_BEGIN), \
            CONTROL("le" id, name " end", U_SEC, referencer::LOOP_END),  \
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

        #define REF_COMMON(tab_selectors, dfl_chan_selector, chan_selectors) \
            SWITCH("play", "Playback", 0), \
            INT_CONTROL("pssel", "Playback sample selector", U_NONE, referencer::SAMPLE_SELECTOR), \
            INT_CONTROL("plsel", "Playback loop selector", U_NONE, referencer::LOOP_SELECTOR), \
            COMBO("source", "Audio source", 0, source_selectors), \
            COMBO("section", "Tab section selector", 0, tab_selectors), \
            SWITCH("mixvis", "Mix graphs visibility", 1), \
            SWITCH("refvis", "Reference graphs visibility", 1), \
            SWITCH("minvis", "Minimum graphs visibility", 0), \
            SWITCH("maxvis", "Maximum graphs visibility", 0), \
            SWITCH("mixfrz", "Freeze mix graphs", 0), \
            SWITCH("reffrz", "Freeze reference graphs", 0), \
            /* playback loop display */ \
            MESH("loop_m", "Active loop contents mesh data", referencer::CHANNELS_MAX, referencer::FILE_MESH_SIZE), \
            METER("loop_l", "Active loop length", U_SEC, referencer::SAMPLE_LENGTH), \
            METER("loop_p", "Active loop play position", U_SEC, referencer::SAMPLE_PLAYBACK), \
            /* gain matching */ \
            COMBO("gmmode", "Gain matching mode", 0, gain_matching), \
            LOG_CONTROL("gmreact", "Gain matching reactivity", U_SEC, referencer::GAIN_MATCH_REACT), \
            /* post-filter */ \
            COMBO("fpos", "Filter position in the chain", 0, filter_positions), \
            COMBO("fmode", "Filter mode", 0, filter_modes), \
            COMBO("fslope", "Filter slope", 3, filter_slopes), \
            COMBO("fsel", "Filter selector", 0, filter_selector), \
            LOG_CONTROL("fsub", "Post-filter sub-bass high frequency", U_HZ, referencer::POST_SUB_BASS), \
            LOG_CONTROL("fbass", "Post-filter bass high frequency", U_HZ, referencer::POST_BASS), \
            LOG_CONTROL("flomid", "Post-filter low-mid frequency", U_HZ, referencer::POST_LOW_MID), \
            LOG_CONTROL("fmid", "Post-filter mid frequency", U_HZ, referencer::POST_MID), \
            LOG_CONTROL("fhimid", "Post-filter high-mid frequency", U_HZ, referencer::POST_HIGH_MID), \
            /* graph display maximum time */ \
            CONTROL("maxtime", "Graph display maximum time", U_SEC, referencer::DYNA_TIME), \
            /* Loudness metering */ \
            CONTROL("ilufsit", "Integrated LUFS integration period", U_SEC, referencer::ILUFS_TIME), \
            SWITCH("lmpk", "Peak graph visible", 0), \
            SWITCH("lmtp", "True peak graph visible", 1), \
            SWITCH("lmrms", "RMS graph visible", 0), \
            SWITCH("lmmlufs", "Momentary LUFS graph visible", 0), \
            SWITCH("lmslufs", "Short-term LUFS graph visible", 1), \
            SWITCH("lmilufs", "Integrated LUFS graph visible", 0), \
            /* PSR (dynamics) metering */ \
            CONTROL("psrtime", "PSR measurement time period", U_SEC, referencer::PSR_PERIOD), \
            LOG_CONTROL("psrthr", "PSR measurement threshold", U_GAIN_AMP, referencer::PSR_THRESH), \
            COMBO("psrmode", "PSR hystogram mode", 0, psr_hyst_mode), \
            MESH("psrmesh", "PSR output hystogram", 3, referencer::PSR_MESH_SIZE + 4), \
            /* Waveform metering */ \
            CONTROL("mixwfof", "Mix waveform frame offset", U_SEC, referencer::WAVE_OFFSET), \
            CONTROL("refwfof", "Reference waveform frame offset", U_SEC, referencer::WAVE_OFFSET), \
            CONTROL("wflen", "Waveform frame length", U_SEC, referencer::WAVE_SIZE), \
            SWITCH("wflog", "Logarithmic scale", 0), \
            CONTROL("wfscmin", "Minimum graph scale", U_DB, referencer::WAVE_SMIN_SCALE), \
            CONTROL("wfscmax", "Maximum graph scale", U_DB, referencer::WAVE_SMAX_SCALE), \
            /* FFT analysis */ \
            LOG_CONTROL("fam_hor", "FFT horizontal marker", U_GAIN_AMP, referencer::FFT_HMARK), \
            SWITCH("fam_horv", "FFT horizontal marker visibility", 0), \
            COMBO("fam_vers", "FFT vertical marker source", dfl_chan_selector, chan_selectors), \
            LOG_CONTROL("fam_ver", "FFT vertical marker", U_HZ, referencer::FFT_VMARK), \
            METER("fam_verv", "Vertical marker frequency level", U_GAIN_AMP, referencer::MTR_VMARK), \
            COMBO("ffttol", "FFT Tolerance", referencer::FFT_RANK_DFL - referencer::FFT_RANK_MIN, fft_tolerance), \
            COMBO("fftwnd", "FFT Window", referencer::FFT_WND_DFL, fft_windows), \
            COMBO("fftenv", "FFT Envelope", referencer::FFT_ENV_DFL, fft_envelopes), \
            LOG_CONTROL("fftrea", "FFT Reactivity", U_SEC, referencer::FFT_REACT_TIME), \
            SWITCH("fftdamp", "FFT Damping", 1), \
            TRIGGER("fftrst", "FFT Reset"), \
            LOG_CONTROL("fftbal", "FFT Ballistics", U_SEC, referencer::FFT_BALLISTICS)

        #define REF_COMMON_METERS(id, name) \
            METER("pk_" id, name " Peak meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("tp_" id, name " True Peak meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("rms_" id, name " RMS meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("mlufs_" id, name " Momentary LUFS meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("slufs_" id, name " Short-Term LUFS meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("ilufs_" id, name " Integrated LUFS meter", U_GAIN_AMP, referencer::LOUD_METER), \
            METER("psr_" id, name " PSR meter", U_GAIN_AMP, referencer::PSR_METER)

        #define REF_COMMON_METERS_MONO(id, name) \
            REF_COMMON_METERS(id, name), \
            METER("psrpc_" id, name " PSR hystogram percentage above threshold", U_GAIN_AMP, referencer::PSR_HYST)

        #define REF_COMMON_METERS_STEREO(id, name) \
            STREAM("gon_" id, name " goniometer stream buffer", 3, 128, 0x8000), \
            REF_COMMON_METERS(id, name), \
            METER("corr_" id, name " correlation meter", U_NONE, referencer::CORRELATION), \
            METER("pan_" id, name " panorama meter", U_NONE, referencer::PANOMETER), \
            METER("msbal_" id, name " mid/side balance meter", U_NONE, referencer::MSBALANCE), \
            METER("psrpc_" id, name " PSR hystogram percentage above threshold", U_GAIN_AMP, referencer::PSR_HYST)

        #define REF_COMMON_MONO \
            MESH("dmmesh", "Dynamics display mesh", 15, referencer::DYNA_MESH_SIZE + 4), \
            MESH("wfmesh", "Waveform mesh", 3, referencer::WAVE_MESH_SIZE + 4), \
            MESH("fftgr", "FFT Analysis mesh", 3, referencer::SPC_MESH_SIZE + 4), \
            MESH("fftming", "FFT minimum extremum mesh", 3, referencer::SPC_MESH_SIZE + 4), \
            MESH("fftmaxg", "FFT maximum extremum mesh", 3, referencer::SPC_MESH_SIZE + 4), \
            REF_COMMON_METERS_MONO("m", "Mix"), \
            REF_COMMON_METERS_MONO("r", "Reference")

        #define REF_COMMON_STEREO \
            COMBO("mode", "Output mode", 0, mode_selectors), \
            COMBO("corrdis", "Correlation view mode", 0, graph_selectors), \
            COMBO("stertyp", "Stereo analysis type", 0, stereo_selectors), \
            COMBO("sterdis", "Stereo view mode", 0, graph_selectors), \
            SWITCH("left_v", "Visibilty of FFT/waveform analysis for left channel", 0), \
            SWITCH("right_v", "Visibilty of FFT/waveform analysis for right channel", 0), \
            SWITCH("mid_v", "Visibilty of FFT/waveform analysis for middle channel", 1), \
            SWITCH("side_v", "Visibilty of FFT/waveform analysis for side channel", 0), \
            MESH("dmmesh", "Dynamics display mesh", 21, referencer::DYNA_MESH_SIZE + 4), \
            MESH("wfmesh", "Waveform mesh", 9, referencer::WAVE_MESH_SIZE + 4), \
            MESH("fftgr", "FFT Analysis mesh", 15, referencer::SPC_MESH_SIZE + 4), \
            MESH("fftming", "FFT minimum extremum mesh", 15, referencer::SPC_MESH_SIZE + 4), \
            MESH("fftmaxg", "FFT maximum extremum mesh", 15, referencer::SPC_MESH_SIZE + 4), \
            CONTROL("goniohs", "Goniometer strobe history size", U_NONE, referencer::GONIO_HISTORY), \
            LOG_CONTROL("goniond", "Maximum dots for plotting goniometer", U_NONE, referencer::GONIO_DOTS), \
            REF_COMMON_METERS_STEREO("m", "Mix"), \
            REF_COMMON_METERS_STEREO("r", "Reference")

        static const port_t referencer_mono_ports[] =
        {
            PORTS_MONO_PLUGIN,

            BYPASS,
            REF_COMMON(mono_tab_selectors, 0, fft_chan_selectors_mono),
            REF_COMMON_MONO,
            REF_SAMPLES,

            PORTS_END
        };

        static const port_t referencer_stereo_ports[] =
        {
            PORTS_STEREO_PLUGIN,

            BYPASS,
            REF_COMMON(stereo_tab_selectors, 2, fft_chan_selectors_stereo),
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



