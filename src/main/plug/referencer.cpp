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

namespace lsp
{
    namespace plugins
    {
        static constexpr size_t BUFFER_SIZE         = 0x400;

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
            nChannels           = 0;
            nPlaySample         = -1;
            nPlayLoop           = -1;
            nCrossfadeTime      = 0;
            nDynaMode           = DM_PSR;
            fDynaTime           = 0.0f;
            vBuffer             = NULL;
            vFftFreqs           = NULL;
            vFftInds            = NULL;
            vFftWindow          = NULL;
            vFftEnvelope        = NULL;
            nFftRank            = 0;
            nFftWindow          = -1;
            nFftEnvelope        = -1;
            fFftReactivity      = 0.0f;
            fFftTau             = 0.0f;
            nGonioStrobe        = 0;
            nGonioPeriod        = 0;

            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels           = NULL;
            enMode              = (nChannels > 1) ? SM_STEREO : SM_MONO;

            sMix.fGain          = GAIN_AMP_M_INF_DB;
            sMix.fOldGain       = GAIN_AMP_M_INF_DB;
            sMix.fNewGain       = GAIN_AMP_M_INF_DB;
            sMix.nTransition    = 0;

            sRef.fGain          = GAIN_AMP_M_INF_DB;
            sRef.fOldGain       = GAIN_AMP_M_INF_DB;
            sRef.fNewGain       = GAIN_AMP_M_INF_DB;
            sRef.nTransition    = 0;

            pExecutor           = NULL;

            pBypass             = NULL;
            pPlay               = NULL;
            pPlaySample         = NULL;
            pPlayLoop           = NULL;
            pSource             = NULL;
            pLoopMesh           = NULL;
            pLoopLen            = NULL;
            pLoopPos            = NULL;
            bPlay               = false;
            bSyncLoopMesh       = true;
            bUpdFft             = true;
            pMode               = NULL;

            pPostMode           = NULL;
            pPostSlope          = NULL;
            pPostSel            = NULL;

            for (size_t i=0; i < meta::referencer::POST_SPLITS; ++i)
                pPostSplit[i]       = NULL;

            pDynaMode           = NULL;
            pDynaTime           = NULL;
            pDynaMesh           = NULL;

            pFftRank            = NULL;
            pFftWindow          = NULL;
            pFftEnvelope        = NULL;
            pFftReactivity      = NULL;
            pFftMesh            = NULL;

            pGoniometer         = NULL;

            for (size_t i=0; i < 2; ++i)
            {
                fft_meters_t *fm    = &vFftMeters[i];

                fm->vHistory[0]     = NULL;
                fm->vHistory[1]     = NULL;

                fm->nFftPeriod      = 0;
                fm->nFftFrame       = 0;
                fm->nFftHistory     = 0;

                for (size_t j=0; j < FG_TOTAL; ++j)
                {
                    fft_graph_t *fg     = &fm->vGraphs[i];

                    fg->vCurr           = NULL;
                    fg->vMin            = NULL;
                    fg->vMax            = NULL;
                }
            }

            for (size_t i=0; i < meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af         = &vSamples[i];

                af->pLoader         = NULL;
                af->pSample         = NULL;
                af->pLoaded         = NULL;
                af->nStatus         = STATUS_UNSPECIFIED;
                af->nLength         = 0;
                af->fGain           = GAIN_AMP_0_DB;
                af->bSync           = false;

                for (size_t j=0; j<meta::referencer::CHANNELS_MAX; ++j)
                    af->vThumbs[j]      = NULL;

                for (size_t j=0; j < meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al          = &af->vLoops[j];

                    al->nState          = PB_OFF;
                    al->nTransition     = 0;
                    al->nStart          = -1;
                    al->nEnd            = -1;
                    al->nPos            = -1;
                    al->bFirst          = true;

                    al->pStart          = NULL;
                    al->pEnd            = NULL;
                    al->pPlayPos        = NULL;
                }

                af->pFile           = NULL;
                af->pStatus         = NULL;
                af->pLength         = NULL;
                af->pMesh           = NULL;
                af->pGain           = NULL;
            }

            pData               = NULL;

//            const size_t sr = 48000;
//            in.init(2, sr * 30, 0);
//            lufs.init(2, sr * 30, 0);
//            tp.init(2, sr * 30, 0);
//            psr.init(2, sr * 30, 0);
//
//            in.set_sample_rate(sr);
//            lufs.set_sample_rate(sr);
//            tp.set_sample_rate(sr);
//            psr.set_sample_rate(sr);
        }

        referencer::~referencer()
        {
            do_destroy();

//            in.save("test-in.wav");
//            lufs.save("test-lufs.wav");
//            tp.save("test-tpeak.wav");
//            psr.save("test-psr.wav");
        }

        void referencer::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialization
            Module::init(wrapper, ports);

            // Save executor service
            pExecutor               = wrapper->executor();

            // Estimate the number of bytes to allocate
            const size_t num_graphs = (nChannels > 1) ? FG_TOTAL : 1;
            const size_t szof_fft   = sizeof(float) << meta::referencer::SPC_MAX_RANK;
            const size_t szof_spc   = align_size(sizeof(float) * meta::referencer::SPC_MESH_SIZE, OPTIMAL_ALIGN);
            const size_t szof_ind   = align_size(sizeof(uint16_t) * meta::referencer::SPC_MESH_SIZE, OPTIMAL_ALIGN);
            const size_t szof_history = align_size(sizeof(float) * meta::referencer::SPC_HISTORY_SIZE, OPTIMAL_ALIGN);
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t szof_buf         = align_size(sizeof(float) * BUFFER_SIZE, OPTIMAL_ALIGN);
            size_t szof_global_buf  = lsp_max(szof_buf * 2, szof_fft * 2 * 4);
            size_t alloc            =
                szof_channels +     // vChannels
                szof_global_buf +   // vBuffer
                szof_spc +          // vFftFreqs
                szof_ind +          // vFftInds
                szof_fft +          // vFftWindow
                szof_spc +          // vFftEnvelope
                nChannels * (
                    szof_buf            // vBuffer
                ) +
                2 * (               // vFftMeters
                    szof_history * nChannels + // vHistory
                    num_graphs * (      // vGraphs
                        szof_spc +         // vCurr
                        szof_spc +         // vMin
                        szof_spc           // vMax
                    )
                );

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialize pointers to channels and temporary buffer
            vChannels               = advance_ptr_bytes<channel_t>(ptr, szof_channels);
            vBuffer                 = advance_ptr_bytes<float>(ptr, szof_global_buf);
            vFftFreqs               = advance_ptr_bytes<float>(ptr, szof_spc);
            vFftInds                = advance_ptr_bytes<uint16_t>(ptr, szof_ind);
            vFftWindow              = advance_ptr_bytes<float>(ptr, szof_fft);
            vFftEnvelope            = advance_ptr_bytes<float>(ptr, szof_spc);

