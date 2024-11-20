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

#ifndef PRIVATE_PLUGINS_REFERENCER_H_
#define PRIVATE_PLUGINS_REFERENCER_H_

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/filters/Equalizer.h>
#include <lsp-plug.in/dsp-units/meters/Correlometer.h>
#include <lsp-plug.in/dsp-units/meters/ILUFSMeter.h>
#include <lsp-plug.in/dsp-units/meters/LoudnessMeter.h>
#include <lsp-plug.in/dsp-units/meters/Panometer.h>
#include <lsp-plug.in/dsp-units/meters/TruePeakMeter.h>
#include <lsp-plug.in/dsp-units/sampling/Sample.h>
#include <lsp-plug.in/dsp-units/stat/QuantizedCounter.h>
#include <lsp-plug.in/dsp-units/util/Delay.h>
#include <lsp-plug.in/dsp-units/util/RawRingBuffer.h>
#include <lsp-plug.in/dsp-units/util/ScaledMeterGraph.h>
#include <lsp-plug.in/dsp-units/util/Sidechain.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/referencer.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class referencer: public plug::Module
        {
            protected:
                enum playback_t
                {
                    PB_OFF,                                                         // Sample is not playing
                    PB_FADE_IN,                                                     // Sample is starting to play
                    PB_FADE_OUT,                                                    // Sample is stopping to play
                    PB_ACTIVE,                                                      // Sample is playing
                };

                enum source_t
                {
                    SRC_MIX,
                    SRC_REFERENCE,
                    SRC_BOTH,
                };

                enum stereo_mode_t
                {
                    SM_STEREO,
                    SM_INVERSE_STEREO,
                    SM_MONO,
                    SM_SIDE,
                    SM_SIDES,
                    SM_MID_SIDE,
                    SM_SIDE_MID,
                    SM_LEFT_ONLY,
                    SM_LEFT,
                    SM_RIGHT,
                    SM_RIGHT_ONLY,
                };

                enum post_filter_t
                {
                    PF_OFF,
                    PF_SUB_BASS,
                    PF_BASS,
                    PF_LOW_MID,
                    PF_MID,
                    PF_HIGH_MID,
                    PF_HIGH,
                };

                enum dmtype_t
                {
                    DM_PEAK,                                                        // Peak values
                    DM_TRUE_PEAK,                                                   // True peak values
                    DM_RMS,                                                         // RMS value
                    DM_M_LUFS,                                                      // Momentary LUFS value
                    DM_S_LUFS,                                                      // Short-term LUFS value
                    DM_I_LUFS,                                                      // Integrated LUFS value
                    DM_PSR,                                                         // PSR (True Peak / LUFS) value
                    DM_CORR,                                                        // Correlation (stereo only)
                    DM_PAN,                                                         // Panning (stereo only)
                    DM_MSBAL,                                                       // Mid/Side balance (stereo only)

                    DM_TOTAL,
                    DM_STEREO = DM_TOTAL,
                    DM_MONO = DM_CORR
                };

                enum fgraph_t
                {
                    FG_LEFT,                                                        // Spectrum analysis of the left channel
                    FG_RIGHT,                                                       // Spectrum analysis of the right channel
                    FG_MID,                                                         // Spectrum analysis of the middle channel
                    FG_SIDE,                                                        // Spectrum analysis of the side channel
                    FG_CORR,                                                        // Spectral correlation between left and right channels
                    FG_PAN,                                                         // Panning between left and right channels
                    FG_MSBAL,                                                       // Balance between mid and side signals

                    FG_TOTAL,
                    FG_STEREO = FG_TOTAL,
                    FG_MONO = FG_RIGHT
                };

                enum ftype_t
                {
                    FT_CURR,
                    FT_MIN,
                    FT_MAX,

                    FT_TOTAL
                };

                enum psr_mode_t
                {
                    PSR_DENSITY,
                    PSR_FREQUENCY,
                    PSR_NORMALIZED
                };

                enum gain_matching_t
                {
                    MATCH_NONE,
                    MATCH_REFERENCE,
                    MATCH_MIX
                };

                enum waveform_t
                {
                    WF_LEFT,
                    WF_RIGHT,
                    WF_MID,
                    WF_SIDE,

                    WF_TOTAL,
                    WF_MONO = WF_RIGHT,
                    WF_STEREO = WF_TOTAL
                };

                struct afile_t;

                class AFLoader: public ipc::ITask
                {
                    private:
                        referencer             *pLink;
                        afile_t                *pFile;

                    public:
                        explicit AFLoader(referencer *link, afile_t *descr);
                        virtual ~AFLoader();

                    public:
                        virtual status_t        run();
                        void                    dump(dspu::IStateDumper *v) const;
                };

                typedef struct asource_t
                {
                    float               fGain;                                      // Current gain
                    float               fOldGain;                                   // Previous gain value
                    float               fNewGain;                                   // New gain
                    uint32_t            nTransition;                                // Gain transition
                    bool                bFreeze;                                    // Freeze analysis

                    plug::IPort        *pFreeze;                                    // Freeze analysis
                } asource_t;

                typedef struct loop_t
                {
                    playback_t          nState;                                     // Playback state
                    uint32_t            nTransition;                                // For transition state, the current offset
                    int32_t             nStart;                                     // Start position of loop
                    int32_t             nEnd;                                       // End position of loop
                    int32_t             nPos;                                       // Current position of loop
                    bool                bFirst;                                     // First loop (does not requre to cross-fade with tail)

                    plug::IPort        *pStart;                                     // Start position of loop
                    plug::IPort        *pEnd;                                       // Start position of loop
                    plug::IPort        *pPlayPos;                                   // Current play position
                } loop_t;

                typedef struct afile_t
                {
                    AFLoader           *pLoader;                                    // Audio file loader task
                    dspu::Sample       *pSample;                                    // Loaded sample
                    dspu::Sample       *pLoaded;                                    // New loaded sample
                    status_t            nStatus;                                    // Loading status
                    uint32_t            nLength;                                    // Audio sample length
                    float               fGain;                                      // Audio file gain
                    bool                bSync;                                      // Sync sample with UI
                    float              *vThumbs[meta::referencer::CHANNELS_MAX];    // List of thumbnails
                    loop_t              vLoops[meta::referencer::AUDIO_LOOPS];      // Array of loops for this sample

                    plug::IPort        *pFile;                                      // Audio file port
                    plug::IPort        *pStatus;                                    // Status of the file
                    plug::IPort        *pLength;                                    // Actual length of the file
                    plug::IPort        *pMesh;                                      // Audio file mesh
                    plug::IPort        *pGain;                                      // Audio gain
                } afile_t;

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass        sBypass;                                    // Bypass
                    dspu::Equalizer     sPostFilter;                                // Post-filter

                    float              *vIn;                                        // Input buffer
                    float              *vOut;                                       // Output buffer
                    float              *vBuffer;                                    // Signal buffer
                    float              *vInBuffer;                                  // Input buffer

                    // Input ports
                    plug::IPort        *pIn;                                        // Input port
                    plug::IPort        *pOut;                                       // Output port
                } channel_t;

                typedef struct fft_graph_t
                {
                    float              *vData[FT_TOTAL];                            // Measured values
                } fft_graph_t;

                typedef struct fft_meters_t
                {
                    float              *vHistory[2];                                // History for left and right channels

                    uint32_t            nFftPeriod;                                 // FFT analysis period
                    uint32_t            nFftFrame;                                  // Current FFT frame
                    uint32_t            nFftHistory;                                // Current FFT per channel history write position

                    fft_graph_t         vGraphs[FG_TOTAL];                          // List of graphs
                } fft_meters_t;

                typedef struct dyna_meters_t
                {
                    dspu::Sidechain     sRMSMeter;                                  // RMS meter
                    dspu::TruePeakMeter sTPMeter[2];                                // True Peak meters
                    dspu::Delay         sTPDelay;                                   // True Peak delay
                    dspu::LoudnessMeter sAutogainMeter;                             // Short-term LUFS meter for Autogain matching
                    dspu::LoudnessMeter sMLUFSMeter;                                // Momentary LUFS meter
                    dspu::LoudnessMeter sSLUFSMeter;                                // Short-term LUFS meter
                    dspu::ILUFSMeter    sILUFSMeter;                                // Integrated loudness meter
                    dspu::Correlometer  sCorrMeter;                                 // Corellometer
                    dspu::Panometer     sPanometer;                                 // Panometer
                    dspu::Panometer     sMsBalance;                                 // Mid/Side balance
                    dspu::QuantizedCounter  sPSRStats;                              // PSR statistics
                    dspu::RawRingBuffer vWaveform[WF_TOTAL];                        // Waveform history (capture)
                    dspu::ScaledMeterGraph  vGraphs[DM_TOTAL];                      // Output graphs

                    float              *vLoudness;                                  // Measured short-term loudness
                    float               fGain;                                      // Current gain
                    double              fTPLevel;                                   // True-peak level

                    plug::IPort        *pMeters[DM_TOTAL];                          // Output meters
                    plug::IPort        *pPsrPcValue;                                // PSR value in percents (over threshold)
                } dyna_meters_t;

            protected:
                static const float      dm_endpoints[];
                static const float      fft_endpoints[];

            protected:
                uint32_t            nChannels;                                  // Number of channels
                uint32_t            nPlaySample;                                // Current sample index
                uint32_t            nPlayLoop;                                  // Current loop index
                uint32_t            nGainMatching;                              // Gain matching mode
                float               fGainMatchGrow;                             // Gain matching grow time coefficient
                float               fGainMatchFall;                             // Gain matching fall time coefficient
                uint32_t            nCrossfadeTime;                             // Cross-fade time in samples
                float               fMaxTime;                                   // Maximum display time
                stereo_mode_t       enMode;                                     // Stereo mode
                float               fWaveformOff;                               // Waveform offset
                float               fWaveformLen;                               // Waveform length
                uint32_t            nFftRank;                                   // FFT rank
                uint32_t            nFftWindow;                                 // FFT window
                uint32_t            nFftEnvelope;                               // FFT envelope
                float               fFftTau;                                    // FFT smooth coefficient
                float               fFftBal;                                    // FFT ballistics coefficient
                uint32_t            nGonioStrobe;                               // Counter for strobe signal of goniometer
                uint32_t            nGonioPeriod;                               // Goniometer period
                uint32_t            nPsrMode;                                   // PSR display mode
                uint32_t            nPsrThresh;                                 // PSR threshold (index)
                double              fTPDecay;                                   // True-peak decay for PSR

                float              *vBuffer;                                    // Temporary buffer
                float              *vFftFreqs;                                  // FFT frequencies
                uint16_t           *vFftInds;                                   // FFT indices
                float              *vFftWindow;                                 // FFT window
                float              *vFftEnvelope;                               // FFT envelope
                float              *vPsrLevels;                                 // PSR levels
                bool                bPlay;                                      // Play
                bool                bSyncLoopMesh;                              // Sync loop mesh
                bool                bUpdFft;                                    // Update FFT-related data
                bool                bFftDamping;                                // FFT damping
                channel_t          *vChannels;                                  // Delay channels
                asource_t           sMix;                                       // Mix signal characteristics
                asource_t           sRef;                                       // Reference signal characteristics
                ipc::IExecutor     *pExecutor;                                  // Executor service
                afile_t             vSamples[meta::referencer::AUDIO_SAMPLES];  // Audio samples
                dyna_meters_t       vDynaMeters[2];                             // Dynamic meters for mix and reference
                fft_meters_t        vFftMeters[2];                              // FFT meters

                plug::IPort        *pBypass;                                    // Bypass
                plug::IPort        *pPlay;                                      // Play switch
                plug::IPort        *pPlaySample;                                // Current sample index
                plug::IPort        *pPlayLoop;                                  // Current loop index
                plug::IPort        *pSource;                                    // Audio source
                plug::IPort        *pLoopMesh;                                  // Loop mesh
                plug::IPort        *pLoopLen;                                   // Loop length
                plug::IPort        *pLoopPos;                                   // Loop play position
                plug::IPort        *pGainMatching;                              // Gain matching mode
                plug::IPort        *pGainMatchReact;                            // Gain matching reactivity
                plug::IPort        *pMode;                                      // Output mode
                plug::IPort        *pPostMode;                                  // Post-filter mode
                plug::IPort        *pPostSlope;                                 // Post-filter slope
                plug::IPort        *pPostSel;                                   // Post-filter selector
                plug::IPort        *pPostSplit[meta::referencer::POST_SPLITS];  // Post-filter split frequencies
                plug::IPort        *pMaxTime;                                   // Maximum time on the graph
                plug::IPort        *pILUFSTime;                                 // Integrated LUFS time
                plug::IPort        *pDynaMesh;                                  // Mesh for dynamics output
                plug::IPort        *pWaveformMesh;                              // Waveform mesh
                plug::IPort        *pFrameOffset;                               // Waveform frame offset
                plug::IPort        *pFrameLength;                               // Waveform frame length
                plug::IPort        *pFftRank;                                   // FFT rank
                plug::IPort        *pFftWindow;                                 // FFT window
                plug::IPort        *pFftEnvelope;                               // FFT envelope
                plug::IPort        *pFftReactivity;                             // FFT reactivity
                plug::IPort        *pFftDamping;                                // Enable FFT damping
                plug::IPort        *pFftReset;                                  // Reset FFT minimum and maximum
                plug::IPort        *pFftBallistics;                             // FFT ballistics
                plug::IPort        *pFftMesh[3];                                // FFT mesh
                plug::IPort        *pGoniometer;                                // Goniometer stream
                plug::IPort        *pPsrPeriod;                                 // PSR period
                plug::IPort        *pPsrThreshold;                              // PSR threshold
                plug::IPort        *pPsrMesh;                                   // PSR output
                plug::IPort        *pPsrDisplay;                                // PSR display mode

                uint8_t            *pData;                                      // Allocated data

            protected:
                static void         destroy_sample(dspu::Sample * &sample);
                static void         make_thumbnail(float *dst, const float *src, size_t len, size_t dst_len);
                static void         copy_waveform(float *dst, dspu::RawRingBuffer *rb, size_t offset, size_t length, size_t dst_len);
                static dspu::equalizer_mode_t decode_equalizer_mode(size_t mode);

            protected:
                status_t            load_file(afile_t *file);
                stereo_mode_t       decode_stereo_mode(size_t mode);
                void                unload_afile(afile_t *file);
                void                preprocess_audio_channels();
                void                process_file_requests();
                void                prepare_reference_signal(size_t samples);
                void                mix_channels(size_t samples);
                void                apply_post_filters(size_t samples);
                void                apply_stereo_mode(size_t samples);
                void                apply_gain_matching(size_t samples);
                void                render_loop(afile_t *af, loop_t *al, size_t samples);
                void                perform_fft_analysis(fft_meters_t *fm, const float *l, const float *r, size_t samples);
                void                process_fft_frame(fft_meters_t *fm);
                void                process_goniometer(const float *l1, const float *r1, const float *l2, const float *r2, size_t samples);
                void                perform_metering(dyna_meters_t *dm, const float *l, const float *r, size_t samples);
                void                accumulate_fft(fft_meters_t *fm, size_t type, const float *buf);
                void                reset_fft();
                void                output_file_data();
                void                output_loop_data();
                void                output_dyna_meters();
                void                output_waveform_meshes();
                void                output_psr_mesh();
                void                output_dyna_meshes();
                void                output_spectrum_analysis(size_t type);
                void                reduce_spectrum(float *dst, const float *src);
                void                reduce_cspectrum(float *dst, const float *src);
                void                do_destroy();

            public:
                explicit referencer(const meta::plugin_t *meta);
                referencer(const referencer &) = delete;
                referencer(referencer &&) = delete;
                virtual ~referencer() override;

                referencer & operator = (const referencer &) = delete;
                referencer & operator = (referencer &&) = delete;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                virtual void        destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
                virtual void        ui_activated() override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_REFERENCER_H_ */

