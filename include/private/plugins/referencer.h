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
#include <lsp-plug.in/dsp-units/sampling/Sample.h>
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
                    float              *vReference;                                 // Reference signal buffer

                    // Input ports
                    plug::IPort        *pIn;                                        // Input port
                    plug::IPort        *pOut;                                       // Output port
                } channel_t;

            protected:
                uint32_t            nChannels;                                  // Number of channels
                uint32_t            nPlaySample;                                // Current sample index
                uint32_t            nPlayLoop;                                  // Current loop index
                uint32_t            nCrossfadeTime;                             // Cross-fade time in samples
                stereo_mode_t       enMode;                                     // Stereo mode
                float              *vBuffer;                                    // Temporary buffer
                bool                bPlay;                                      // Play
                bool                bSyncLoopMesh;                              // Sync loop mesh
                channel_t          *vChannels;                                  // Delay channels
                asource_t           sMix;                                       // Mix signal characteristics
                asource_t           sRef;                                       // Reference signal characteristics
                ipc::IExecutor     *pExecutor;                                  // Executor service
                afile_t             vSamples[meta::referencer::AUDIO_SAMPLES];  // Audio samples

                plug::IPort        *pBypass;                                    // Bypass
                plug::IPort        *pPlay;                                      // Play switch
                plug::IPort        *pPlaySample;                                // Current sample index
                plug::IPort        *pPlayLoop;                                  // Current loop index
                plug::IPort        *pSource;                                    // Audio source
                plug::IPort        *pLoopMesh;                                  // Loop mesh
                plug::IPort        *pLoopLen;                                   // Loop length
                plug::IPort        *pLoopPos;                                   // Loop play position
                plug::IPort        *pMode;                                      // Output mode
                plug::IPort        *pPostMode;                                  // Post-filter mode
                plug::IPort        *pPostSlope;                                 // Post-filter slope
                plug::IPort        *pPostSel;                                   // Post-filter selector
                plug::IPort        *pPostSplit[meta::referencer::POST_SPLITS];  // Post-filter split frequencies

                uint8_t            *pData;                                      // Allocated data

            protected:
                static void         destroy_sample(dspu::Sample * &sample);
                static void         make_thumbnail(float *dst, const float *src, size_t len);
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
                void                render_loop(afile_t *af, loop_t *al, size_t samples);
                void                output_file_data();
                void                output_loop_data();
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

