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

                typedef struct loop_t
                {
                    int32_t             nStart;                                     // Start position of loop
                    int32_t             nEnd;                                       // End position of loop
                    int32_t             nPos;                                       // Current position of loop

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
                    size_t              nLength;                                    // Audio sample length
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

                    // Input ports
                    plug::IPort        *pIn;                                        // Input port
                    plug::IPort        *pOut;                                       // Output port
                } channel_t;

            protected:
                size_t              nChannels;                                  // Number of channels
                channel_t          *vChannels;                                  // Delay channels
                ipc::IExecutor     *pExecutor;                                  // Executor service
                afile_t             vSamples[meta::referencer::AUDIO_SAMPLES];  // Audio samples

                plug::IPort        *pBypass;                                    // Bypass
                plug::IPort        *pSource;                                    // Audio source
                plug::IPort        *pMode;                                      // Output mode

                uint8_t            *pData;                                      // Allocated data

            protected:
                static void         destroy_sample(dspu::Sample * &sample);

            protected:
                status_t            load_file(afile_t *file);
                void                unload_afile(afile_t *file);
                void                process_file_requests();
                void                output_file_data();
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

