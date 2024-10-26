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

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/shared/debug.h>

#include <private/plugins/referencer.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE         0x1000U

namespace lsp
{
    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::referencer_mono,
            &meta::referencer_stereo
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new referencer(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 2);

        //-------------------------------------------------------------------------
        referencer::AFLoader::AFLoader(referencer *link, afile_t *descr)
        {
            pLink       = link;
            pFile       = descr;
        }

        referencer::AFLoader::~AFLoader()
        {
            pLink       = NULL;
            pFile       = NULL;
        }

        status_t referencer::AFLoader::run()
        {
            return pLink->load_file(pFile);
        };

        void referencer::AFLoader::dump(dspu::IStateDumper *v) const
        {
            v->write("pLink", pLink);
            v->write("pFile", pFile);
        }

        //---------------------------------------------------------------------
        // Implementation
        referencer::referencer(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            nPlaySample     = -1;
            nPlayLoop       = -1;
            nCrossfadeTime  = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels       = NULL;

            pExecutor       = NULL;

            pBypass         = NULL;
            pPlay           = NULL;
            pPlaySample     = NULL;
            pPlayLoop       = NULL;
            pSource         = NULL;
            bPlay           = false;
            bSyncRange      = true;
            pMode           = NULL;

            for (size_t i=0; i < meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af     = &vSamples[i];

                af->pLoader     = NULL;
                af->pSample     = NULL;
                af->pLoaded     = NULL;
                af->nStatus     = STATUS_UNSPECIFIED;
                af->nLength     = 0;
                af->bSync       = false;

                for (size_t j=0; j<meta::referencer::CHANNELS_MAX; ++j)
                    af->vThumbs[j]  = NULL;

                for (size_t j=0; j < meta::referencer::AUDIO_SAMPLES; ++j)
                {
                    loop_t *al      = &af->vLoops[j];

                    al->nState      = PB_OFF;
                    al->nTransition = 0;
                    al->nStart      = -1;
                    al->nEnd        = -1;
                    al->nPos        = -1;
                    al->bFirst      = true;

                    al->pStart      = NULL;
                    al->pEnd        = NULL;
                    al->pPlayPos    = NULL;
                }

                af->pFile       = NULL;
                af->pStatus     = NULL;
                af->pLength     = NULL;
                af->pMesh       = NULL;
                af->pGain       = NULL;
            }

            pData           = NULL;
        }

        referencer::~referencer()
        {
            do_destroy();
        }

        void referencer::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Save executor service
            pExecutor               = wrapper->executor();

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t alloc            = szof_channels;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();

