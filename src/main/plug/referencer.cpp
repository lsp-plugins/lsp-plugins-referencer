/*
 * Copyright (C) 2025 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2025 Vladimir Sadovnikov <sadko4u@gmail.com>
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

        const float referencer::dm_endpoints[] =
        {
            GAIN_AMP_M_INF_DB,  // DM_PEAK
            GAIN_AMP_M_INF_DB,  // DM_TRUE_PEAK
            GAIN_AMP_M_INF_DB,  // DM_RMS
            GAIN_AMP_M_INF_DB,  // DM_M_LUFS
            GAIN_AMP_M_INF_DB,  // DM_S_LUFS
            GAIN_AMP_M_INF_DB,  // DM_L_LUFS
            GAIN_AMP_M_INF_DB,  // DM_I_LUFS
            GAIN_AMP_0_DB,      // DM_PSR
            0,                  // DM_CORR
            0.5f,               // DM_PAN
            0.5f,               // DM_MSBAL
        };

        const float referencer::fft_endpoints[] =
        {
            GAIN_AMP_M_INF_DB, GAIN_AMP_M_INF_DB, GAIN_AMP_P_24_DB,     // FG_LEFT
            GAIN_AMP_M_INF_DB, GAIN_AMP_M_INF_DB, GAIN_AMP_P_24_DB,     // FG_RIGHT
            GAIN_AMP_M_INF_DB, GAIN_AMP_M_INF_DB, GAIN_AMP_P_24_DB,     // FG_MID
            GAIN_AMP_M_INF_DB, GAIN_AMP_M_INF_DB, GAIN_AMP_P_24_DB,     // FG_SIDE
            0.0f, -2.0f, 2.0f,                                          // FG_CORR
            0.5f, -1.0f, 2.0f,                                          // FG_PAN
            0.0f, -1.0f, 2.0f,                                          // FG_MSBAL
        };

        referencer::referencer(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels           = 0;
            nPlaySample         = -1;
            nPlayLoop           = -1;
            nGainMatching       = MATCH_NONE;
            fGainMatchGrow      = 1.0f;
            fGainMatchFall      = 1.0f;
            nCrossfadeTime      = 0;
            fMaxTime            = 0.0f;
            vBuffer             = NULL;
            vFftFreqs           = NULL;
            vFftInds            = NULL;
            vFftWindow          = NULL;
            vFftEnvelope        = NULL;
            vPsrLevels          = NULL;
            nFftRank            = 0;
            nFftWindow          = -1;
            nFftEnvelope        = -1;
            fFftTau             = 0.0f;
            fFftBal             = 0.0f;
            nFftSrc             = 0;
            fFftFreq            = 0.0f;
            nGonioPeriod        = 0;
            nPsrMode            = PSR_DENSITY;
            nPsrThresh          = 0;
            fPSRDecay            = 0.0f;
            bPlay               = false;
            bSyncLoopMesh       = true;
            bUpdFft             = true;
            bFftDamping         = true;
            bFreeze             = false;

            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels           = NULL;
            enMode              = (nChannels > 1) ? SM_STEREO : SM_MONO;
            fWaveformLen        = 0.0f;

            sMix.fGain          = GAIN_AMP_M_INF_DB;
            sMix.fOldGain       = GAIN_AMP_M_INF_DB;
            sMix.fNewGain       = GAIN_AMP_M_INF_DB;
            sMix.nTransition    = 0;
            sMix.fWaveformOff   = 0.0f;
            sMix.pFrameOffset   = NULL;

            sRef.fGain          = GAIN_AMP_M_INF_DB;
            sRef.fOldGain       = GAIN_AMP_M_INF_DB;
            sRef.fNewGain       = GAIN_AMP_M_INF_DB;
            sRef.nTransition    = 0;
            sRef.fWaveformOff   = 0.0f;
            sRef.pFrameOffset   = NULL;

            pExecutor           = NULL;

            pBypass             = NULL;
            pFreeze             = NULL;
            pPlay               = NULL;
            pPlaySample         = NULL;
            pPlayLoop           = NULL;
            pSource             = NULL;
            pLoopMesh           = NULL;
            pLoopLen            = NULL;
            pLoopPos            = NULL;
            pGainMatching       = NULL;
            pGainMatchReact     = NULL;
            pMode               = NULL;

            pFltPos             = NULL;
            pFltMode            = NULL;
            pFltSlope           = NULL;
            pFltSel             = NULL;

            for (size_t i=0; i < meta::referencer::FLT_SPLITS; ++i)
                pFltSplit[i]       = NULL;

            pMaxTime            = NULL;
            pLLUFSTime          = NULL;
            pDynaMesh           = NULL;

            pWaveformMesh       = NULL;
            pFrameLength        = NULL;

            pFftRank            = NULL;
            pFftWindow          = NULL;
            pFftEnvelope        = NULL;
            pFftReactivity      = NULL;
            pFftDamping         = NULL;
            pFftReset           = NULL;
            pFftBallistics      = NULL;
            for (size_t i=0; i<FT_TOTAL; ++i)
                pFftMesh[i]         = NULL;
            pFftVMarkSrc        = NULL;
            pFftVMarkFreq       = NULL;
            pFftVMarkVal        = NULL;

            pPsrPeriod          = NULL;
            pPsrThreshold       = NULL;
            pPsrMesh            = NULL;
            pPsrDisplay         = NULL;

            for (size_t i=0; i < 2; ++i)
            {
                dyna_meters_t *dm   = &vDynaMeters[i];

                dm->vLoudness       = NULL;
                dm->fGain           = GAIN_AMP_0_DB;
                dm->fPSRLevel        = 0.0;
                dm->nGonioStrobe    = 0;
                dm->pGoniometer     = NULL;

                for (size_t i=0; i<DM_TOTAL; ++i)
                    dm->pMeters[i]      = NULL;
                dm->pPsrPcValue     = NULL;
            }

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

                    for (size_t j=0; j<FT_TOTAL; ++j)
                        fg->vData[j]        = NULL;
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
            const size_t num_graphs = (nChannels > 1) ? FG_TOTAL : 1;
            const size_t szof_fft   = sizeof(float) << meta::referencer::SPC_MAX_RANK;
            const size_t szof_spc   = align_size(sizeof(float) * meta::referencer::SPC_MESH_SIZE, OPTIMAL_ALIGN);
            const size_t szof_ind   = align_size(sizeof(uint16_t) * meta::referencer::SPC_MESH_SIZE, OPTIMAL_ALIGN);
            const size_t szof_history = align_size(sizeof(float) * meta::referencer::SPC_HISTORY_SIZE, OPTIMAL_ALIGN);
            const size_t szof_psr   = align_size(sizeof(float) * meta::referencer::PSR_MESH_SIZE, OPTIMAL_ALIGN);
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
                szof_psr +          // vPsrLevels
                nChannels * (
                    szof_buf +          // vBuffer
                    szof_buf            // vInBuffer
                ) +
                2 * (               // vDynaMeters
                    szof_buf            // vLoudness
                ) +
                2 * (               // vFftMeters
                    szof_history * nChannels +  // vHistory
                    num_graphs * (      // vGraphs
                        szof_spc * FT_TOTAL     // Curr, Min, Max
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
            vPsrLevels              = advance_ptr_bytes<float>(ptr, szof_psr);

            // Initialize audio channels
            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->vPreFilters[0].construct();
                c->vPreFilters[1].construct();
                c->sPostFilter.construct();

                // Initialize DSP processors
                if (!c->vPreFilters[0].init(1, meta::referencer::EQ_RANK))
                    return;
                if (!c->vPreFilters[1].init(1, meta::referencer::EQ_RANK))
                    return;
                if (!c->sPostFilter.init(1, meta::referencer::EQ_RANK))
                    return;
                c->vPreFilters[0].set_smooth(true);
                c->vPreFilters[1].set_smooth(true);
                c->sPostFilter.set_smooth(true);

                // Initialize fields
                c->vBuffer              = advance_ptr_bytes<float>(ptr, szof_buf);
                c->vInBuffer            = advance_ptr_bytes<float>(ptr, szof_buf);

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

                    for (size_t k=0; k<FT_TOTAL; ++k)
                        fg->vData[k]        = advance_ptr_bytes<float>(ptr, szof_spc);
                }
            }

            // Initialize dynamics meters
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                if (!dm->sRMSMeter.init(nChannels, dspu::bs::LUFS_MEASURE_PERIOD_MS))
                    return;

                dm->vLoudness           = advance_ptr_bytes<float>(ptr, szof_buf);

                dm->sRMSMeter.set_mode(dspu::SCM_RMS);
                dm->sRMSMeter.set_stereo_mode(dspu::SCSM_STEREO);
                dm->sRMSMeter.set_source(dspu::SCS_MIDDLE);
                dm->sRMSMeter.set_gain(GAIN_AMP_0_DB);
                dm->sRMSMeter.set_reactivity(dspu::bs::LUFS_MEASURE_PERIOD_MS);

                if (!dm->sTPMeter[0].init())
                    return;
                if (!dm->sTPMeter[1].init())
                    return;

                if (dm->sAutogainMeter.init(nChannels, meta::referencer::AUTOGAIN_MEASURE_PERIOD) != STATUS_OK)
                    return;
                if (dm->sMLUFSMeter.init(nChannels, dspu::bs::LUFS_MOMENTARY_PERIOD) != STATUS_OK)
                    return;
                if (dm->sSLUFSMeter.init(nChannels, dspu::bs::LUFS_SHORT_TERM_PERIOD) != STATUS_OK)
                    return;
                if (dm->sLLUFSMeter.init(nChannels, meta::referencer::ILUFS_TIME_MAX, dspu::bs::LUFS_MOMENTARY_PERIOD) != STATUS_OK)
                    return;
                if (dm->sILUFSMeter.init(nChannels, 0, dspu::bs::LUFS_MOMENTARY_PERIOD) != STATUS_OK)
                    return;

                dm->sCorrMeter.construct();
                dm->sPanometer.construct();
                dm->sMsBalance.construct();

                dm->sPSRStats.construct();

                dm->sAutogainMeter.set_period(dspu::bs::LUFS_SHORT_TERM_PERIOD);
                dm->sAutogainMeter.set_weighting(dspu::bs::WEIGHT_K);
                dm->sMLUFSMeter.set_period(dspu::bs::LUFS_MOMENTARY_PERIOD);
                dm->sMLUFSMeter.set_weighting(dspu::bs::WEIGHT_K);
                dm->sSLUFSMeter.set_period(dspu::bs::LUFS_SHORT_TERM_PERIOD);
                dm->sSLUFSMeter.set_weighting(dspu::bs::WEIGHT_K);
                dm->sLLUFSMeter.set_weighting(dspu::bs::WEIGHT_K);
                dm->sILUFSMeter.set_weighting(dspu::bs::WEIGHT_K);

                if (nChannels > 1)
                {
                    dm->sAutogainMeter.set_active(0, true);
                    dm->sAutogainMeter.set_active(1, true);
                    dm->sAutogainMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sAutogainMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                    dm->sMLUFSMeter.set_active(0, true);
                    dm->sMLUFSMeter.set_active(1, true);
                    dm->sMLUFSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sMLUFSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                    dm->sSLUFSMeter.set_active(0, true);
                    dm->sSLUFSMeter.set_active(1, true);
                    dm->sSLUFSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sSLUFSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                    dm->sLLUFSMeter.set_active(0, true);
                    dm->sLLUFSMeter.set_active(1, true);
                    dm->sLLUFSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sLLUFSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);

                    dm->sILUFSMeter.set_active(0, true);
                    dm->sILUFSMeter.set_active(1, true);
                    dm->sILUFSMeter.set_designation(0, dspu::bs::CHANNEL_LEFT);
                    dm->sILUFSMeter.set_designation(1, dspu::bs::CHANNEL_RIGHT);
                }
                else
                {
                    dm->sAutogainMeter.set_active(0, true);
                    dm->sAutogainMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                    dm->sMLUFSMeter.set_active(0, true);
                    dm->sMLUFSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                    dm->sSLUFSMeter.set_active(0, true);
                    dm->sSLUFSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                    dm->sLLUFSMeter.set_active(0, true);
                    dm->sLLUFSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);

                    dm->sILUFSMeter.set_active(0, true);
                    dm->sILUFSMeter.set_designation(0, dspu::bs::CHANNEL_CENTER);
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
            SKIP_PORT("Mix graph visibility");
            SKIP_PORT("Reference graph visibility");
            SKIP_PORT("Current graphs visibility");
            SKIP_PORT("Minimum graphs visibility");
            SKIP_PORT("Maximum graphs visibility");
            BIND_PORT(pFreeze);
            BIND_PORT(pLoopMesh);
            BIND_PORT(pLoopLen);
            BIND_PORT(pLoopPos);
            BIND_PORT(pGainMatching);
            BIND_PORT(pGainMatchReact);

            // Post-filter controls
            BIND_PORT(pFltPos);
            BIND_PORT(pFltMode);
            BIND_PORT(pFltSlope);
            BIND_PORT(pFltSel);
            for (size_t i=0; i < meta::referencer::FLT_SPLITS; ++i)
                BIND_PORT(pFltSplit[i]);

            // Common graph parameters
            BIND_PORT(pMaxTime);

            // Loudness graph parameters
            BIND_PORT(pLLUFSTime);
            SKIP_PORT("Peak graph visible");
            SKIP_PORT("True Peak graph visible");
            SKIP_PORT("RMS graph visible");
            SKIP_PORT("Momentary LUFS graph visible");
            SKIP_PORT("Short-term LUFS graph visible");
            SKIP_PORT("Long-term LUFS graph visible");
            SKIP_PORT("Integrated LUFS graph visible");

            // PSR metering
            BIND_PORT(pPsrPeriod);
            BIND_PORT(pPsrThreshold);
            BIND_PORT(pPsrDisplay);
            BIND_PORT(pPsrMesh);

            // Waveform-related ports
            BIND_PORT(sMix.pFrameOffset);
            BIND_PORT(sRef.pFrameOffset);
            BIND_PORT(pFrameLength);
            SKIP_PORT("Logarithmic scale of waveform");
            SKIP_PORT("Minimum Waveform scale");
            SKIP_PORT("Maximum Waveform scale");

            // FFT metering
            SKIP_PORT("FFT horizontal marker");
            SKIP_PORT("FFT horizontal marker visibility");
            BIND_PORT(pFftVMarkSrc);
            BIND_PORT(pFftVMarkFreq);
            BIND_PORT(pFftVMarkVal);
            BIND_PORT(pFftRank);
            BIND_PORT(pFftWindow);
            BIND_PORT(pFftEnvelope);
            BIND_PORT(pFftReactivity);
            BIND_PORT(pFftDamping);
            BIND_PORT(pFftReset);
            BIND_PORT(pFftBallistics);

            // Operating mode
            if (nChannels > 1)
            {
                BIND_PORT(pMode);
                SKIP_PORT("Correlation view mode");
                SKIP_PORT("Stereo view type");
                SKIP_PORT("Stereo view mode");
                SKIP_PORT("Left channel visibility");
                SKIP_PORT("Right channel visibility");
                SKIP_PORT("Middle channel visibility");
                SKIP_PORT("Side channel visibility");
            }

            // Meshes and meters
            BIND_PORT(pDynaMesh);
            BIND_PORT(pWaveformMesh);

            for (size_t i=0; i<FT_TOTAL; ++i)
                BIND_PORT(pFftMesh[i]);

            if (nChannels > 1)
            {
                SKIP_PORT("Goniometer history size");
                SKIP_PORT("Goniometer dots");

                for (size_t i=0; i<2; ++i)
                {
                    dyna_meters_t *dm   = &vDynaMeters[i];
                    BIND_PORT(dm->pGoniometer);

                    for (size_t j=0; j<DM_STEREO; ++j)
                        BIND_PORT(dm->pMeters[j]);
                    BIND_PORT(dm->pPsrPcValue);
                }
            }
            else
            {
                for (size_t i=0; i<2; ++i)
                {
                    dyna_meters_t *dm   = &vDynaMeters[i];
                    for (size_t j=0; j<DM_MONO; ++j)
                        BIND_PORT(dm->pMeters[j]);
                    BIND_PORT(dm->pPsrPcValue);
                }
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

            // Initialize PSR levels
            const float psr_delta   = (meta::referencer::PSR_MAX_LEVEL - meta::referencer::PSR_MIN_LEVEL) / meta::referencer::PSR_MESH_SIZE;
            for (size_t i=0; i<meta::referencer::PSR_MESH_SIZE; ++i)
                vPsrLevels[i]       = dspu::db_to_gain(meta::referencer::PSR_MIN_LEVEL + psr_delta * i);
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
                dm->sPSRDelay.destroy();
                dm->sAutogainMeter.destroy();
                dm->sMLUFSMeter.destroy();
                dm->sSLUFSMeter.destroy();
                dm->sLLUFSMeter.destroy();
                dm->sILUFSMeter.destroy();
                dm->sCorrMeter.destroy();
                dm->sPanometer.destroy();
                dm->sMsBalance.destroy();

                for (size_t j=0; j<WF_TOTAL; ++j)
                    dm->vWaveform[j].destroy();

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
                    c->vPreFilters[0].destroy();
                    c->vPreFilters[1].destroy();
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
            const double tpd    = double(meta::referencer::PSR_TRUE_PEAK_DECAY * 0.1 * M_LN10) / double(sr);
            fPSRDecay            = exp(tpd);

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

            // Update sample rate for the bypass processors
            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c        = &vChannels[i];
                c->sBypass.init(sr);
                c->vPreFilters[0].set_sample_rate(sr);
                c->vPreFilters[1].set_sample_rate(sr);
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
                    const float dfl     = fft_endpoints[j * FT_TOTAL];

                    for (size_t k=0; k<FT_TOTAL; ++k)
                        dsp::fill(fg->vData[k], dfl, meta::referencer::SPC_MESH_SIZE);
                }
            }

            // Initialize FFT frequencies
            const float f_norm      = logf(SPEC_FREQ_MAX/SPEC_FREQ_MIN) / (meta::referencer::SPC_MESH_SIZE - 1);
            for (size_t i=0; i<meta::referencer::SPC_MESH_SIZE; ++i)
                vFftFreqs[i]            = SPEC_FREQ_MIN * expf(i * f_norm);

            // Update dynamics meters
            const size_t max_wf_len     = dspu::seconds_to_samples(sr, meta::referencer::WAVE_OFFSET_MAX + meta::referencer::WAVE_SIZE_MAX);
            const size_t corr_period    = dspu::millis_to_samples(sr, meta::referencer::CORR_PERIOD);
            const size_t max_psr_period = dspu::seconds_to_samples(sr, meta::referencer::PSR_PERIOD_MAX);
            const size_t dmesh_period   = dspu::seconds_to_samples(sr, meta::referencer::DYNA_TIME_MAX / meta::referencer::DYNA_MESH_SIZE);

            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                dm->sRMSMeter.set_sample_rate(sr);
                dm->sTPMeter[0].set_sample_rate(sr);
                dm->sTPMeter[1].set_sample_rate(sr);

                dm->sAutogainMeter.set_sample_rate(sr);
                dm->sMLUFSMeter.set_sample_rate(sr);
                dm->sSLUFSMeter.set_sample_rate(sr);
                dm->sLLUFSMeter.set_sample_rate(sr);
                dm->sILUFSMeter.set_sample_rate(sr);

                const size_t delay      = dspu::millis_to_samples(fSampleRate, dspu::bs::LUFS_MEASURE_PERIOD_MS * 0.5f);
                dm->sPSRDelay.init(delay + BUFFER_SIZE);
                dm->sPSRDelay.set_delay(0); //delay - dm->sTPMeter[0].latency());

                dm->sCorrMeter.init(corr_period);
                dm->sCorrMeter.set_period(corr_period);
                dm->sCorrMeter.clear();

                dm->sPanometer.init(corr_period);
                dm->sPanometer.set_period(corr_period);
                dm->sPanometer.set_pan_law(dspu::PAN_LAW_EQUAL_POWER);
                dm->sPanometer.set_default_pan(0.5f);
                dm->sPanometer.clear();

                dm->sMsBalance.init(corr_period);
                dm->sMsBalance.set_period(corr_period);
                dm->sMsBalance.set_pan_law(dspu::PAN_LAW_LINEAR);
                dm->sMsBalance.set_default_pan(0.0f);
                dm->sMsBalance.clear();

                dm->sPSRStats.init(max_psr_period, meta::referencer::PSR_MESH_SIZE);
                dm->sPSRStats.set_range(
                    meta::referencer::PSR_MIN_LEVEL,
                    meta::referencer::PSR_MAX_LEVEL,
                    meta::referencer::PSR_MESH_SIZE);

                for (size_t j=0; j<WF_TOTAL; ++j)
                    dm->vWaveform[j].init(max_wf_len + BUFFER_SIZE);

                for (size_t j=0; j<DM_TOTAL; ++j)
                    dm->vGraphs[j].init(meta::referencer::DYNA_MESH_SIZE, meta::referencer::DYNA_SUBSAMPLING, dmesh_period);

                dm->vGraphs[DM_CORR].set_method(dspu::MM_SIGN_MAXIMUM);

                dm->fPSRLevel            = 0.0f;
                dm->nGonioStrobe        = nGonioPeriod;
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

        void referencer::configure_filter(dspu::Equalizer *eq, bool enable)
        {
            dspu::equalizer_mode_t mode     = decode_equalizer_mode(pFltMode->value());
            const size_t post_slope         = pFltSlope->value();
            const size_t post_sel           = pFltSel->value();
            const float post_hpf            = (post_sel >= PF_BASS) ? pFltSplit[post_sel - PF_BASS]->value() : -1.0f;
            const float post_lpf            = ((post_sel >= PF_SUB_BASS) && (post_sel < PF_HIGH)) ? pFltSplit[post_sel - PF_SUB_BASS]->value() : -1.0f;

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
                    mode                = dspu::EQM_BYPASS;
                }
            }

            eq->set_params(0, &fp);
            eq->set_mode((enable) ? mode : dspu::EQM_BYPASS);
        }

        void referencer::set_loop_range(loop_t *al, ssize_t begin, ssize_t end, ssize_t limit)
        {
            const ssize_t first     = lsp_min(begin, limit);
            const ssize_t last      = lsp_min(end, limit);

            al->nStart              = lsp_min(first, last);
            al->nEnd                = lsp_max(first, last);
            if (al->nStart < al->nEnd)
                al->nPos                = lsp_limit(al->nPos, al->nStart, al->nEnd - 1);
            else
                al->nPos                = -1;
        }

        void referencer::update_playback_state()
        {
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
        }

        void referencer::update_loop_ranges()
        {
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                afile_t *af             = &vSamples[i];
                af->fGain               = af->pGain->value();
                ssize_t len             = (af->pSample != NULL) ? af->pSample->length() : 0;

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loop_t *al              = &af->vLoops[j];

                    const ssize_t l_start   = al->nStart;
                    const ssize_t l_end     = al->nEnd;
                    set_loop_range(al,
                        dspu::seconds_to_samples(fSampleRate, al->pStart->value()),
                        dspu::seconds_to_samples(fSampleRate, al->pEnd->value()),
                        len);

                    // Check if we need to syncrhonize loop mesh
                    if ((i == nPlaySample) && (j == nPlayLoop))
                    {
                        if ((al->nStart != l_start) || (al->nEnd != l_end))
                            bSyncLoopMesh           = true;
                    }
                }
            }
        }


        void referencer::update_settings()
        {
            update_playback_state();
            update_loop_ranges();

            // Enable gain matching
            const float gm_react    = 10.0f / pGainMatchReact->value();
            nGainMatching           = pGainMatching->value();
            const float gm_ksr      = (M_LN10 / 20.0f) / fSampleRate;
            fGainMatchGrow          = expf(gm_react * gm_ksr);
            fGainMatchFall          = expf(-gm_react * gm_ksr);

            // Waveform analysis
            sMix.fWaveformOff       = sMix.pFrameOffset->value();
            sRef.fWaveformOff       = sRef.pFrameOffset->value();
            fWaveformLen            = pFrameLength->value();

            // Apply filter settings
            bool pre_filter         = pFltPos->value() < 0.5f;
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                configure_filter(&c->vPreFilters[0], pre_filter);
                configure_filter(&c->vPreFilters[1], pre_filter);
                configure_filter(&c->sPostFilter, !pre_filter);
            }

            // Update dynamics analysis
            fMaxTime                = pMaxTime->value();
            const float llufs_time  = pLLUFSTime->value();
            const size_t period     = dspu::seconds_to_samples(fSampleRate, fMaxTime / float(meta::referencer::DYNA_MESH_SIZE));
            const size_t psr_period = dspu::seconds_to_samples(fSampleRate, pPsrPeriod->value());
            nPsrMode                = pPsrDisplay->value();
            const float psr_th      = dspu::gain_to_db(pPsrThreshold->value());

            nPsrThresh              = (psr_th * meta::referencer::PSR_MESH_SIZE) / (meta::referencer::PSR_MAX_LEVEL - meta::referencer::PSR_MIN_LEVEL);
            lsp_trace("psr_th = %f, nPsrThresh = %d", psr_th, int(nPsrThresh));

            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];
                for (size_t j=0; j<DM_TOTAL; ++j)
                {
                    dm->vGraphs[j].set_period(period);
                    dm->sLLUFSMeter.set_integration_period(llufs_time);
                    dm->sPSRStats.set_period(psr_period);
                }
            }

            // Apply FFT analysis settings
            const float fft_react   = pFftReactivity->value();
            const float fft_ball    = lsp_max(fft_react, pFftBallistics->value());
            const size_t fft_rank   = meta::referencer::FFT_RANK_MIN + pFftRank->value();
            const size_t fft_window = pFftWindow->value();
            const size_t fft_env    = pFftEnvelope->value();
            const size_t fft_size   = 1 << fft_rank;

            fFftTau                 = expf(logf(1.0f - M_SQRT1_2) / dspu::seconds_to_samples(meta::referencer::SPC_REFRESH_RATE, fft_react));
            fFftBal                 = expf(logf(1.0f - M_SQRT1_2) / dspu::seconds_to_samples(meta::referencer::SPC_REFRESH_RATE, fft_ball));
            bFftDamping             = pFftDamping->value() >= 0.5f;
            nFftSrc                 = pFftVMarkSrc->value();
            fFftFreq                = pFftVMarkFreq->value();
            if (nFftRank != fft_rank)
            {
                nFftRank                = fft_rank;
                nFftWindow              = -1;
                nFftEnvelope            = -1;
                bUpdFft                 = true;
            }

            // Need to reset values?
            if (pFftReset->value() >= 0.5f)
                reset_fft();

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

            bFreeze                 = pFreeze->value() >= 0.5f;

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

        void referencer::copy_waveform(float *dst, dspu::RawRingBuffer *rb, size_t offset, size_t length, size_t dst_len)
        {
            const float *src    = rb->begin();
            const size_t limit  = rb->size();

            // Compute the initial offset to start from
            offset              = (rb->position() + limit - length - offset) % limit;

            for (size_t i=0; i<dst_len; ++i)
            {
                size_t first    = (i * length) / dst_len;
                dst[i]          = src[(first + offset) % limit];
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
            status = source->resample(fSampleRate);
            if (status != STATUS_OK)
            {
                lsp_trace("resampling failed: status=%d (%s)", status, get_status(status));
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
                    af->nStatus             = af->pLoader->code();
                    af->nLength             = (af->nStatus == STATUS_OK) ? af->pSample->length() : 0;
                    af->bSync               = true;

                    // Now we can surely commit changes and reset task state
                    path->commit();
                    af->pLoader->reset();

                    // Update loop range to make not possible to go out of sample memory region
                    if (i == nPlaySample)
                        bSyncLoopMesh   = true;
                    update_playback_state();
                    update_loop_ranges();
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
                    to_process          = lsp_min(ssize_t(nCrossfadeTime) - al->nPos, to_process);
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

        void referencer::apply_pre_filters(size_t samples)
        {
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];
                c->vPreFilters[0].process(c->vInBuffer, c->vInBuffer, samples);
                c->vPreFilters[1].process(c->vBuffer, c->vBuffer, samples);
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
                    const float *src = vChannels[i].vInBuffer;

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
                    dsp::fmadd_k3(vChannels[i].vBuffer, vChannels[i].vInBuffer, sMix.fGain, samples);
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

        void referencer::reset_fft()
        {
            const size_t max_graph  = (nChannels > 1) ? FG_STEREO : FG_MONO;

            for (size_t i=0; i<2; ++i)
            {
                fft_meters_t *fm = &vFftMeters[i];

                for (size_t j=0; j<max_graph; ++j)
                {
                    fft_graph_t *fg = & fm->vGraphs[j];

                    dsp::copy(fg->vData[FT_MIN], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);
                    dsp::copy(fg->vData[FT_MAX], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);
                }
            }
        }

        void referencer::accumulate_fft(fft_meters_t *fm, size_t type, const float *buf)
        {
            fft_graph_t *fg = &fm->vGraphs[type];

            // Current value
            dsp::mix2(fg->vData[FT_CURR], buf, fFftTau, 1.0f - fFftTau, meta::referencer::SPC_MESH_SIZE);

            // Compute minimum and maximum
            if (bFftDamping)
            {
                // Minimum
                dsp::mix2(fg->vData[FT_MIN], fg->vData[FT_CURR], fFftBal, 1.0f - fFftBal, meta::referencer::SPC_MESH_SIZE);
                dsp::pmin2(fg->vData[FT_MIN], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);

                // Maximum
                dsp::mix2(fg->vData[FT_MAX], fg->vData[FT_CURR], fFftBal, 1.0f - fFftBal, meta::referencer::SPC_MESH_SIZE);
                dsp::pmax2(fg->vData[FT_MAX], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);
            }
            else
            {
                // Minimum
                dsp::pmin2(fg->vData[FT_MIN], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);
                // Maximum
                dsp::pmax2(fg->vData[FT_MAX], fg->vData[FT_CURR], meta::referencer::SPC_MESH_SIZE);
            }

            // Check if we have to report frequency meter
            switch (type)
            {
                case FG_LEFT:
                case FG_RIGHT:
                case FG_MID:
                case FG_SIDE:
                    break;
                default:
                    return;
            }

            const size_t index = (nChannels > 1) ?
                (fm - &vFftMeters[0]) * 4 + (type - FG_LEFT) :
                (fm - &vFftMeters[0]);

            if (index == nFftSrc)
            {
                const ssize_t findex = logf(fFftFreq/SPEC_FREQ_MIN) * (meta::referencer::SPC_MESH_SIZE-1) / logf(SPEC_FREQ_MAX / SPEC_FREQ_MIN);
                const float level = ((findex >= 0) && (size_t(findex) < meta::referencer::SPC_MESH_SIZE)) ?
                        fg->vData[FT_CURR][findex] * vFftEnvelope[findex] : GAIN_AMP_M_INF_DB;
                pFftVMarkVal->set_value(level);
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
                accumulate_fft(fm, FG_MID, ft1);
                accumulate_fft(fm, FG_SIDE, ft2);

                // Analyze mid/side balance between left and right channels
                dsp::depan_lin(ft1, ft1, ft2, 0.0f, meta::referencer::SPC_MESH_SIZE);
                accumulate_fft(fm, FG_MSBAL, ft1);

                // Analyze complex correlation between left and right
                dsp::pcomplex_corr(ft2, fl, fr, meta::referencer::SPC_MESH_SIZE);
                accumulate_fft(fm, FG_CORR, ft2);

                // Analyze left and right channels
                dsp::pcomplex_mod(fl, fl, meta::referencer::SPC_MESH_SIZE);
                dsp::pcomplex_mod(fr, fr, meta::referencer::SPC_MESH_SIZE);
                accumulate_fft(fm, FG_LEFT, fl);
                accumulate_fft(fm, FG_RIGHT, fr);

                // Analyze panorama between left and right channels
                dsp::depan_eqpow(ft1, fl, fr, 0.5f, meta::referencer::SPC_MESH_SIZE);
                accumulate_fft(fm, FG_PAN, ft1);
            }
            else
            {
                float *fl       = vBuffer;
                float *ft1      = &fl[fft_xsize];

                // Prepare buffers
                if (split >= fft_size)
                    dsp::mul3(fl, &fm->vHistory[0][head], &vFftWindow[0], fft_size);
                else
                {
                    dsp::mul3(fl, &fm->vHistory[0][head], &vFftWindow[0], split);
                    dsp::mul3(&fl[split], &fm->vHistory[0][0], &vFftWindow[split], fft_size - split);
                }

                // Perform FFT transform
                dsp::pcomplex_r2c(ft1, fl, fft_size);
                dsp::packed_direct_fft(ft1, ft1, nFftRank);
                reduce_cspectrum(fl, ft1);

                // Analyze channel
                dsp::pcomplex_mod(fl, fl, meta::referencer::SPC_MESH_SIZE);
                accumulate_fft(fm, FG_LEFT, fl);
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
                // Capture waveform for left and right
                dm->vWaveform[WF_LEFT].push(l, samples);
                dm->vWaveform[WF_RIGHT].push(r, samples);

                // Compute stereo panorama
                dm->sPanometer.process(b1, l, r, samples);
                dm->vGraphs[DM_PAN].process(b1, samples);

                // Compute Mid/Side balance
                dsp::lr_to_ms(b1, b2, l, r, samples);
                dm->vWaveform[WF_MID].push(b1, samples);
                dm->vWaveform[WF_SIDE].push(b2, samples);
                dm->sMsBalance.process(b1, b1, b2, samples);
                dm->vGraphs[DM_MSBAL].process(b1, samples);

                // Compute Peak values
                dsp::pamax3(b1, l, r, samples);
                dm->vGraphs[DM_PEAK].process(b1, samples);

                // Compute True Peak values
                dm->sTPMeter[0].process(b1, l, samples);
                dm->sTPMeter[1].process(b2, r, samples);
                dsp::pmax2(b1, b2, samples);
                dm->vGraphs[DM_TRUE_PEAK].process(b1, samples);

                dm->sPSRDelay.process(b1, b1, samples);

                // Compute RMS values
                dm->sRMSMeter.process(b2, const_cast<const float **>(in), samples);
                dm->vGraphs[DM_RMS].process(b2, samples);

                // Compute correlation between channels
                dm->sCorrMeter.process(b2, l, r, samples);
                dm->vGraphs[DM_CORR].process(b2, samples);

                // Compute Momentary LUFS value
                dm->sMLUFSMeter.bind(0, NULL, l, 0);
                dm->sMLUFSMeter.bind(1, NULL, r, 0);
                dm->sMLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_M_LUFS].process(b2, samples);

                // Compute Long-term LUFS value
                dm->sLLUFSMeter.bind(0, l);
                dm->sLLUFSMeter.bind(1, r);
                dm->sLLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_L_LUFS].process(b2, samples);

                // Compute Integrated LUFS value
                dm->sILUFSMeter.bind(0, l);
                dm->sILUFSMeter.bind(1, r);
                dm->sILUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_I_LUFS].process(b2, samples);

                // Compute Short-term LUFS value
                dm->sSLUFSMeter.bind(0, NULL, l, 0);
                dm->sSLUFSMeter.bind(1, NULL, r, 0);
                dm->sSLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_S_LUFS].process(b2, samples);
            }
            else
            {
                // Capture waveform
                dm->vWaveform[WF_LEFT].push(l, samples);

                // Compute True Peak values
                dm->sTPMeter[0].process(b1, l, samples);
                dm->vGraphs[DM_TRUE_PEAK].process(b1, samples);

                // Compute Peak values
                dsp::abs2(b1, l, samples);
                dm->vGraphs[DM_PEAK].process(b1, samples);
                dm->sPSRDelay.process(b1, b1, samples);

                // Compute RMS values
                dm->sRMSMeter.process(b2, const_cast<const float **>(in), samples);
                dm->vGraphs[DM_RMS].process(b2, samples);

                // Compute Momentary LUFS value
                dm->sMLUFSMeter.bind(0, NULL, l, 0);
                dm->sMLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_M_LUFS].process(b2, samples);

                // Compute Long-term LUFS value
                dm->sLLUFSMeter.bind(0, l);
                dm->sLLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_L_LUFS].process(b2, samples);

                // Compute Integrated LUFS value
                dm->sILUFSMeter.bind(0, l);
                dm->sILUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_I_LUFS].process(b2, samples);

                // Compute Short-term LUFS value
                dm->sSLUFSMeter.bind(0, NULL, l, 0);
                dm->sSLUFSMeter.process(b2, samples, dspu::bs::DBFS_TO_LUFS_SHIFT_GAIN);
                dm->vGraphs[DM_S_LUFS].process(b2, samples);
            }

            // Now b1 contains Sample Peak value and b2 contains short-term LUFS value
            // Compute the PSR value as 'Peak / Short-Term LUFS' as defined in AES 143 EB 373:
            // "We propose that the PSR of an audio track be
            //  defined as the real­time difference between the
            //  Sample Peak level of the audio (measured in dBFS
            //  with an instant rise and 0.5 dB/s decay) and the
            //  Short­-term loudness as defined in EBU Tech Doc
            //  3341[7], with negative values clamped to 0".
            //
            // "By selecting Sample Peak for PSR instead of True
            //  Peak, the meter offers an intuitive indication of the
            //  micro­dynamics of the audio. The intention of PSR
            //  is to provide a minimum, “worst case” metric for
            //  micro­dynamics, and Sample Peak works best for
            //  this purpose. In contrast, the use of True Peak values
            //  when calculating PSR would result in larger values
            //  in some circumstances that reflect intersample
            //  peaks[6] caused by increased distortion rather than
            //  genuine musical micro­dynamics. Even when the
            //  PLR value seems healthy, low PSR values indicate
            //  that the audio is more “crushed” (i.e. that the
            //  short-­term loudness has been pushed closer to the
            //  Sample Peak value, often by limiting or clipping)".
            for (size_t i=0; i<samples; ++i)
            {
                const float peak    = lsp_max(double(b1[i]), dm->fPSRLevel * fPSRDecay);
                const float lufs    = b2[i];

                const float psr     = (lufs >= GAIN_AMP_M_72_DB) ? peak / lufs : GAIN_AMP_M_3_DB;
                const float psr_db  = dspu::gain_to_db(lsp_max(psr, 0.0f));

                b1[i]               = psr;
                b2[i]               = psr_db;
                dm->fPSRLevel       = peak;
            }

            dm->vGraphs[DM_PSR].process(b1, samples);
            dm->sPSRStats.process(b2, samples);
        }

        void referencer::process_goniometer(
            dyna_meters_t *dm,
            const float *l, const float *r,
            size_t samples)
        {
            // Check that stream is present
            if (dm->pGoniometer == NULL)
                return;
            plug::stream_t *stream = dm->pGoniometer->buffer<plug::stream_t>();
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
                    if (dm->nGonioStrobe == 0)
                    {
                        mid[i]                  = 1.0f;
                        dm->nGonioStrobe        = nGonioPeriod;
                    }

                    const size_t advance    = lsp_min(count - i, dm->nGonioStrobe);
                    dm->nGonioStrobe       -= advance;
                    i                      += advance;
                }
                stream->write_frame(0, mid, 0, count);

                // Perform analysis of the first pair
                dsp::lr_to_ms(mid, side, &l[offset], &r[offset], count);
                stream->write_frame(1, side, 0, count);
                stream->write_frame(2, mid, 0, count);

                // Commit frame
                stream->commit_frame();
                offset         += count;
            }
        }

        void referencer::apply_gain_matching(size_t samples)
        {
            dyna_meters_t *src_dm   = &vDynaMeters[0];
            dyna_meters_t *dst_dm   = &vDynaMeters[1];

            // First, measure automatic gain for both Mix and reference signals
            if (nChannels > 1)
            {
                src_dm->sAutogainMeter.bind(0, NULL, vChannels[0].vIn, 0);
                src_dm->sAutogainMeter.bind(1, NULL, vChannels[1].vIn, 0);
                src_dm->sAutogainMeter.process(src_dm->vLoudness, samples);

                dst_dm->sAutogainMeter.bind(0, NULL, vChannels[0].vBuffer, 0);
                dst_dm->sAutogainMeter.bind(1, NULL, vChannels[1].vBuffer, 0);
                dst_dm->sAutogainMeter.process(dst_dm->vLoudness, samples);
            }
            else
            {
                src_dm->sAutogainMeter.bind(0, NULL, vChannels[0].vIn, 0);
                src_dm->sAutogainMeter.process(src_dm->vLoudness, samples);

                dst_dm->sAutogainMeter.bind(0, NULL, vChannels[0].vBuffer, 0);
                dst_dm->sAutogainMeter.process(dst_dm->vLoudness, samples);
            }

            // Now compute gain correction
            if (nGainMatching == MATCH_MIX)
                lsp::swap(src_dm, dst_dm);

            float src_gain      = src_dm->fGain;
            float dst_gain      = dst_dm->fGain;
            float *src          = src_dm->vLoudness;
            float *dst          = dst_dm->vLoudness;

            if (nGainMatching == MATCH_NONE)
            {
                for (size_t i=0; i<samples; ++i)
                {
                    src_gain            = (src_gain > GAIN_AMP_0_DB) ? lsp_max(src_gain * fGainMatchFall, GAIN_AMP_0_DB) : lsp_min(src_gain * fGainMatchGrow, GAIN_AMP_0_DB);
                    dst_gain            = (dst_gain > GAIN_AMP_0_DB) ? lsp_max(dst_gain * fGainMatchFall, GAIN_AMP_0_DB) : lsp_min(dst_gain * fGainMatchGrow, GAIN_AMP_0_DB);

                    // Store values to resulting arrays
                    src[i]              = src_gain;
                    dst[i]              = dst_gain;
                }
            }
            else
            {
                for (size_t i=0; i<samples; ++i)
                {
                    // Normalize source gain if needed
                    src_gain            = (src_gain > GAIN_AMP_0_DB) ? lsp_max(src_gain * fGainMatchFall, GAIN_AMP_0_DB) : lsp_min(src_gain * fGainMatchGrow, GAIN_AMP_0_DB);

                    // Compute destination gain
                    if (dst[i] >= GAIN_AMP_M_60_DB)
                    {
                        const float src_loud= src[i] * src_gain;
                        const float dst_loud= dst[i] * dst_gain;
                        dst_gain            = (dst_loud > src_loud) ? dst_gain * fGainMatchFall : dst_gain * fGainMatchGrow;
                    }
                    else
                        dst_gain            = lsp_min(dst_gain * fGainMatchGrow, GAIN_AMP_0_DB);

                    // Store values to resulting arrays
                    src[i]              = src_gain;
                    dst[i]              = dst_gain;
                }
            }

            // Store new values
            src_dm->fGain       = src_gain;
            dst_dm->fGain       = dst_gain;

            // Apply gain correction to buffers
            src_dm              = &vDynaMeters[0];
            dst_dm              = &vDynaMeters[1];

            if (nChannels > 1)
            {
                dsp::mul3(vChannels[0].vInBuffer, vChannels[0].vIn, src_dm->vLoudness, samples);
                dsp::mul3(vChannels[1].vInBuffer, vChannels[1].vIn, src_dm->vLoudness, samples);
                dsp::mul2(vChannels[0].vBuffer, dst_dm->vLoudness, samples);
                dsp::mul2(vChannels[1].vBuffer, dst_dm->vLoudness, samples);
            }
            else
            {
                dsp::mul3(vChannels[0].vInBuffer, vChannels[0].vIn, src_dm->vLoudness, samples);
                dsp::mul2(vChannels[0].vBuffer, dst_dm->vLoudness, samples);
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
                apply_gain_matching(to_process);
                apply_pre_filters(to_process);

                // Measure input and reference signal parameters
                if (!bFreeze)
                {
                    perform_metering(
                        &vDynaMeters[0],
                        vChannels[0].vInBuffer,
                        (nChannels > 1) ? vChannels[1].vInBuffer : NULL,
                        to_process);
                    if (nChannels > 1)
                        process_goniometer(
                            &vDynaMeters[0],
                            vChannels[0].vInBuffer, vChannels[1].vInBuffer,
                            to_process);
                    perform_fft_analysis(
                        &vFftMeters[0],
                        vChannels[0].vInBuffer,
                        (nChannels > 1) ? vChannels[1].vInBuffer : NULL,
                        to_process);

                    perform_metering(
                        &vDynaMeters[1],
                        vChannels[0].vBuffer,
                        (nChannels > 1) ? vChannels[1].vBuffer : NULL,
                        to_process);
                    if (nChannels > 1)
                        process_goniometer(
                            &vDynaMeters[1],
                            vChannels[0].vBuffer, vChannels[1].vBuffer,
                            to_process);
                    perform_fft_analysis(
                        &vFftMeters[1],
                        vChannels[0].vBuffer,
                        (nChannels > 1) ? vChannels[1].vBuffer : NULL,
                        to_process);
                }

                mix_channels(to_process);
                apply_post_filters(to_process);

                if (nChannels > 1)
                    apply_stereo_mode(to_process);

                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c = &vChannels[i];

                    c->sBypass.process(c->vOut, c->vIn, c->vBuffer, to_process);

                    c->vIn             += to_process;
                    c->vOut            += to_process;
                }

                offset             += to_process;
            }

            output_file_data();
            output_loop_data();
            output_waveform_meshes();
            output_dyna_meters();
            output_dyna_meshes();
            output_psr_mesh();
            for (size_t i=0; i<FT_TOTAL; ++i)
                output_spectrum_analysis(i);
        }

        void referencer::output_dyna_meters()
        {
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                // Report meter values
                for (size_t i=0; i<DM_STEREO; ++i)
                {
                    if (dm->pMeters[i] != NULL)
                    {
                        const float value       = dm->vGraphs[i].level();
                        dm->pMeters[i]->set_value(value);
                    }
                }

                // Report the PSR percentage value
                if (dm->pPsrPcValue != NULL)
                {
                    // Compute the amount of PRS values above the threshold
                    const float psr_total       = dm->sPSRStats.count();
                    const uint32_t *psr_values  = dm->sPSRStats.counters();
                    size_t psr_above            = dm->sPSRStats.above();
                    for (size_t k=nPsrThresh; k < meta::referencer::PSR_MESH_SIZE; ++k)
                        psr_above                  += psr_values[k];

                    const float psr_pc          = (psr_above * 100.0f) / psr_total;
                    dm->pPsrPcValue->set_value(psr_pc);
                }
            }
        }

        void referencer::output_psr_mesh()
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pPsrMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Form the levels
            size_t rows = 0;
            float *t    = mesh->pvData[rows++];

            dsp::copy(&t[2], vPsrLevels, meta::referencer::PSR_MESH_SIZE);
            t[0]        = meta::referencer::PSR_MIN_LEVEL * 0.5f;
            t[1]        = t[0];
            t          += meta::referencer::PSR_MESH_SIZE + 2;
            t[0]        = meta::referencer::PSR_MAX_LEVEL * 2.0f;
            t[1]        = t[0];

            // Append meshes
            for (size_t i=0; i < 2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];
                dspu::QuantizedCounter *qc = &dm->sPSRStats;

                size_t count            = dm->sPSRStats.count();
                float *t                = mesh->pvData[rows++];

                if (count > 0)
                {
                    size_t below            = qc->below();
                    size_t above            = qc->above();
                    const uint32_t *c       = dm->sPSRStats.counters();

                    if (nPsrMode == PSR_DENSITY)
                    {
                        const float norm        = 100.0f / count;

                        *(t++)                  = 0;
                        *(t++)                  = count * norm;
                        count                  -= below;

                        for (size_t j=0; j<meta::referencer::PSR_MESH_SIZE; ++j)
                        {
                            *(t++)                  = count * norm;
                            count                  -= c[j];
                        }

                        *(t++)                  = count * norm;
                        *(t++)                  = 0;
                    }
                    else if (nPsrMode == PSR_FREQUENCY)
                    {
                        const float norm        = 100.0f / count;

                        *(t++)                  = 0;
                        *(t++)                  = below * norm;

                        for (size_t j=0; j<meta::referencer::PSR_MESH_SIZE; ++j)
                            *(t++)                  = c[j] * norm;

                        *(t++)                  = above * norm;
                        *(t++)                  = 0;
                    }
                    else // PSR_NORMALIZED
                    {
                        // Compute norming factor
                        size_t max              = lsp_max(below, above);
                        for (size_t j=0; j<meta::referencer::PSR_MESH_SIZE; ++j)
                            max                     = lsp_max(max, c[j]);

                        const float norm        = 100.0f / max;

                        // Apply changes
                        *(t++)                  = 0;
                        *(t++)                  = below * norm;

                        for (size_t j=0; j<meta::referencer::PSR_MESH_SIZE; ++j)
                            *(t++)                  = c[j] * norm;

                        *(t++)                  = above * norm;
                        *(t++)                  = 0;
                    }
                }
                else
                    dsp::fill_zero(t, meta::referencer::PSR_MESH_SIZE + 4);
            }

            // Commit data to mesh
            mesh->data(rows, meta::referencer::PSR_MESH_SIZE + 4);
        }

        void referencer::output_waveform_meshes()
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pWaveformMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()) )
                return;

            // Generate timestamp
            size_t rows = 0;

            // Time
            float *t    = mesh->pvData[rows++];
            dsp::lramp_set1(&t[2], fWaveformLen, 0.0f, meta::referencer::WAVE_MESH_SIZE);
            t[0]    = fWaveformLen * 1.25f;
            t[1]    = t[0];
            t      += meta::referencer::WAVE_MESH_SIZE + 2;
            t[0]    = -0.25f * fWaveformLen;
            t[1]    = t[0];

            const size_t frame_len      = dspu::seconds_to_samples(fSampleRate, fWaveformLen);

            // Copy contents of all graphs
            const size_t max_graph  = (nChannels > 1) ? WF_STEREO : WF_MONO;
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                const float wave_off    = (i == 0) ? sMix.fWaveformOff : sRef.fWaveformOff;
                const size_t frame_off  = dspu::seconds_to_samples(fSampleRate, wave_off);

                for (size_t j=0; j<max_graph; ++j)
                {
                    dspu::RawRingBuffer *rb = &dm->vWaveform[j];

                    t       = mesh->pvData[rows++];

                    copy_waveform(&t[2], rb, frame_off, frame_len, meta::referencer::WAVE_MESH_SIZE);

                    t[0]    = 0.0f;
                    t[1]    = t[2];
                    t      += meta::referencer::WAVE_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = 0.0f;
                }
            }

            // Commit data to mesh
            mesh->data(rows, meta::referencer::WAVE_MESH_SIZE + 4);
        }

        void referencer::output_dyna_meshes()
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pDynaMesh->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            // Generate timestamp
            size_t rows = 0;

            // Time
            float *t    = mesh->pvData[rows++];
            dsp::lramp_set1(&t[2], fMaxTime, 0.0f, meta::referencer::DYNA_MESH_SIZE);
            t[0]    = meta::referencer::DYNA_TIME_MAX + 0.5f;
            t[1]    = t[0];
            t      += meta::referencer::DYNA_MESH_SIZE + 2;
            t[0]    = -0.5f;
            t[1]    = t[0];

            // Copy contents of all graphs
            const size_t max_graph  = (nChannels > 1) ? DM_STEREO : DM_MONO;
            for (size_t i=0; i<2; ++i)
            {
                dyna_meters_t *dm       = &vDynaMeters[i];

                for (size_t j=0; j<max_graph; ++j)
                {
                    dspu::ScaledMeterGraph *mg    = &dm->vGraphs[j];

                    t       = mesh->pvData[rows++];
                    mg->read(&t[2], meta::referencer::DYNA_MESH_SIZE);

                    t[0]    = dm_endpoints[j];
                    t[1]    = t[2];
                    t      += meta::referencer::DYNA_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = dm_endpoints[j];
                }
            }

            // Commit data to mesh
            mesh->data(rows, meta::referencer::DYNA_MESH_SIZE + 4);
        }

        void referencer::output_spectrum_analysis(size_t type)
        {
            // Check that mesh is ready for receiving data
            plug::mesh_t *mesh  = reinterpret_cast<plug::mesh_t *>(pFftMesh[type]->buffer());
            if ((mesh == NULL) || (!mesh->isEmpty()))
                return;

            size_t rows = 0;

            // Frequencies
            float *t =  mesh->pvData[rows++];
            dsp::copy(&t[2], vFftFreqs, meta::referencer::SPC_MESH_SIZE);
            t[0]    = SPEC_FREQ_MIN * 0.25f;
            t[1]    = SPEC_FREQ_MIN * 0.5f;
            t      += meta::referencer::SPC_MESH_SIZE + 2;
            t[0]    = SPEC_FREQ_MAX * 2.0f;
            t[1]    = SPEC_FREQ_MAX * 3.0f;

            const size_t max_graph  = (nChannels > 1) ? FG_STEREO : FG_MONO;

            for (size_t i=0; i<2; ++i)
            {
                fft_meters_t *fm  = &vFftMeters[i];

                for (size_t j=0; j<max_graph; ++j)
                {
                    fft_graph_t *fg = & fm->vGraphs[j];
                    const float dfl = fft_endpoints[j * FT_TOTAL + type];
                    t               =  mesh->pvData[rows++];

                    if ((j >= FG_LEFT) && (j <= FG_SIDE))
                        dsp::mul3(&t[2], fg->vData[type], vFftEnvelope, meta::referencer::SPC_MESH_SIZE);
                    else
                        dsp::copy(&t[2], fg->vData[type], meta::referencer::SPC_MESH_SIZE);

                    t[0]    = dfl;
                    t[1]    = t[2];
                    t      += meta::referencer::SPC_MESH_SIZE + 2;
                    t[0]    = t[-1];
                    t[1]    = dfl;
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

        void referencer::dump_channels(dspu::IStateDumper *v) const
        {
            v->begin_array("vChannels", vChannels, nChannels);
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sBypass", &c->sBypass);
                    v->write_object_array("vPreFilters", c->vPreFilters, 2);
                    v->write_object("sPostFilter", &c->sPostFilter);

                    v->write("vIn", c->vIn);
                    v->write("vOut", c->vOut);
                    v->write("vBuffer", c->vBuffer);
                    v->write("vInBuffer", c->vInBuffer);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);
                }
                v->end_object();
            }
            v->end_array();
        }

        void referencer::dump_asource(dspu::IStateDumper *v, const char *name, const asource_t *as) const
        {
            v->begin_object(name, as, sizeof(asource_t));

            v->write("fGain", as->fGain);
            v->write("fOldGain", as->fOldGain);
            v->write("fNewGain", as->fNewGain);
            v->write("nTransition", as->nTransition);
            v->write("fWaveformOff", as->fWaveformOff);
            v->write("pFrameOffset", as->pFrameOffset);

            v->end_object();
        }

        void referencer::dump_dyna_meters(dspu::IStateDumper *v) const
        {
            v->begin_array("vDynaMeters", vDynaMeters, 2);
            for (size_t i=0; i<2; ++i)
            {
                const dyna_meters_t *dm     = &vDynaMeters[i];

                v->begin_object(dm, sizeof(dyna_meters_t));
                {
                    v->write_object("sRMSMeter", &dm->sRMSMeter);
                    v->write_object_array("sTPMeter", dm->sTPMeter, 2);
                    v->write_object("sPSRDelay", &dm->sPSRDelay);
                    v->write_object("sAutogainMeter", &dm->sAutogainMeter);
                    v->write_object("sMLUFSMeter", &dm->sMLUFSMeter);
                    v->write_object("sSLUFSMeter", &dm->sSLUFSMeter);
                    v->write_object("sLLUFSMeter", &dm->sLLUFSMeter);
                    v->write_object("sILUFSMeter", &dm->sILUFSMeter);
                    v->write_object("sCorrMeter", &dm->sCorrMeter);
                    v->write_object("sPanometer", &dm->sPanometer);
                    v->write_object("sMsBalance", &dm->sMsBalance);
                    v->write_object("sPSRStats", &dm->sPSRStats);
                    v->write_object_array("vWaveform", dm->vWaveform, WF_TOTAL);
                    v->write_object_array("vGraphs", dm->vGraphs, DM_TOTAL);

                    v->write("vLoudness", dm->vLoudness);
                    v->write("fGain", dm->fGain);
                    v->write("fPSRLevel", dm->fPSRLevel);
                    v->write("nGonioStrobe", dm->nGonioStrobe);

                    v->writev("pMeters", dm->pMeters, DM_TOTAL);
                    v->write("pGoniometer", dm->pGoniometer);
                    v->write("pPsrPcValue", dm->pPsrPcValue);
                }
                v->end_object();
            }
            v->end_array();
        }

        void referencer::dump_fft_meters(dspu::IStateDumper *v) const
        {
            v->begin_array("vFftMeters", vFftMeters, 2);
            for (size_t i=0; i<2; ++i)
            {
                const fft_meters_t *fm  = &vFftMeters[i];

                v->begin_object(fm, sizeof(fft_meters_t));
                {
                    v->writev("vHistory", fm->vHistory, 2);
                    v->write("nFftPeriod", fm->nFftPeriod);
                    v->write("nFftFrame", fm->nFftFrame);
                    v->write("nFftHistory", fm->nFftHistory);

                    v->begin_array("vGraphs", fm->vGraphs, FG_TOTAL);
                    for (size_t j=0; j<2; ++j)
                    {
                        const fft_graph_t *fg   = &fm->vGraphs[j];

                        v->begin_object(fg, sizeof(fft_graph_t));
                        {
                            v->writev("vData", fg->vData, FT_TOTAL);
                        }
                        v->end_object();
                    }
                    v->end_array();
                }
                v->end_object();
            }
            v->end_array();
        }

        void referencer::dump(dspu::IStateDumper *v) const
        {
            plug::Module::dump(v);

            v->write("nChannels", nChannels);
            v->write("nPlaySample", nPlaySample);
            v->write("nPlayLoop", nPlayLoop);
            v->write("nGainMatching", nGainMatching);
            v->write("fGainMatchGrow", fGainMatchGrow);
            v->write("fGainMatchFall", fGainMatchFall);
            v->write("nCrossfadeTime", nCrossfadeTime);
            v->write("fMaxTime", fMaxTime);
            v->write("enMode", enMode);
            v->write("fWaveformLen", fWaveformLen);
            v->write("nFftRank", nFftRank);
            v->write("nFftWindow", nFftWindow);
            v->write("nFftEnvelope", nFftEnvelope);
            v->write("fFftTau", fFftTau);
            v->write("fFftBal", fFftBal);
            v->write("nFftSrc", nFftSrc);
            v->write("nGonioPeriod", nGonioPeriod);
            v->write("nPsrMode", nPsrMode);
            v->write("nPsrThresh", nPsrThresh);
            v->write("fPSRDecay", fPSRDecay);
            v->write("bPlay", bPlay);
            v->write("bSyncLoopMesh", bSyncLoopMesh);
            v->write("bUpdFft", bUpdFft);
            v->write("bFftDamping", bFftDamping);
            v->write("bFreeze", bFreeze);

            v->write("vBuffer", vBuffer);
            v->write("vFftFreqs", vFftFreqs);
            v->write("vFftInds", vFftInds);
            v->write("vFftWindow", vFftWindow);
            v->write("vFftEnvelope", vFftEnvelope);
            v->write("vPsrLevels", vPsrLevels);

            dump_channels(v);
            dump_asource(v, "sMix", &sMix);
            v->write("pExecutor", pExecutor);
            dump_dyna_meters(v);
            dump_fft_meters(v);

            v->write("pBypass", pBypass);
            v->write("pFreeze", pFreeze);
            v->write("pPlay", pPlay);
            v->write("pPlayLoop", pPlayLoop);
            v->write("pSource", pSource);
            v->write("pLoopMesh", pLoopMesh);
            v->write("pLoopLen", pLoopLen);
            v->write("pLoopPos", pLoopPos);
            v->write("pGainMatching", pGainMatching);
            v->write("pGainMatchReact", pGainMatchReact);
            v->write("pMode", pMode);
            v->write("pFltPos", pFltPos);
            v->write("pFltMode", pFltMode);
            v->write("pFltSel", pFltSel);
            v->writev("pFltSplit", pFltSplit, meta::referencer::FLT_SPLITS);
            v->write("pMaxTime", pMaxTime);
            v->write("pLLUFSTime", pLLUFSTime);
            v->write("pDynaMesh", pDynaMesh);
            v->write("pWaveformMesh", pWaveformMesh);
            v->write("pFrameLength", pFrameLength);
            v->write("pFftRank", pFftRank);
            v->write("pFftWindow", pFftWindow);
            v->write("pFftEnvelope", pFftEnvelope);
            v->write("pFftReactivity", pFftReactivity);
            v->write("pFftDamping", pFftDamping);
            v->write("pFftReset", pFftReset);
            v->write("pFftBallistics", pFftBallistics);
            v->writev("pFftMesh", pFftMesh, 3);
            v->write("pFftVMarkSrc", pFftVMarkSrc);
            v->write("pFftVMarkFreq", pFftVMarkFreq);
            v->write("pFftVMarkVal", pFftVMarkVal);
            v->write("pPsrPeriod", pPsrPeriod);
            v->write("pPsrThreshold", pPsrThreshold);
            v->write("pPsrMesh", pPsrMesh);
            v->write("pPsrDisplay", pPsrDisplay);

            v->write("pData", pData);
        }

    } /* namespace plugins */
} /* namespace lsp */


