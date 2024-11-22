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

#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/ui.h>

#include <private/meta/referencer.h>
#include <private/ui/referencer.h>

namespace lsp
{
    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::referencer_mono,
            &meta::referencer_stereo
        };

        static ui::Module *ui_factory(const meta::plugin_t *meta)
        {
            return new referencer_ui(meta);
        }

        static ui::Factory factory(ui_factory, plugin_uis, 2);

        //---------------------------------------------------------------------
        referencer_ui::referencer_ui(const meta::plugin_t *meta):
            ui::Module(meta)
        {
            play_matrix_t *pm   = &sPlayMatrix;

            pm->pPlayLoop       = NULL;
            pm->pPlaySample     = NULL;

            waveform_t *wf      = &sWaveform;

            wf->pLogScale       = NULL;
            wf->pLinMax         = NULL;
            wf->pLogMin         = NULL;
            wf->pLogMax         = NULL;

            wf->fScaleMin       = 0.0f;
            wf->fScaleMax       = 0.0f;
            wf->bLogScale       = false;
            wf->bEditing        = false;
        }

        referencer_ui::~referencer_ui()
        {
        }

        ui::IPort *referencer_ui::bind_port(const char *id)
        {
            ui::IPort *p = pWrapper->port(id);
            if (p != NULL)
                p->bind(this);
            return p;
        }

        status_t referencer_ui::init_waveform_graphs()
        {
            static const char * const graph_ids[] =
            {
                "waveform_ref",
                "waveform_ref_l",
                "waveform_ref_r",
                "waveform_ref_m",
                "waveform_ref_s",
                "waveform_mix",
                "waveform_mix_l",
                "waveform_mix_r",
                "waveform_mix_m",
                "waveform_mix_s",
                NULL
            };

            sWaveform.pLogScale         = bind_port("wflog");
            sWaveform.pLogMin           = bind_port("wfscmin");
            sWaveform.pLogMax           = bind_port("wfscmax");

            for (const char * const *uid = graph_ids; *uid != NULL; ++uid)
            {
                tk::GraphMesh *mesh = pWrapper->controller()->widgets()->get<tk::GraphMesh>(*uid);
                if (mesh == NULL)
                    continue;

                if (!sWaveform.vMeshes.add(mesh))
                    return STATUS_NO_MEM;

                mesh->set_transform(waveform_transform_func, this);
            }

            return STATUS_OK;
        }

        status_t referencer_ui::init_playback_matrix()
        {
//            vWaveformGraphs;
            sPlayMatrix.pPlaySample     = bind_port("pssel");
            sPlayMatrix.pPlayLoop       = bind_port("plsel");

            if ((sPlayMatrix.pPlaySample != NULL) && (sPlayMatrix.pPlayLoop != NULL))
            {
                LSPString id;
                for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
                {
                    for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                    {
                        tk::Button *btn = NULL;

                        if (id.fmt_ascii("play_matrix_%d_%d", int(i + 1), int(j + 1)) > 0)
                            btn             = pWrapper->controller()->widgets()->get<tk::Button>(&id);

                        if (!sPlayMatrix.vButtons.add(btn))
                            return STATUS_NO_MEM;

                        if (btn != NULL)
                            btn->slots()->bind(tk::SLOT_CHANGE, slot_matrix_change, this);
                    }
                }
            }

            return STATUS_OK;
        }

        status_t referencer_ui::post_init()
        {
            // Initialize parent class
            status_t res = ui::Module::post_init();
            if (res != STATUS_OK)
                return res;

            LSP_STATUS_ASSERT(init_playback_matrix());
            LSP_STATUS_ASSERT(init_waveform_graphs());

            // Synchronize state of the matrix
            sync_matrix_state(NULL);
            sync_waveform_state(NULL, 0);

            return STATUS_OK;
        }

        void referencer_ui::notify(ui::IPort *port, size_t flags)
        {
            if (port == NULL)
                return;

            sync_matrix_state(port);
            sync_waveform_state(port, flags);
        }

        void referencer_ui::sync_matrix_state(ui::IPort *port)
        {
            if (port != NULL)
            {
                if ((port != sPlayMatrix.pPlayLoop) && (port != sPlayMatrix.pPlaySample))
                    return;
            }

            const ssize_t sample    = (sPlayMatrix.pPlaySample != NULL) ? sPlayMatrix.pPlaySample->value() - 1 : -1;
            const ssize_t loop      = (sPlayMatrix.pPlayLoop != NULL) ? sPlayMatrix.pPlayLoop->value() - 1 : -1;
            const ssize_t active    = sample * meta::referencer::AUDIO_SAMPLES + loop;

            for (size_t i=0, n=sPlayMatrix.vButtons.size(); i<n; ++i)
            {
                tk::Button *btn = sPlayMatrix.vButtons.uget(i);
                if (btn == NULL)
                    continue;

                btn->down()->set(ssize_t(i) == active);
            }
        }

        void referencer_ui::sync_waveform_state(ui::IPort *port, size_t flags)
        {
            waveform_t *wf      = &sWaveform;
            if (wf->bEditing)
                return;

            // Set-up edit flag
            wf->bEditing        = true;
            lsp_finally {
                wf->bEditing        = false;
            };

            size_t changes      = 0;

            // Sync logarithmic scale
            if ((port == NULL) || (port == wf->pLogScale))
            {
                const bool log          = (wf->pLogScale != NULL) ? wf->pLogScale->value() >= 0.5f : false;
                if (wf->bLogScale != log)
                {
                    wf->bLogScale     = log;
                    ++changes;
                }
            }

            // Synchronize minimum and maximum scales
            if ((port == NULL) || (port == wf->pLogMin) || (port == wf->pLogMax))
            {
                float log_min           = (wf->pLogMin != NULL) ? wf->pLogMin->value() : meta::referencer::WAVE_SMIN_SCALE_DFL;
                float log_max           = (wf->pLogMax != NULL) ? wf->pLogMax->value() : meta::referencer::WAVE_SMAX_SCALE_DFL;
                float delta             = log_max - log_min;

                lsp_trace("log_min = %f, log_max = %f, delta = %f",
                    log_min, log_max, delta);

                if ((flags & ui::PORT_USER_EDIT) && (delta < meta::referencer::WAVE_SRANGE_DIFF_MIN))
                {
                    if (port == wf->pLogMin)
                    {
                        log_max             = log_min + meta::referencer::WAVE_SRANGE_DIFF_MIN;
                        lsp_trace("new log_max = %f", log_max);

                        if (wf->pLogMax != NULL)
                        {
                            wf->pLogMax->set_value(log_max);
                            wf->pLogMax->notify_all(ui::PORT_USER_EDIT);
                        }
                    }
                    else
                    {
                        log_min             = log_max - meta::referencer::WAVE_SRANGE_DIFF_MIN;
                        lsp_trace("new log_min = %f", log_min);

                        if (wf->pLogMin != NULL)
                        {
                            wf->pLogMin->set_value(log_min);
                            wf->pLogMin->notify_all(ui::PORT_USER_EDIT);
                        }
                    }
                }

                if ((log_min != wf->fScaleMin) || (log_max != wf->fScaleMax))
                {
                    wf->fScaleMin       = log_min;
                    wf->fScaleMax       = log_max;
                    ++changes;
                }
            }

            if (changes > 0)
            {
                for (size_t i=0, n=sWaveform.vMeshes.size(); i<n; ++i)
                {
                    tk::GraphMesh *gm = sWaveform.vMeshes.uget(i);
                    if (gm != NULL)
                        gm->query_draw();
                }
            }
        }

        bool referencer_ui::waveform_transform_func(float *dst, const float *src, size_t count, tk::GraphMesh::coord_t coord, void *data)
        {
            if (coord != tk::GraphMesh::COORD_Y)
                return false;

            referencer_ui *self = reinterpret_cast<referencer_ui *>(data);
            if (self == NULL)
                return false;

            waveform_t *wf = &self->sWaveform;

            const float gmax    = dspu::db_to_gain(wf->fScaleMax);

            if (wf->bLogScale)
            {
                const float gmin    = dspu::db_to_gain(wf->fScaleMin);
                const float norm    = 1.0f / logf(gmax / gmin);
                const float mul     = 1.0f / gmin;

                for (size_t i=0; i<count; ++i)
                {
                    const float sign = (src[i] >= 0.0f) ? 1.0f : -1.0f;
                    const float s   = fabsf(src[i]);

                    if (s >= gmin)
                        dst[i]      = sign * norm * logf(mul * s);
                    else
                        dst[i]      = 0.0f;
                }
            }
            else
                dsp::mul_k3(dst, src, 1.0f / gmax, count);

            return true;
        }

        status_t referencer_ui::on_matrix_change(tk::Button *btn)
        {
            if (sPlayMatrix.pPlaySample == NULL)
                return STATUS_OK;
            if (sPlayMatrix.pPlayLoop == NULL)
                return STATUS_OK;

            const ssize_t index     = sPlayMatrix.vButtons.index_of(btn);
            if (index < 0)
                return STATUS_OK;

            const ssize_t sample    = index / meta::referencer::AUDIO_SAMPLES + 1;
            const ssize_t loop      = index % meta::referencer::AUDIO_SAMPLES + 1;

            sPlayMatrix.pPlaySample->set_value(sample);
            sPlayMatrix.pPlayLoop->set_value(loop);
            sPlayMatrix.pPlaySample->notify_all(ui::PORT_USER_EDIT);
            sPlayMatrix.pPlayLoop->notify_all(ui::PORT_USER_EDIT);

            return STATUS_OK;
        }

        status_t referencer_ui::slot_matrix_change(tk::Widget *sender, void *ptr, void *data)
        {
            tk::Button *btn = tk::widget_cast<tk::Button>(sender);
            if (btn == NULL)
                return STATUS_OK;

            referencer_ui *self = static_cast<referencer_ui *>(ptr);
            return (self != NULL) ? self->on_matrix_change(btn) : STATUS_OK;
        }


    } /* namespace plugins */
} /* namespace lsp */