                // Initialize fields
                c->pIn                  = NULL;
                c->pOut                 = NULL;
            }

            // Initialize offline tasks
            lsp_trace("Creating offline tasks");
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t  *af        = &vSamples[i];

                // Create loader task
                af->pLoader         = new AFLoader(this, af);
                if (af->pLoader == NULL)
                    return;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pIn);

            // Bind output audio ports
            for (size_t i=0; i<nChannels; ++i)
                BIND_PORT(vChannels[i].pOut);

            // Bind common ports
            lsp_trace("Binding common ports");
            BIND_PORT(pBypass);
            BIND_PORT(pPlay);
            BIND_PORT(pPlaySample);
            BIND_PORT(pPlayLoop);
            BIND_PORT(pSource);
            SKIP_PORT("Tab section selector");

            if (nChannels > 0)
            {
                BIND_PORT(pMode);
            }

            // Bind sample-related ports
            lsp_trace("Binding sample-related ports");
            SKIP_PORT("Sample selector");

            for (size_t i=0; i < meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af     = &vSamples[i];

                BIND_PORT(af->pFile);
                BIND_PORT(af->pStatus);
                BIND_PORT(af->pLength);
                BIND_PORT(af->pMesh);
                BIND_PORT(af->pGain);
                SKIP_PORT("Loop selector");

                for (size_t j=0; j < meta::referencer::AUDIO_SAMPLES; ++j)
                {
                    loop_t *al      = &af->vLoops[j];

                    BIND_PORT(al->pStart);
                    BIND_PORT(al->pEnd);
                    BIND_PORT(al->pPlayPos);
                }
            }
        }

        void referencer::destroy()
        {
            do_destroy();
            Module::destroy();
        }

        void referencer::do_destroy()
        {
            // Destroy audio samples
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t  *af        = &vSamples[i];

                // Destroy tasks
                if (af->pLoader != NULL)
                {
                    delete af->pLoader;
                    af->pLoader         = NULL;
                }

                // Destroy audio file
                unload_afile(af);
            }

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                }
                vChannels   = NULL;
            }

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void referencer::update_sample_rate(long sr)
        {
            // Update cross-fade time and sync it with playbacks
            nCrossfadeTime      = dspu::millis_to_samples(fSampleRate, meta::referencer::CROSSFADE_TIME);

            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af             = &vSamples[i];
                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al              = &af->vLoops[j];
                    al->nTransition         = lsp_min(al->nTransition, nCrossfadeTime);
                }
            }

            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->sBypass.init(sr);
            }
        }

        void referencer::update_settings()
        {
            // Update playback state
            bool play               = pPlay->value() > 0.5f;
            uint32_t play_sample    = pPlaySample->value() - 1.0f;
            uint32_t play_loop      = pPlayLoop->value() - 1.0f;
            if ((play != bPlay) || (play_sample != nPlaySample) || (play_loop != nPlayLoop))
            {
                for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
                {
                    afile_t *af             = &vSamples[i];

                    for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                    {
                        loop_t *al              = &af->vLoops[j];

                        if ((play) && (play_sample == i) && (play_loop == j))
                        {
                            // Turn on sample playback or continue
                            switch (al->nState)
                            {
                                case PB_FADE_OUT:
                                    al->nState      = PB_FADE_IN;
                                    al->nTransition = nCrossfadeTime - lsp_min(al->nTransition, nCrossfadeTime);
                                    al->bFirst      = true;
                                    break;

                                case PB_OFF:
                                    al->nState      = PB_FADE_IN;
                                    al->nTransition = 0;
                                    al->bFirst      = true;
                                    break;

                                case PB_FADE_IN:
                                case PB_ACTIVE:
                                default:
                                    break;
                            }
                        }
                        else
                        {
                            // Turn off sample playback
                            switch (al->nState)
                            {
                                case PB_FADE_IN:
                                    al->nState      = PB_FADE_OUT;
                                    al->nTransition = nCrossfadeTime - lsp_min(al->nTransition, nCrossfadeTime);
                                    break;

                                case PB_ACTIVE:
                                    al->nState      = PB_FADE_OUT;
                                    al->nTransition = 0;
                                    break;

                                case PB_FADE_OUT:
                                case PB_OFF:
                                default:
                                    break;
                            }
                        }
                    }
                }

                if ((nPlaySample != play_sample) && (nPlayLoop != play_loop))
                    bSyncRange              = true;

                bPlay                   = play;
                nPlaySample             = play_sample;
                nPlayLoop               = play_loop;
            }

            // Apply configuration to channels
            bool bypass             = pBypass->value() >= 0.5f;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.set_bypass(bypass);
            }
        }

        void referencer::destroy_sample(dspu::Sample * &sample)
        {
            if (sample != NULL)
            {
                delete sample;
                sample                  = NULL;
            }
        }

        void referencer::unload_afile(afile_t *af)
        {
            // Destroy original sample if present
            destroy_sample(af->pLoaded);

            // Destroy pointer to thumbnails
            if (af->vThumbs[0])
            {
                free(af->vThumbs[0]);
                for (size_t i=0; i<meta::referencer::CHANNELS_MAX; ++i)
                    af->vThumbs[i]              = NULL;
            }
        }

        status_t referencer::load_file(afile_t *af)
        {
            // Load sample
            lsp_trace("file = %p", af);

            // Validate arguments
            if ((af == NULL) || (af->pFile == NULL))
                return STATUS_UNKNOWN_ERR;

            unload_afile(af);

            // Get path
            plug::path_t *path      = af->pFile->buffer<plug::path_t>();
            if (path == NULL)
                return STATUS_UNKNOWN_ERR;

            // Get file name
            const char *fname   = path->path();
            if (strlen(fname) <= 0)
                return STATUS_UNSPECIFIED;

            // Load audio file
            dspu::Sample *source    = new dspu::Sample();
            if (source == NULL)
                return STATUS_NO_MEM;
            lsp_trace("Allocated sample %p", source);
            lsp_finally { destroy_sample(source); };

            // Load sample
            status_t status = source->load_ext(fname, meta::referencer::SAMPLE_LENGTH_MAX);
            if (status != STATUS_OK)
            {
                lsp_trace("load failed: status=%d (%s)", status, get_status(status));
                return status;
            }
            const size_t channels   = lsp_min(nChannels, source->channels());
            if (!source->set_channels(channels))
            {
                lsp_trace("failed to resize source sample to %d channels", int(channels));
                return status;
            }

            // Initialize and render thumbnails
            float *thumbs           = static_cast<float *>(malloc(sizeof(float) * channels * meta::referencer::FILE_MESH_SIZE));
            if (thumbs == NULL)
                return STATUS_NO_MEM;

            for (size_t i=0; i<channels; ++i)
            {
                af->vThumbs[i]          = thumbs;
                thumbs                 += meta::referencer::FILE_MESH_SIZE;

                // Render channel thumbnails
                const float *src        = source->channel(i);
                float *dst              = af->vThumbs[i];
                const size_t len        = source->length();

                for (size_t k=0; k<meta::referencer::FILE_MESH_SIZE; ++k)
                {
                    size_t first    = (k * len) / meta::referencer::FILE_MESH_SIZE;
                    size_t last     = ((k + 1) * len) / meta::referencer::FILE_MESH_SIZE;
                    if (first < last)
                        dst[k]          = dsp::abs_max(&src[first], last - first);
                    else if (first < len)
                        dst[k]          = fabsf(src[first]);
                    else
                        dst[k]          = 0.0f;
                }
            }

            // Commit the result
            lsp_trace("file successfully loaded: %s", fname);
            lsp::swap(af->pLoaded, source);

            return STATUS_OK;
        }

        void referencer::process_file_requests()
        {
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t  *af        = &vSamples[i];
                if (af->pFile == NULL)
                    continue;

                // Get path
                plug::path_t *path = af->pFile->buffer<plug::path_t>();
                if (path == NULL)
                    continue;

                // If there is new load request and loader is idle, then wake up the loader
                if ((path->pending()) && (af->pLoader->idle()))
                {
                    // Try to submit task
                    if (pExecutor->submit(af->pLoader))
                    {
                        af->nStatus     = STATUS_LOADING;
                        lsp_trace("successfully submitted loader task");
                        path->accept();
                    }
                }
                else if ((path->accepted()) && (af->pLoader->completed()))
                {
                    // Commit the result and trigger for sync
                    lsp::swap(af->pLoaded, af->pSample);
                    af->nStatus     = af->pLoader->code();
                    af->nLength     = (af->nStatus == STATUS_OK) ? af->pSample->length() : 0;
                    af->bSync       = true;

                    // Now we can surely commit changes and reset task state
                    path->commit();
                    af->pLoader->reset();
                }
            }
        }

        void referencer::output_file_data()
        {
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af         = &vSamples[i];

                // Output information about the file
                af->pLength->set_value(dspu::samples_to_seconds(fSampleRate, af->nLength));
                af->pStatus->set_value(af->nStatus);

                // Transfer file thumbnails to mesh
                plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(af->pMesh->buffer());
                if ((mesh == NULL) || (!mesh->isEmpty()) || (!af->bSync) || (!af->pLoader->idle()))
                    continue;

                const size_t channels   = (af->pSample != NULL) ? af->pSample->channels() : 0;
                if (channels > 0)
                {
                    // Copy thumbnails
                    for (size_t j=0; j<channels; ++j)
                        dsp::copy(mesh->pvData[j], af->vThumbs[j], meta::referencer::FILE_MESH_SIZE);

                    mesh->data(channels, meta::referencer::FILE_MESH_SIZE);
                }
                else
                    mesh->data(0, 0);

                af->bSync           = false;
            }
        }

        void referencer::process(size_t samples)
        {
            process_file_requests();

            // Process each channel independently
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Get input and output buffers
                const float *in         = c->pIn->buffer<float>();
                float *out              = c->pOut->buffer<float>();
                if ((in == NULL) || (out == NULL))
                    continue;

                dsp::copy(out, in, samples);
            }

            output_file_data();
        }

        void referencer::ui_activated()
        {
            // Mark all samples needed for synchronization
            bSyncRange          = true;

            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af         = &vSamples[i];
                af->bSync           = true;
            }
        }

        void referencer::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            // TODO: write proper dump

            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                }
                v->end_object();
            }
            v->end_array();

            v->write("pBypass", pBypass);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