            // Initialize audio channels
            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sPostFilter.construct();

                // Initialize DSP processors
                if (!c->sPostFilter.init(1, meta::referencer::EQ_RANK))
                    return;
                c->sPostFilter.set_smooth(true);

                // Initialize fields
                c->vBuffer              = advance_ptr_bytes<float>(ptr, szof_buf);

                c->pIn                  = NULL;
                c->pOut                 = NULL;
            }

            // Initialize FFT meters
            for (size_t i=0; i < 2; ++i)
            {
                fft_meters_t *fm    = &vFftMeters[i];

                fm->vHistory[0]     = advance_ptr_bytes<float>(ptr, szof_history);
                if (nChannels > 1)
                    fm->vHistory[1]     = advance_ptr_bytes<float>(ptr, szof_history);

                for (size_t j=0; j < num_graphs; ++j)
                {
                    fft_graph_t *fg     = &fm->vGraphs[j];

                    fg->vCurr           = advance_ptr_bytes<float>(ptr, szof_spc);
                    fg->vMin            = advance_ptr_bytes<float>(ptr, szof_spc);
                    fg->vMax            = advance_ptr_bytes<float>(ptr, szof_spc);
                }
            }

            // Initialize dynamics meters
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                if (!dm->sRMSMeter.init(nChannels, dspu::bs::LUFS_MEASURE_PERIOD_MS))
                    return;

                dm->sRMSMeter.set_mode(dspu::SCM_RMS);
                dm->sRMSMeter.set_stereo_mode(dspu::SCSM_STEREO);
                dm->sRMSMeter.set_source(dspu::SCS_MIDDLE);
                dm->sRMSMeter.set_gain(GAIN_AMP_0_DB);
                dm->sRMSMeter.set_reactivity(dspu::bs::LUFS_MEASURE_PERIOD_MS);

                if (!dm->sTPMeter[0].init())
                    return;
                if (!dm->sTPMeter[1].init())
                    return;

                if (dm->sLUFSMeter.init(nChannels, dspu::bs::LUFS_MEASURE_PERIOD_MS) != STATUS_OK)
                    return;

                dm->sLUFSMeter.set_period(dspu::bs::LUFS_MEASURE_PERIOD_MS);
                dm->sLUFSMeter.set_weighting(dspu::bs::WEIGHT_K);

                if (nChannels > 1)
                {
                    dm->sLUFSMeter.set_active(0, true);
                    dm->sLUFSMeter.set_active(1, true);
                    dm->sLUFSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sLUFSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                }
                else
                {
                    dm->sLUFSMeter.set_active(0, true);
                    dm->sLUFSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
                }
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
            BIND_PORT(pLoopMesh);
            BIND_PORT(pLoopLen);
            BIND_PORT(pLoopPos);

            // Post-filter controls
            BIND_PORT(pPostMode);
            BIND_PORT(pPostSlope);
            BIND_PORT(pPostSel);
            for (size_t i=0; i < meta::referencer::POST_SPLITS; ++i)
                BIND_PORT(pPostSplit[i]);

            // Dynamics meters
            SKIP_PORT("Dynamics display source");
            BIND_PORT(pDynaMode);
            BIND_PORT(pDynaTime);

            // FFT metering
            BIND_PORT(pFftRank);
            BIND_PORT(pFftWindow);
            BIND_PORT(pFftEnvelope);
            BIND_PORT(pFftReactivity);

            // Operating mode
            if (nChannels > 1)
            {
                BIND_PORT(pMode);
            }

            // Meshes and meters
            BIND_PORT(pDynaMesh);
            BIND_PORT(pFftMesh);
            if (nChannels > 1)
            {
                SKIP_PORT("Goniometer history size");
                SKIP_PORT("Goniometer dots");
                BIND_PORT(pGoniometer);
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

            // Destroy meters
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                dm->sRMSMeter.destroy();
                dm->sTPMeter[0].destroy();
                dm->sTPMeter[1].destroy();
                dm->sTPDelay.destroy();
                dm->sLUFSMeter.destroy();
//                dm->sLUFSDelay.destroy();

                for (size_t j=0; j<DM_TOTAL; ++j)
                    dm->vGraphs[j].destroy();
            }

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                    c->sPostFilter.destroy();
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
            bUpdFft             = true;

            sMix.fGain          = sMix.fNewGain;
            sMix.fOldGain       = sMix.fNewGain;
            sMix.nTransition    = nCrossfadeTime;

            sRef.fGain          = sRef.fNewGain;
            sRef.fOldGain       = sRef.fNewGain;
            sRef.nTransition    = nCrossfadeTime;

            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af             = &vSamples[i];
                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al              = &af->vLoops[j];
                    al->nTransition         = lsp_min(al->nTransition, nCrossfadeTime);
                }
            }

            // Update goniometer settings
            nGonioPeriod        = dspu::hz_to_samples(fSampleRate, meta::referencer::GONIO_REFRESH_RATE);
            nGonioStrobe        = nGonioPeriod;

            // Update sample rate for the bypass processors
            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c        = &vChannels[i];
                c->sBypass.init(sr);
                c->sPostFilter.set_sample_rate(sr);
            }

            // Cleanup FFT buffers
            const size_t num_graphs = (nChannels > 1) ? FG_TOTAL : 1;
            for (size_t i=0; i < 2; ++i)
            {
                fft_meters_t *fm    = &vFftMeters[i];

                fm->nFftPeriod      = dspu::hz_to_samples(fSampleRate, meta::referencer::SPC_REFRESH_RATE);
                fm->nFftFrame       = 0;
                fm->nFftHistory     = 0;

                for (size_t j=0; j < num_graphs; ++j)
                {
                    fft_graph_t *fg     = &fm->vGraphs[j];

                    dsp::fill_zero(fg->vCurr, meta::referencer::SPC_MESH_SIZE);
                    dsp::fill_zero(fg->vMin, meta::referencer::SPC_MESH_SIZE);
                    dsp::fill_zero(fg->vMax, meta::referencer::SPC_MESH_SIZE);
                }
            }

            // Initialize FFT frequencies
            const float f_norm      = logf(SPEC_FREQ_MAX/SPEC_FREQ_MIN) / (meta::referencer::SPC_MESH_SIZE - 1);
            for (size_t i=0; i<meta::referencer::SPC_MESH_SIZE; ++i)
                vFftFreqs[i]            = SPEC_FREQ_MIN * expf(i * f_norm);

            // Update dynamics meters
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                dm->sRMSMeter.set_sample_rate(sr);
                dm->sTPMeter[0].set_sample_rate(sr);
                dm->sTPMeter[1].set_sample_rate(sr);
                dm->sLUFSMeter.set_sample_rate(sr);

                const size_t delay      = dspu::millis_to_samples(fSampleRate, dspu::bs::LUFS_MEASURE_PERIOD_MS * 0.25f);
                dm->sTPDelay.init(delay + BUFFER_SIZE);
                dm->sTPDelay.set_delay(delay - dm->sTPMeter[0].latency());

