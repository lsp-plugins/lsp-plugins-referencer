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

#include <lsp-plug.in/dsp-units/misc/envelope.h>
#include <lsp-plug.in/dsp-units/misc/windows.h>
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

            static constexpr size_t CHANNELS_MAX                = 2;                    // Maximum audio channels
            static constexpr size_t AUDIO_SAMPLES               = 4;                    // Number of samples
            static constexpr size_t AUDIO_LOOPS                 = 4;                    // Number of loops per sample
            static constexpr size_t FILE_MESH_SIZE              = 640;                  // Audio file mesh size
            static constexpr size_t DYNA_MESH_SIZE              = 640;                  // Dynamics graph mesh size
            static constexpr size_t DYNA_SUBSAMPLING            = 64;                   // Dynamics graph mesh sub-sampling
            static constexpr size_t WAVE_MESH_SIZE              = 640;                  // Waveform graph mesh size
            static constexpr size_t FLT_BANDS                   = 6;                    // Number of post-filter bands
            static constexpr size_t FLT_SPLITS                  = FLT_BANDS - 1;        // Number of post-filter frequency splits
            static constexpr size_t EQ_RANK                     = 12;                   // Equalizer rank
            static constexpr float  CROSSFADE_TIME              = 5.0f;                 // Cross-fade time in milliseconds
            static constexpr size_t SPC_MAX_RANK                = 14;
            static constexpr size_t SPC_MESH_SIZE               = 640;
            static constexpr size_t SPC_HISTORY_SIZE            = 1 << (SPC_MAX_RANK + 1);
            static constexpr size_t SPC_REFRESH_RATE            = 20;
            static constexpr size_t GONIO_REFRESH_RATE          = 20;
            static constexpr size_t FFT_WND_DFL                 = dspu::windows::HANN;
            static constexpr size_t FFT_ENV_DFL                 = dspu::envelope::PINK_NOISE;
            static constexpr float  CORR_PERIOD                 = 200.0f;               // Correlation period
            static constexpr float  PSR_MIN_LEVEL               = 0.0f;                 // Minimum PSR level (dB)
            static constexpr float  PSR_MAX_LEVEL               = 18.0f;                // Maximum PSR level (dB)
            static constexpr size_t PSR_MESH_SIZE               = 360;                  // Size of PSR mesh
            static constexpr float  PSR_TRUE_PEAK_DECAY         = -0.5f;                // True peak decay for PSR (see AES Convention 143 Brief 373 for reference)
            static constexpr float  AUTOGAIN_MEASURE_PERIOD     = 3000.0f;              // Measurement period for Autogain
            static constexpr float  WAVE_SRANGE_DIFF_MIN        = 12.0f;                // 12 dB minimum difference on logarithmic scale

            static constexpr float  SAMPLE_LENGTH_MIN           = 0.0f;                 // Minimum length (s)
            static constexpr float  SAMPLE_LENGTH_MAX           = 1000.0f;              // Maximum sample length (s)
            static constexpr float  SAMPLE_LENGTH_DFL           = 0.0f;                 // Sample length (s)
            static constexpr float  SAMPLE_LENGTH_STEP          = 0.005f;               // Sample step (s)

            static constexpr float  LOOP_BEGIN_MIN              = SAMPLE_LENGTH_MIN;    // Minimum loop begin position (s)
            static constexpr float  LOOP_BEGIN_MAX              = SAMPLE_LENGTH_MAX;    // Maximum loop begin position (s)
            static constexpr float  LOOP_BEGIN_DFL              = SAMPLE_LENGTH_MIN;    // Default loop begin position (s)
            static constexpr float  LOOP_BEGIN_STEP             = SAMPLE_LENGTH_STEP;   // Loop begin position step (s)

            static constexpr float  LOOP_END_MIN                = SAMPLE_LENGTH_MIN;    // Minimum loop end position (s)
            static constexpr float  LOOP_END_MAX                = SAMPLE_LENGTH_MAX;    // Maximum loop end position (s)
            static constexpr float  LOOP_END_DFL                = SAMPLE_LENGTH_MAX;    // Default loop end position (s)
            static constexpr float  LOOP_END_STEP               = SAMPLE_LENGTH_STEP;   // Loop end position step (s)

            static constexpr float  SAMPLE_PLAYBACK_MIN         = -1.0f;                // Minimum playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_MAX         = 1000.0f;              // Maximum playback posotion (s)
            static constexpr float  SAMPLE_PLAYBACK_DFL         = -1.0f;                // Default playback position (s)
            static constexpr float  SAMPLE_PLAYBACK_STEP        = 0.01f;                // Playback step (s)

            static constexpr size_t SAMPLE_SELECTOR_MIN         = 1;                    // Minimum sample selector
            static constexpr size_t SAMPLE_SELECTOR_MAX         = AUDIO_SAMPLES;        // Maximum sample selector
            static constexpr size_t SAMPLE_SELECTOR_DFL         = SAMPLE_SELECTOR_MIN;  // Default sample selector
            static constexpr size_t SAMPLE_SELECTOR_STEP        = 1;                    // Sample selector step

            static constexpr size_t LOOP_SELECTOR_MIN           = 1;                    // Minimum loop selector
            static constexpr size_t LOOP_SELECTOR_MAX           = AUDIO_LOOPS;          // Maximum loop selector
            static constexpr size_t LOOP_SELECTOR_DFL           = LOOP_SELECTOR_MIN;    // Default loop selector
            static constexpr size_t LOOP_SELECTOR_STEP          = 1;                    // Sample loop step

            static constexpr float  POST_SUB_BASS_MIN           = 20.0f;                // Sub-bass minimium frequency
            static constexpr float  POST_SUB_BASS_MAX           = 80.0f;                // Sub-bass maximium frequency
            static constexpr float  POST_SUB_BASS_DFL           = 60.0f;                // Sub-bass default frequency
            static constexpr float  POST_SUB_BASS_STEP          = 0.0007f;              // post_step(POST_SUB_BASS_MIN, POST_SUB_BASS_MAX);

            static constexpr float  POST_BASS_MIN               = POST_SUB_BASS_MAX;    // Bass minimium frequency
            static constexpr float  POST_BASS_MAX               = 350.0f;               // Bass maximium frequency
            static constexpr float  POST_BASS_DFL               = 250.0f;               // Bass default frequency
            static constexpr float  POST_BASS_STEP              = 0.0005f;              // post_step(POST_BASS_MIN, POST_BASS_MAX);

            static constexpr float  POST_LOW_MID_MIN            = POST_BASS_MAX;        // Low-mid minimium frequency
            static constexpr float  POST_LOW_MID_MAX            = 1000.0f;              // Low-mid maximium frequency
            static constexpr float  POST_LOW_MID_DFL            = 500.0f;               // Low-mid default frequency
            static constexpr float  POST_LOW_MID_STEP           = 0.0005f;              // post_step(POST_LOW_MID_MIN, POST_LOW_MID_MAX);

            static constexpr float  POST_MID_MIN                = POST_LOW_MID_MAX;     // Mid minimium frequency
            static constexpr float  POST_MID_MAX                = 4000.0f;              // Mid maximium frequency
            static constexpr float  POST_MID_DFL                = 2000.0f;              // Mid default frequency
            static constexpr float  POST_MID_STEP               = 0.0005f;              // post_step(POST_MID_MIN, POST_MID_MAX);

            static constexpr float  POST_HIGH_MID_MIN           = POST_MID_MAX;         // High-mid minimium frequency
            static constexpr float  POST_HIGH_MID_MAX           = 12000.0f;             // High-mid maximium frequency
            static constexpr float  POST_HIGH_MID_DFL           = 6000.0f;              // High-mid default frequency
            static constexpr float  POST_HIGH_MID_STEP          = 0.0005f;              // post_step(POST_HIGH_MID_MIN, POST_HIGH_MID_MAX);

            static constexpr float  DYNA_TIME_MIN               = 2.0f;                 // Minimum dynamics time
            static constexpr float  DYNA_TIME_MAX               = 20.0f;                // Maximum dynamics time
            static constexpr float  DYNA_TIME_DFL               = 5.0f;                 // Default dynamics time
            static constexpr float  DYNA_TIME_STEP              = 0.005f;               // Dynamics time step

            static constexpr size_t FFT_RANK_MIN                = 10;                   // Minimum FFT rank
            static constexpr size_t FFT_RANK_DFL                = 12;                   // Default FFT rank
            static constexpr size_t FFT_RANK_MAX                = 14;                   // Maximum FFT rank

            static constexpr float  FFT_REACT_TIME_MIN          = 0.000f;               // Spectrum analysis reactivity min
            static constexpr float  FFT_REACT_TIME_MAX          = 10.000f;              // Spectrum analysis reactivity max
            static constexpr float  FFT_REACT_TIME_DFL          = 1.000f;               // Spectrum analysis reactivity default value
            static constexpr float  FFT_REACT_TIME_STEP         = 0.004f;               // Spectrum analysis reactivity step

            static constexpr float  FFT_BALLISTICS_MIN          = 1.000f;               // Spectrum analysis reactivity min
            static constexpr float  FFT_BALLISTICS_MAX          = 50.000f;              // Spectrum analysis reactivity max
            static constexpr float  FFT_BALLISTICS_DFL          = 10.000f;              // Spectrum analysis reactivity default value
            static constexpr float  FFT_BALLISTICS_STEP         = 0.001f;                // Spectrum analysis reactivity step

            static constexpr float  FFT_HMARK_MIN               = GAIN_AMP_M_72_DB;     // Minimum position of horizontal marker
            static constexpr float  FFT_HMARK_MAX               = GAIN_AMP_P_24_DB;     // Maximum position of horizontal marker
            static constexpr float  FFT_HMARK_DFL               = GAIN_AMP_0_DB;        // Default position of horizontal marker
            static constexpr float  FFT_HMARK_STEP              = 0.005f;               // Step of position of horizontal marker

            static constexpr float  FFT_VMARK_MIN               = SPEC_FREQ_MIN;        // Minimum position of vertical marker
            static constexpr float  FFT_VMARK_MAX               = SPEC_FREQ_MAX;        // Maximum position of vertical marker
            static constexpr float  FFT_VMARK_DFL               = FFT_VMARK_MAX;        // Default position of vertical marker
            static constexpr float  FFT_VMARK_STEP              = 0.005f;               // Step of position of vertical marker

            static constexpr float  MTR_VMARK_MIN               = GAIN_AMP_M_INF_DB;    // Minimum gain for vertical marker
            static constexpr float  MTR_VMARK_MAX               = GAIN_AMP_P_48_DB;     // Maximum gain of vertical marker
            static constexpr float  MTR_VMARK_DFL               = MTR_VMARK_MIN;        // Default gain of vertical marker
            static constexpr float  MTR_VMARK_STEP              = 0.005f;               // Step of gain of vertical marker

            static constexpr float  GONIO_DOTS_MAX              = 8192.0f;              // Maximum dots in goniometer
            static constexpr float  GONIO_DOTS_MIN              = 512.0f;               // Minimum dots in goniometer
            static constexpr float  GONIO_DOTS_DFL              = 2048.0f;              // Default number of dots in goniometer
            static constexpr float  GONIO_DOTS_STEP             = 0.01f;                // Configuration step

            static constexpr size_t GONIO_HISTORY_MAX           = 10;                   // Maximum history for goniometer
            static constexpr size_t GONIO_HISTORY_MIN           = 0;                    // Minimum history for goniometer
            static constexpr size_t GONIO_HISTORY_DFL           = 5;                    // Default history for goniometer
            static constexpr size_t GONIO_HISTORY_STEP          = 1;                    // Step for goniometer

            static constexpr float  CORRELATION_MIN             = -1.0f;                // Minimum correlation value
            static constexpr float  CORRELATION_MAX             = 1.0f;                 // Maximum correlation value
            static constexpr float  CORRELATION_DFL             = 0.0f;                 // Default correlation value
            static constexpr float  CORRELATION_STEP            = 0.001f;               // Correlation step

            static constexpr float  PANOMETER_MIN               = 0.0f;                 // Minimum panometer value
            static constexpr float  PANOMETER_MAX               = 1.0f;                 // Maximum panometer value
            static constexpr float  PANOMETER_DFL               = 0.5f;                 // Default panometer value
            static constexpr float  PANOMETER_STEP              = 0.001f;               // Panometer step

            static constexpr float  MSBALANCE_MIN               = 0.0f;                 // Minimum mid/side balance value
            static constexpr float  MSBALANCE_MAX               = 1.0f;                 // Maximum mid/side balance value
            static constexpr float  MSBALANCE_DFL               = 0.5f;                 // Default mid/side balance value
            static constexpr float  MSBALANCE_STEP              = 0.001f;               // Mid/Side balance step

            static constexpr float  PSR_PERIOD_MIN              = 1.0f;                 // Minimum PSR statistics period
            static constexpr float  PSR_PERIOD_MAX              = 30.0f;                // Maximum PSR statistics period
            static constexpr float  PSR_PERIOD_DFL              = 10.0f;                // Default PSR statistics period
            static constexpr float  PSR_PERIOD_STEP             = 0.05f;                // PSR statistics period step

            static constexpr float  PSR_THRESH_MIN              = GAIN_AMP_P_1_DB;      // Minimum PSR threshold
            static constexpr float  PSR_THRESH_MAX              = GAIN_AMP_P_16_DB;     // Maximum PSR threshold
            static constexpr float  PSR_THRESH_DFL              = GAIN_AMP_P_7_DB;      // Default PSR threshold
            static constexpr float  PSR_THRESH_STEP             = GAIN_AMP_S_0_1_DB;    // PSR threshold step

            static constexpr float  PSR_METER_MIN               = GAIN_AMP_0_DB;        // Minimum PSR meter value
            static constexpr float  PSR_METER_MAX               = GAIN_AMP_P_36_DB;     // Maximum PSR meter value
            static constexpr float  PSR_METER_DFL               = GAIN_AMP_0_DB;        // Default PSR meter value
            static constexpr float  PSR_METER_STEP              = 0.01f;                // PSR meter step

            static constexpr float  PSR_HYST_MIN                = 0;                    // PSR hystogram minimum value
            static constexpr float  PSR_HYST_MAX                = 100.0f;               // PSR hystogram maximum value
            static constexpr float  PSR_HYST_DFL                = 0.0f;                 // PSR hystogram default value
            static constexpr float  PSR_HYST_STEP               = 0.01f;                // PSR hystogram step

            static constexpr float  LOUD_METER_MIN              = GAIN_AMP_M_INF_DB;    // Loudness meter minimum
            static constexpr float  LOUD_METER_MAX              = GAIN_AMP_P_24_DB;     // Loudness meter maximum
            static constexpr float  LOUD_METER_DFL              = GAIN_AMP_M_INF_DB;    // Loudness meter default
            static constexpr float  LOUD_METER_STEP             = 0.01f;                // Loudness meter step

            static constexpr float  ILUFS_TIME_MIN              = 5.0f;                 // Minimum integrated LUFS computation time
            static constexpr float  ILUFS_TIME_MAX              = 60.0f;                // Maximum integrated LUFS computation time
            static constexpr float  ILUFS_TIME_DFL              = 20.0f;                // Integrated LUFS default computation time
            static constexpr float  ILUFS_TIME_STEP             = 0.05f;                // Integrated LUFS computation time step

            static constexpr float  GAIN_MATCH_REACT_MIN        = 0.1f;                 // Minimum gain matching reactivity (s)
            static constexpr float  GAIN_MATCH_REACT_MAX        = 5.0f;                 // Maximum gain matching reactivity (s)
            static constexpr float  GAIN_MATCH_REACT_DFL        = 1.0f;                 // Default gain matching reactivity (s)
            static constexpr float  GAIN_MATCH_REACT_STEP       = 0.001f;               // Gain matching reactivity step

            static constexpr float  WAVE_OFFSET_MIN             = 0.0f;                 // Minimum waveform frame offset in seconds
            static constexpr float  WAVE_OFFSET_MAX             = 2.0f;                 // Maximum waveform frame offset in seconds
            static constexpr float  WAVE_OFFSET_DFL             = 0.0f;                 // Default waveform frame offset in seconds
            static constexpr float  WAVE_OFFSET_STEP            = 0.0005f;              // Waveform frame offset step in seconds

            static constexpr float  WAVE_SIZE_MIN               = 0.005f;               // Minimum waveform frame size in seconds
            static constexpr float  WAVE_SIZE_MAX               = 2.0f;                 // Maximum waveform frame size in seconds
            static constexpr float  WAVE_SIZE_DFL               = 2.0f;                 // Default waveform frame size in seconds
            static constexpr float  WAVE_SIZE_STEP              = 0.002f;               // Waveform frame size step in seconds

            static constexpr float  WAVE_SMIN_SCALE_MIN         = -72.0f;               // Minimum waveform mesh range begin value
            static constexpr float  WAVE_SMIN_SCALE_MAX         = -12.0f;               // Maximum waveform mesh range begin value
            static constexpr float  WAVE_SMIN_SCALE_DFL         = -36.0f;               // Default waveform mesh range begin value
            static constexpr float  WAVE_SMIN_SCALE_STEP        = 0.02f;                // Default waveform mesh range step

            static constexpr float  WAVE_SMAX_SCALE_MIN         = -48.0f;               // Minimum waveform mesh range end value
            static constexpr float  WAVE_SMAX_SCALE_MAX         = 12.0f;                // Maximum waveform mesh range end value
            static constexpr float  WAVE_SMAX_SCALE_DFL         = 0.0f;                 // Default waveform mesh range end value
            static constexpr float  WAVE_SMAX_SCALE_STEP        = 0.02f;                // Default waveform mesh range step

            enum tabs_t
            {
                TAB_OVERVIEW,
                TAB_SAMPLES,
                TAB_LOUDNESS,
                TAB_WAVEFORM,
                TAB_SPECTRUM,
                TAB_DYNAMICS,
                TAB_CORRELATION,
                TAB_STEREO
            };

        } referencer;

        // Plugin type metadata
        extern const plugin_t referencer_mono;
        extern const plugin_t referencer_stereo;

    } /* namespace meta */
} /* namespace lsp */

#endif /* PRIVATE_META_REFERENCER_H_ */