//                dm->sLUFSDelay.init(dm->sTPMeter[0].latency());
//                dm->sLUFSDelay.set_delay(dm->sTPMeter[0].latency());

                const size_t period     = dspu::seconds_to_samples(sr, meta::referencer::DYNA_TIME_MAX / meta::referencer::DYNA_MESH_SIZE);
                for (size_t j=0; j<DM_TOTAL; ++j)
                    dm->vGraphs[j].init(meta::referencer::DYNA_MESH_SIZE, meta::referencer::DYNA_SUBSAMPLING, period);
            }
        }

        referencer::stereo_mode_t referencer::decode_stereo_mode(size_t mode)
        {
            switch (mode)
            {
                case 0: return SM_STEREO;
                case 1: return SM_INVERSE_STEREO;
                case 2: return SM_MONO;
                case 3: return SM_SIDE;
                case 4: return SM_SIDES;
                case 5: return SM_MID_SIDE;
                case 6: return SM_SIDE_MID;
                case 7: return SM_LEFT_ONLY;
                case 8: return SM_LEFT;
                case 9: return SM_RIGHT;
                case 10: return SM_RIGHT_ONLY;
                default:
                    break;
            }

            return (nChannels > 1) ? SM_STEREO : SM_MONO;
        }

        dspu::equalizer_mode_t referencer::decode_equalizer_mode(size_t mode)
        {
            switch (mode)
            {
                case 0: return dspu::EQM_IIR;
                case 1: return dspu::EQM_FIR;
                case 2: return dspu::EQM_FFT;
                case 3: return dspu::EQM_SPM;
                default:
                    break;
            }

            return dspu::EQM_BYPASS;
        }

        void referencer::update_settings()
        {
            // Update playback state
            bool play               = pPlay->value() < 0.5f;
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

                if ((nPlaySample != play_sample) || (nPlayLoop != play_loop))
                    bSyncLoopMesh           = true;

                bPlay                   = play;
                nPlaySample             = play_sample;
                nPlayLoop               = play_loop;
            }

            // Update loop ranges
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af             = &vSamples[i];
                af->fGain               = af->pGain->value();

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al              = &af->vLoops[j];

                    const ssize_t first     = dspu::seconds_to_samples(fSampleRate, al->pStart->value());
                    const ssize_t last      = dspu::seconds_to_samples(fSampleRate, al->pEnd->value());
                    const ssize_t l_start   = al->nStart;
                    const ssize_t l_end     = al->nEnd;

                    al->nStart              = lsp_min(first, last);
                    al->nEnd                = lsp_max(first, last);

                    // Check if we need to syncrhonize loop mesh
                    if ((i == nPlaySample) && (j == nPlayLoop))
                    {
                        if ((al->nStart != l_start) || (al->nEnd != l_end))
                            bSyncLoopMesh           = true;
                    }
                }
            }

            // Apply post-filter settings
            dspu::equalizer_mode_t post_mode  = decode_equalizer_mode(pPostMode->value());
            const size_t post_slope = pPostSlope->value();
            const size_t post_sel   = pPostSel->value();
            const float post_hpf    = (post_sel >= PF_BASS) ? pPostSplit[post_sel - PF_BASS]->value() : -1.0f;
            const float post_lpf    = ((post_sel >= PF_SUB_BASS) && (post_sel < PF_HIGH)) ? pPostSplit[post_sel - PF_SUB_BASS]->value() : -1.0f;

            dspu::filter_params_t fp;
            fp.nSlope               = post_slope * 2;
            fp.fGain                = 1.0f;
            fp.fQuality             = 0.0f;

            if (post_hpf > 0.0f)
            {
                if (post_lpf > 0.0f)
                {
                    fp.nType            = dspu::FLT_BT_BWC_BANDPASS;
                    fp.fFreq            = post_hpf;
                    fp.fFreq2           = post_lpf;
                }
                else // post_lpf <= 0.0f
                {
                    fp.nType            = dspu::FLT_BT_BWC_HIPASS;
                    fp.fFreq            = post_hpf;
                    fp.fFreq2           = post_hpf;
                }
            }
            else // post_hpf <= 0.0f
            {
                if (post_lpf > 0.0f)
                {
                    fp.nType            = dspu::FLT_BT_BWC_LOPASS;
                    fp.fFreq            = post_lpf;
                    fp.fFreq2           = post_lpf;
                }
                else // post_lpf <= 0.0f
                {
                    fp.nType            = dspu::FLT_NONE;
                    fp.fFreq            = post_hpf;
                    fp.fFreq2           = post_lpf;
                    post_mode           = dspu::EQM_BYPASS;
                }
            }

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sPostFilter.set_params(0, &fp);
                c->sPostFilter.set_mode(post_mode);
            }

            nDynaMode               = pDynaMode->value();
            fDynaTime               = pDynaTime->value();
            const size_t period     = dspu::seconds_to_samples(fSampleRate, fDynaTime / float(meta::referencer::DYNA_MESH_SIZE));
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];
                for (size_t j=0; j<DM_TOTAL; ++j)
                {
                    dm->vGraphs[j].set_period(period);
                }
            }

            // Apply FFT analysis settings
            const float fft_react   = pFftReactivity->value();
            const size_t fft_rank   = meta::referencer::FFT_RANK_MIN + pFftRank->value();
            const size_t fft_window = pFftWindow->value();
            const size_t fft_env    = pFftEnvelope->value();
            const size_t fft_size   = 1 << fft_rank;

            fFftTau                 = 1.0f - expf(logf(1.0f - M_SQRT1_2) / dspu::seconds_to_samples(meta::referencer::SPC_REFRESH_RATE, fft_react));
            if (nFftRank != fft_rank)
            {
                nFftRank                = fft_rank;
                nFftWindow              = -1;
                nFftEnvelope            = -1;
                bUpdFft                 = true;
            }

            if (bUpdFft)
            {
                const float norm        = logf(SPEC_FREQ_MAX/SPEC_FREQ_MIN) / (meta::referencer::SPC_MESH_SIZE - 1);
                const float scale       = float(fft_size) / float(fSampleRate);
                const float fft_csize   = fft_size >> 1;

                for (size_t i=0; i<meta::referencer::SPC_MESH_SIZE; ++i)
                {
                    float f                 = SPEC_FREQ_MIN * expf(float(i) * norm);
                    size_t ix               = lsp_min(size_t(scale * f), fft_csize);

                    vFftFreqs[i]            = f;
                    vFftInds[i]             = ix;
                }

                for (size_t i=0; i<2; ++i)
                {
                    fft_meters_t *fm    = &vFftMeters[i];
                    dsp::fill_zero(fm->vHistory[0], meta::referencer::SPC_HISTORY_SIZE);
                    if (nChannels > 1)
                        dsp::fill_zero(fm->vHistory[1], meta::referencer::SPC_HISTORY_SIZE);
                }
                bUpdFft                 = false;
            }

            if (nFftWindow != fft_window)
            {
                nFftWindow              = fft_window;
                dspu::windows::window(vFftWindow, fft_size, dspu::windows::window_t(nFftWindow));
            }
            if (nFftEnvelope != fft_env)
            {
                nFftEnvelope            = fft_env;
                dspu::envelope::reverse_noise(vBuffer, fft_size + 1, dspu::envelope::envelope_t(nFftEnvelope));
                reduce_spectrum(vFftEnvelope, vBuffer);
                dsp::mul_k2(vFftEnvelope, GAIN_AMP_P_12_DB / fft_size, meta::referencer::SPC_MESH_SIZE);
            }

            // Apply configuration to channels
            bool bypass             = pBypass->value() >= 0.5f;
            size_t source           = pSource->value();
            enMode                  = (pMode != NULL) ? decode_stereo_mode(pMode->value()) : SM_MONO;

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                c->sBypass.set_bypass(bypass);
            }

            switch (source)
            {
                case SRC_MIX:
                    sMix.fOldGain           = sMix.fGain;
                    sMix.fNewGain           = GAIN_AMP_0_DB;
                    sMix.nTransition        = 0;

                    sRef.fOldGain           = sRef.fGain;
                    sRef.fNewGain           = GAIN_AMP_M_INF_DB;
                    sRef.nTransition        = 0;
                    break;

                case SRC_REFERENCE:
                    sMix.fOldGain           = sMix.fGain;
                    sMix.fNewGain           = GAIN_AMP_M_INF_DB;
                    sMix.nTransition        = 0;

                    sRef.fOldGain           = sRef.fGain;
                    sRef.fNewGain           = GAIN_AMP_0_DB;
                    sRef.nTransition        = 0;
                    break;

                case SRC_BOTH:
                default:
                    sMix.fOldGain           = sMix.fGain;
                    sMix.fNewGain           = GAIN_AMP_M_6_DB;
                    sMix.nTransition        = 0;

                    sRef.fOldGain           = sRef.fGain;
                    sRef.fNewGain           = GAIN_AMP_M_6_DB;
                    sRef.nTransition        = 0;
                    break;
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

        void referencer::make_thumbnail(float *dst, const float *src, size_t len, size_t dst_len)
        {
            for (size_t i=0; i<dst_len; ++i)
            {
                size_t first    = (i * len) / dst_len;
                size_t last     = ((i + 1) * len) / dst_len;
                if (first < last)
                    dst[i]          = dsp::abs_max(&src[first], last - first);
                else if (first < len)
                    dst[i]          = fabsf(src[first]);
                else
                    dst[i]          = 0.0f;
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
                make_thumbnail(af->vThumbs[i], source->channel(i), source->length(), meta::referencer::FILE_MESH_SIZE);
            }

            // Commit the result
            lsp_trace("file successfully loaded: %s", fname);
            lsp::swap(af->pLoaded, source);

            return STATUS_OK;
        }

        void referencer::preprocess_audio_channels()
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Get input and output buffers
                c->vIn                  = c->pIn->buffer<float>();
                c->vOut                 = c->pOut->buffer<float>();
            }
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

                    if (i == nPlaySample)
                        bSyncLoopMesh   = true;

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

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al              = &af->vLoops[j];
                    al->pPlayPos->set_value(dspu::samples_to_seconds(fSampleRate, al->nPos));
                }

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

        void referencer::output_loop_data()
        {
            afile_t *af             = &vSamples[nPlaySample];
            loop_t *al              = &af->vLoops[nPlayLoop];

            const ssize_t limit     = (af->pSample != NULL) ? af->pSample->length() : 0;
            const size_t channels   = (af->pSample != NULL) ? af->pSample->channels() : 0;

            const ssize_t start     = lsp_limit(al->nStart, 0, limit);
            const ssize_t end       = lsp_limit(al->nEnd, 0, limit);
            const size_t len        = ((al->nEnd >= 0) && (al->nStart >= 0)) ? end - start : 0;

            pLoopLen->set_value(dspu::samples_to_seconds(fSampleRate, len));
            pLoopPos->set_value(dspu::samples_to_seconds(fSampleRate, al->nPos - al->nStart));

            if (!bSyncLoopMesh)
                return;

            // Transfer file thumbnails to mesh
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pLoopMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            if ((channels > 0) && (al->nEnd >= 0) && (al->nStart >= 0))
            {
                // Copy thumbnails
                for (size_t i=0; i<channels; ++i)
                    make_thumbnail(mesh->pvData[i], af->pSample->channel(i, start), len, meta::referencer::FILE_MESH_SIZE);

                mesh->data(channels, meta::referencer::FILE_MESH_SIZE);
            }
            else
                mesh->data(0, 0);

            bSyncLoopMesh           = false;
        }

        void referencer::render_loop(afile_t *af, loop_t *al, size_t samples)
        {
            // Update position to match the loop range
            ssize_t to_process      = 0;
            bool crossfade          = false;
            const size_t length     = al->nEnd - al->nStart;
            if (length < nCrossfadeTime * 2)
                return;

            const size_t s_channels = af->pSample->channels();
            const float gain        = af->fGain;
            al->nPos                = lsp_limit(al->nPos, al->nStart, al->nEnd - 1);

            // Process loop playback
            for (size_t offset = 0; offset < samples; )
            {
                if (al->nState == PB_OFF)
                    break;

                ssize_t step_size   = (al->nState == PB_ACTIVE) ? samples - offset : lsp_min(nCrossfadeTime - al->nTransition, samples - offset);
                to_process          = lsp_min(al->nEnd - al->nPos, step_size);

                // Compute how many data we can do
                if ((!al->bFirst) && (al->nPos < ssize_t(nCrossfadeTime)))
                {
                    // We need to render cross-fade first
                    to_process          = lsp_min(nCrossfadeTime - al->nPos, to_process);
                    crossfade           = true;
                }
                else
                    crossfade           = false;

                // Process each channel independently
                for (size_t i=0; i<nChannels; ++i)
                {
                    // Obtain source and destination pointers
                    float *dst          = &vChannels[i].vBuffer[offset];
                    const float *src    = af->pSample->channel(i % s_channels, al->nPos);
                    if (crossfade)
                    {
                        dsp::lin_inter_mul3(
                            vBuffer, src,
                            0, GAIN_AMP_M_INF_DB, nCrossfadeTime, GAIN_AMP_0_DB,
                            al->nPos, to_process);
                        dsp::lin_inter_fmadd2(
                            vBuffer, &src[al->nEnd + al->nPos - nCrossfadeTime],
                            0, GAIN_AMP_0_DB, nCrossfadeTime, GAIN_AMP_M_INF_DB,
                            al->nPos, to_process);
                        src                 = vBuffer;
                    }

                    // Now we can process the sample
                    switch (al->nState)
                    {
                        case PB_FADE_OUT:
                            dsp::lin_inter_fmadd2(
                                dst, src,
                                0, gain, nCrossfadeTime, GAIN_AMP_M_INF_DB,
                                al->nTransition, to_process);
                            break;

                        case PB_FADE_IN:
                            dsp::lin_inter_fmadd2(
                                dst, src,
                                0, GAIN_AMP_M_INF_DB, nCrossfadeTime, gain,
                                al->nTransition, to_process);
                            break;

                        case PB_ACTIVE:
                        default:
                            dsp::mul_k3(dst, src, gain, to_process);
                            break;
                    }
                }

                // Update positions
                switch (al->nState)
                {
                    case PB_FADE_OUT:
                        al->nTransition += to_process;
                        if (al->nTransition >= nCrossfadeTime)
                            al->nState = PB_OFF;
                        break;

                    case PB_FADE_IN:
                        al->nTransition += to_process;
                        if (al->nTransition >= nCrossfadeTime)
                            al->nState = PB_ACTIVE;
                        break;

                    default:
                        break;
                }

                offset         += to_process;
                al->nPos       += to_process;
                if (al->nPos >= al->nEnd)
                {
                    al->nPos        = al->nStart;
                    al->bFirst      = false;
                }
            }
        }

        void referencer::prepare_reference_signal(size_t samples)
        {
            // Cleanup buffers
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];
                dsp::fill_zero(c->vBuffer, samples);
            }

            // Process each loop depending on it's state
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af = &vSamples[i];

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al = &af->vLoops[j];

                    // Check that file contains sample
                    if (af->pSample == NULL)
                    {
                        al->nPos        = -1;
                        break;
                    }

                    // Render sample loop
                    if (al->nState != PB_OFF)
                        render_loop(af, al, samples);
                }
            }
        }

        void referencer::apply_post_filters(size_t samples)
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];
                c->sPostFilter.process(c->vBuffer, c->vBuffer, samples);
            }
        }

        void referencer::mix_channels(size_t samples)
        {
            // Process reference signal first
            if (sRef.nTransition < nCrossfadeTime)
            {
                size_t to_process   = lsp_min(nCrossfadeTime - sRef.nTransition, samples);
                const float gain    = sRef.fOldGain + (sRef.nTransition * (sRef.fNewGain - sRef.fOldGain)) / nCrossfadeTime;

                // Apply envelope
                for (size_t i=0; i<nChannels; ++i)
                {
                    float *dst = vChannels[i].vBuffer;

                    dsp::lramp1(dst, sRef.fGain, gain, to_process);
                    if (to_process < samples)
                        dsp::mul_k2(&dst[to_process], gain, samples - to_process);
                }

                sRef.nTransition       += to_process;
                sRef.fGain              = (sRef.nTransition >= nCrossfadeTime) ? sRef.fNewGain : gain;
            }
            else
            {
                for (size_t i=0; i<nChannels; ++i)
                    dsp::mul_k2(vChannels[i].vBuffer, sRef.fGain, samples);
            }

            // Now process mix signal
            if (sMix.nTransition < nCrossfadeTime)
            {
                size_t to_process   = lsp_min(nCrossfadeTime - sMix.nTransition, samples);
                const float gain    = sMix.fOldGain + (sMix.nTransition * (sMix.fNewGain - sMix.fOldGain)) / nCrossfadeTime;

                // Apply envelope
                for (size_t i=0; i<nChannels; ++i)
                {
                    float *dst = vChannels[i].vBuffer;
                    const float *src = vChannels[i].vIn;

                    dsp::lramp_add2(dst, src, sMix.fGain, gain, to_process);
                    if (to_process < samples)
                        dsp::fmadd_k3(&dst[to_process], &src[to_process], gain, samples - to_process);
                }

                sMix.nTransition       += to_process;
                sMix.fGain              = (sMix.nTransition >= nCrossfadeTime) ? sMix.fNewGain : gain;
            }
            else
            {
                for (size_t i=0; i<nChannels; ++i)
                    dsp::fmadd_k3(vChannels[i].vBuffer, vChannels[i].vIn, sMix.fGain, samples);
            }
        }

        void referencer::apply_stereo_mode(size_t samples)
        {
            switch (enMode)
            {
                case SM_STEREO:
                    break;
                case SM_INVERSE_STEREO:
                    lsp::swap(vChannels[0].vBuffer, vChannels[1].vBuffer);
                    break;
                case SM_MONO:
                    dsp::lr_to_mid(vChannels[0].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    dsp::copy(vChannels[1].vBuffer, vChannels[0].vBuffer, samples);
                    break;
                case SM_SIDE:
                    dsp::lr_to_side(vChannels[0].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    dsp::copy(vChannels[1].vBuffer, vChannels[0].vBuffer, samples);
                    break;
                case SM_SIDES:
                    dsp::lr_to_side(vChannels[0].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    dsp::mul_k3(vChannels[1].vBuffer, vChannels[0].vBuffer, -1.0f, samples);
                    break;
                case SM_MID_SIDE:
                    dsp::lr_to_ms(vChannels[0].vBuffer, vChannels[1].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    break;
                case SM_SIDE_MID:
                    dsp::lr_to_ms(vChannels[1].vBuffer, vChannels[0].vBuffer, vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    break;
                case SM_LEFT:
                    dsp::copy(vChannels[1].vBuffer, vChannels[0].vBuffer, samples);
                    break;
                case SM_LEFT_ONLY:
                    dsp::fill_zero(vChannels[1].vBuffer, samples);
                    break;
                case SM_RIGHT_ONLY:
                    dsp::fill_zero(vChannels[0].vBuffer, samples);
                    break;
                case SM_RIGHT:
                    dsp::copy(vChannels[0].vBuffer, vChannels[1].vBuffer, samples);
                    break;
                default:
                    break;
            }
        }

        void referencer::reduce_spectrum(float *dst, const float *src)
        {
            for (size_t i=0; i<meta::referencer::SPC_MESH_SIZE; ++i)
                dst[i]      = src[vFftInds[i]];
        }

        void referencer::reduce_cspectrum(float *dst, const float *src)
        {
            for (size_t i=0; i<meta::referencer::SPC_MESH_SIZE; ++i)
            {
                const size_t index  = vFftInds[i];
                const float *v      = &src[index * 2];
                dst[0]              = v[0];
                dst[1]              = v[1];

                dst                += 2;
            }
        }

        void referencer::process_fft_frame(fft_meters_t *fm)
        {
            const size_t fft_size           = 1 << nFftRank;
            const size_t fft_xsize          = fft_size << 1;
            const size_t head               = (fm->nFftHistory + meta::referencer::SPC_HISTORY_SIZE - fft_size) % meta::referencer::SPC_HISTORY_SIZE;
            const size_t split              = meta::referencer::SPC_HISTORY_SIZE - head;

            if (nChannels > 1)
            {
                // Stereo processing
                float *fl       = vBuffer;
                float *fr       = &fl[fft_xsize];
                float *ft1      = &fr[fft_xsize];
                float *ft2      = &ft1[fft_xsize];

                // Prepare buffers
                if (split >= fft_size)
                {
                    dsp::mul3(fl, &fm->vHistory[0][head], &vFftWindow[0], fft_size);
                    dsp::mul3(fr, &fm->vHistory[1][head], &vFftWindow[0], fft_size);
                }
                else
                {
                    dsp::mul3(fl, &fm->vHistory[0][head], &vFftWindow[0], split);
                    dsp::mul3(&fl[split], &fm->vHistory[0][0], &vFftWindow[split], fft_size - split);

                    dsp::mul3(fr, &fm->vHistory[1][head], &vFftWindow[0], split);
                    dsp::mul3(&fr[split], &fm->vHistory[1][0], &vFftWindow[split], fft_size - split);
                }

                // Perform FFT transform
                dsp::pcomplex_r2c(ft1, fl, fft_size);
                dsp::packed_direct_fft(ft1, ft1, nFftRank);
                reduce_cspectrum(fl, ft1);

                dsp::pcomplex_r2c(ft1, fr, fft_size);
                dsp::packed_direct_fft(ft1, ft1, nFftRank);
                reduce_cspectrum(fr, ft1);

                // Analyze Mid and side signals
                dsp::lr_to_ms(ft1, ft2, fl, fr, meta::referencer::SPC_MESH_SIZE * 2);
                dsp::pcomplex_mod(ft1, ft1, meta::referencer::SPC_MESH_SIZE);
                dsp::pcomplex_mod(ft2, ft2, meta::referencer::SPC_MESH_SIZE);

                dsp::mix2(fm->vGraphs[FG_MID].vCurr, ft1, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_MID].vMax, ft1, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_MID].vMax, fm->vGraphs[FG_MID].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);

                dsp::mix2(fm->vGraphs[FG_SIDE].vCurr, ft2, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_SIDE].vMax, ft2, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_SIDE].vMax, fm->vGraphs[FG_SIDE].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);

                // Analyze complex correlation between left and right
                dsp::pcomplex_corr(ft2, fl, fr, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_CORR].vCurr, ft2, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_CORR].vMax, ft2, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_CORR].vMax, fm->vGraphs[FG_CORR].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);

                // Analyze left and right channels
                dsp::pcomplex_mod(fl, fl, meta::referencer::SPC_MESH_SIZE);
                dsp::pcomplex_mod(fr, fr, meta::referencer::SPC_MESH_SIZE);

                dsp::mix2(fm->vGraphs[FG_LEFT].vCurr, fl, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_LEFT].vMax, fl, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_LEFT].vMax, fm->vGraphs[FG_LEFT].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);

                dsp::mix2(fm->vGraphs[FG_RIGHT].vCurr, fr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_RIGHT].vMax, fr, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_RIGHT].vMax, fm->vGraphs[FG_RIGHT].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);

                // Analyze panorama between left and right channels
                dsp::depan_panl(ft1, fl, fr, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_PAN].vCurr, ft1, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fm->vGraphs[FG_PAN].vMax, ft1, meta::referencer::SPC_MESH_SIZE);
                dsp::mix2(fm->vGraphs[FG_PAN].vMax, fm->vGraphs[FG_LEFT].vCurr, 1.0 - fFftTau, fFftTau, meta::referencer::SPC_MESH_SIZE);
            }
            else
            {
                // Mono processing
            }
        }

        void referencer::perform_fft_analysis(fft_meters_t *fm, const float *l, const float *r, size_t samples)
        {
            for (size_t offset = 0; offset < samples; )
            {
                // Determine how many samples to process
                size_t tail_size    = meta::referencer::SPC_HISTORY_SIZE - fm->nFftHistory;
                size_t strobe       = fm->nFftPeriod - fm->nFftFrame;
                size_t to_do        = lsp_min(tail_size, strobe, samples - offset);

                // Append samples to history
                dsp::copy(&fm->vHistory[0][fm->nFftHistory], l, to_do);
                l                  += to_do;
                if (nChannels > 1)
                {
                    dsp::copy(&fm->vHistory[1][fm->nFftHistory], r, to_do);
                    r                  += to_do;
                }
                fm->nFftHistory     = (fm->nFftHistory + to_do) % meta::referencer::SPC_HISTORY_SIZE;

                // Perform FFT if necessary
                fm->nFftFrame      += to_do;
                if (fm->nFftFrame >= fm->nFftPeriod)
                {
                    process_fft_frame(fm);
                    fm->nFftFrame      %= fm->nFftPeriod;
                }

                offset             += to_do;
            }
        }

        void referencer::perform_metering(dyna_meters_t *dm, const float *l, const float *r, size_t samples)
        {
            float *b1       = vBuffer;
            float *b2       = &vBuffer[BUFFER_SIZE];
            float *in[2];
            in[0]           = const_cast<float *>(l);
            in[1]           = const_cast<float *>(r);

            if (nChannels > 1)
            {
                // Compute Peak values
                dsp::pamax3(b1, l, r, samples);
                dm->vGraphs[DM_PEAK].process(b1, samples);

                // Compute True Peak values
                dm->sTPMeter[0].process(b1, l, samples);
                dm->sTPMeter[1].process(b2, r, samples);
                dsp::pmax2(b1, b2, samples);
                dm->vGraphs[DM_TRUE_PEAK].process(b1, samples);
                dm->sTPDelay.process(b1, b1, samples);

                // Compute RMS values
                dm->sRMSMeter.process(b2, const_cast<const float **>(in), samples);
                dm->vGraphs[DM_RMS].process(b2, samples);

                // Compute LUFS value
                dm->sLUFSMeter.bind(0, NULL, l, 0);
                dm->sLUFSMeter.bind(1, NULL, r, 0);
                dm->sLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
//                dm->sLUFSDelay.process(b2, b2, samples);
                dm->vGraphs[DM_LUFS].process(b2, samples);

//                if (dm == &vDynaMeters[0])
//                {
//                    size_t offset = this->in.length();
//                    this->in.append(samples);
//                    dsp::copy(this->in.channel(0, offset), l, samples);
//                    dsp::copy(this->in.channel(1, offset), r, samples);
//
//                    offset = lufs.length();
//                    lufs.append(samples);
//                    dsp::copy(lufs.channel(0, offset), b2, samples);
//
//                    offset = tp.length();
//                    tp.append(samples);
//                    dsp::copy(tp.channel(0, offset), b1, samples);
//                }
            }
            else
            {
                // Compute Peak values
                dsp::abs2(b1, l, samples);
                dm->vGraphs[DM_PEAK].process(b1, samples);

                // Compute True Peak values
                dm->sTPMeter[0].process(b1, l, samples);
                dm->vGraphs[DM_TRUE_PEAK].process(b1, samples);
                dm->sTPDelay.process(b1, b1, samples);

                // Compute RMS values
                dm->sRMSMeter.process(b2, const_cast<const float **>(in), samples);
                dm->vGraphs[DM_RMS].process(b2, samples);

                // Compute LUFS value
                dm->sLUFSMeter.bind(0, NULL, l, 0);
                dm->sLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
//                dm->sLUFSDelay.process(b2, b2, samples);
                dm->vGraphs[DM_LUFS].process(b2, samples);
            }

            // Now b1 contains True Peak value and b2 contains LUFS value
            // Compute the PSR value as True Peak / LUFS
            for (size_t i=0; i<samples; ++i)
            {
                const float peak    = b1[i];
                const float lufs    = b2[i];
                b1[i]               = (lufs >= GAIN_AMP_M_60_DB) ? peak / lufs : 0.0f;
            }

//            if (dm == &vDynaMeters[0])
//            {
//                size_t offset = psr.length();
//                psr.append(samples);
//                dsp::copy(psr.channel(0, offset), b1, samples);
//            }

            dm->vGraphs[DM_PSR].process(b1, samples);
        }

        void referencer::process_goniometer(
            const float *l1, const float *r1,
            const float *l2, const float *r2,
            size_t samples)
        {
            // Check that stream is present
            if (pGoniometer == NULL)
                return;
            plug::stream_t *stream = pGoniometer->buffer<plug::stream_t>();
            if (stream == NULL)
                return;

            float *mid  = vBuffer;
            float *side = &mid[BUFFER_SIZE];

            for (size_t offset=0; offset < samples; )
            {
                const size_t count  = stream->add_frame(samples - offset);     // Add a frame

                // Form the strobe signal
                dsp::fill_zero(mid, count);

                for (size_t i=0; i < count; )
                {
                    if (nGonioStrobe == 0)
                    {
                        mid[i]                  = 1.0f;
                        nGonioStrobe            = nGonioPeriod;
                    }

                    const size_t advance    = lsp_min(count - i, nGonioStrobe);
                    nGonioStrobe           -= advance;
                    i                      += advance;
                }
                stream->write_frame(0, mid, 0, count);

                // Perform analysis of the first pair
                dsp::lr_to_ms(mid, side, l1, r1, count);
//                for (size_t i=0; i < count; ++i)
//                {
//
//                }

                stream->write_frame(1, side, 0, count);
                stream->write_frame(2, mid, 0, count);
                l1             += count;
                r1             += count;

                // Perform analysis of the second pair
                dsp::lr_to_ms(mid, side, l2, r2, count);
                stream->write_frame(3, side, 0, count);
                stream->write_frame(4, mid, 0, count);
                l2             += count;
                r2             += count;

                // Commit frame
                stream->commit_frame();
                offset         += count;
            }
        }

        void referencer::process(size_t samples)
        {
            preprocess_audio_channels();
            process_file_requests();

            for (size_t offset = 0; offset < samples; )
            {
                const size_t to_process = lsp_min(samples - offset, BUFFER_SIZE);

                prepare_reference_signal(to_process);

                // Measure input and reference signal parameters
                perform_fft_analysis(
                    &vFftMeters[0],
                    vChannels[0].vIn,
                    (nChannels > 1) ? vChannels[1].vIn : NULL,
                    to_process);
                perform_fft_analysis(
                    &vFftMeters[1],
                    vChannels[0].vBuffer,
                    (nChannels > 1) ? vChannels[1].vBuffer : NULL,
                    to_process);

                perform_metering(
                    &vDynaMeters[0],
                    vChannels[0].vIn,
                    (nChannels > 1) ? vChannels[1].vIn : NULL,
                    to_process);
                perform_metering(
                    &vDynaMeters[1],
                    vChannels[0].vBuffer,
                    (nChannels > 1) ? vChannels[1].vBuffer : NULL,
                    to_process);

                if (nChannels > 1)
                    process_goniometer(
                        vChannels[0].vIn, vChannels[1].vIn,
                        vChannels[0].vBuffer, vChannels[1].vBuffer,
                        to_process);

                mix_channels(to_process);
                apply_post_filters(to_process);

                if (nChannels > 1)
                    apply_stereo_mode(to_process);

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c = &vChannels[i];
                    dsp::copy(c->vOut, c->vBuffer, to_process);

                    c->vIn             += to_process;
                    c->vOut            += to_process;
                }

                offset             += to_process;
            }

            output_file_data();
            output_loop_data();
            output_dyna_meters();
            output_spectrum_analysis();
        }

        void referencer::output_dyna_meters()
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pDynaMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Generate timestamp
            float *t = mesh->pvData[0];
            float *s = mesh->pvData[1];
            float *r = mesh->pvData[2];

            dsp::lramp_set1(&t[2], fDynaTime, 0.0f, meta::referencer::DYNA_MESH_SIZE);

            // Output graphs
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];
                dspu::ScaledMeterGraph *mg    = &dm->vGraphs[nDynaMode];
                mg->read(&mesh->pvData[i + 1][2], meta::referencer::DYNA_MESH_SIZE);
            }

            // Generate end points
            t[0]    = meta::referencer::DYNA_TIME_MAX + 0.5f;
            t[1]    = t[0];
            s[0]    = GAIN_AMP_M_INF_DB;
            s[1]    = s[2];
            r[0]    = GAIN_AMP_M_INF_DB;
            r[1]    = r[2];

            t     += meta::referencer::DYNA_MESH_SIZE + 2;
            s     += meta::referencer::DYNA_MESH_SIZE + 2;
            r     += meta::referencer::DYNA_MESH_SIZE + 2;

            t[0]    = - 0.5f;
            t[1]    = t[0];
            s[0]    = s[-1];
            s[1]    = GAIN_AMP_M_INF_DB;
            r[0]    = r[-1];
            r[1]    = GAIN_AMP_M_INF_DB;

            // Commit data to mesh
            mesh->data(3, meta::referencer::DYNA_MESH_SIZE + 4);
        }

        void referencer::output_spectrum_analysis()
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pFftMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            size_t rows = 0;

            // Frequencies
            float *t =  mesh->pvData[rows++];
            dsp::copy(&t[2], vFftFreqs, meta::referencer::SPC_MESH_SIZE);
            t[0]    = SPEC_FREQ_MIN * 0.5f;
            t[1]    = SPEC_FREQ_MIN * 0.5f;
            t      += meta::referencer::SPC_MESH_SIZE + 2;
            t[0]    = SPEC_FREQ_MAX * 2.0f;
            t[1]    = SPEC_FREQ_MAX * 2.0f;

            for (size_t i=0; i<2; ++i)
            {
                fft_meters_t *fm = &vFftMeters[i];

                if (nChannels > 1)
                {
                    // Left channel
                    t =  mesh->pvData[rows++];
                    dsp::mul3(&t[2], fm->vGraphs[FG_LEFT].vCurr, vFftEnvelope, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = GAIN_AMP_M_INF_DB;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = GAIN_AMP_M_INF_DB;

                    // Right channel
                    t =  mesh->pvData[rows++];
                    dsp::mul3(&t[2], fm->vGraphs[FG_RIGHT].vCurr, vFftEnvelope, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = GAIN_AMP_M_INF_DB;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = GAIN_AMP_M_INF_DB;

                    // Middle channel
                    t =  mesh->pvData[rows++];
                    dsp::mul3(&t[2], fm->vGraphs[FG_MID].vCurr, vFftEnvelope, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = GAIN_AMP_M_INF_DB;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = GAIN_AMP_M_INF_DB;

                    // Side channel
                    t =  mesh->pvData[rows++];
                    dsp::mul3(&t[2], fm->vGraphs[FG_SIDE].vCurr, vFftEnvelope, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = GAIN_AMP_M_INF_DB;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = GAIN_AMP_M_INF_DB;

                    // Correlation
                    t =  mesh->pvData[rows++];
                    dsp::copy(&t[2], fm->vGraphs[FG_CORR].vCurr, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = 0.0f;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = 0.0f;

                    // Panorama
                    t =  mesh->pvData[rows++];
                    dsp::copy(&t[2], fm->vGraphs[FG_PAN].vCurr, meta::referencer::SPC_MESH_SIZE);
                    t[0]    = 0.5f;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = 0.5f;
                }
                else
                {
                    // TODO
                }
            }

            mesh->data(rows, meta::referencer::SPC_MESH_SIZE + 4);
        }

        void referencer::ui_activated()
        {
            // Mark all samples needed for synchronization
            bSyncLoopMesh       = true;

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


