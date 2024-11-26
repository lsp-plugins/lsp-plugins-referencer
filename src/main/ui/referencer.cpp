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
#include <lsp-plug.in/stdlib/locale.h>

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
        static const char *note_names[] =
        {
            "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"
        };

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

            fft_meters_t *fm    = &sFftMeters;
            fm->pHorLevel       = NULL;
            fm->pVerFreq        = NULL;
            fm->pVerMeter       = NULL;
            fm->wHorText        = NULL;
            fm->wVerText        = NULL;

            bStereo             = (strcmp(meta->uid, meta::referencer_stereo.uid) == 0);
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

        ui::IPort *referencer_ui::bind_port(const LSPString *id)
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
            LSPString id;

            sPlayMatrix.pPlaySample     = bind_port("pssel");
            sPlayMatrix.pPlayLoop       = bind_port("plsel");
            sPlayMatrix.pTabSel         = bind_port("section");
            sPlayMatrix.pSampleSel      = bind_port("ssel");

            if ((sPlayMatrix.pPlaySample != NULL) && (sPlayMatrix.pPlayLoop != NULL))
            {
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

            for (size_t i=0; i < meta::referencer::AUDIO_SAMPLES; ++i)
            {
                sample_loader_t *loader = &sPlayMatrix.vLoaders[i];

                loader->pStatus         = NULL;
                loader->pLoopSel        = NULL;
                loader->pFileName       = NULL;
                loader->wView           = NULL;
                loader->wEditor         = NULL;

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    loader->vLoop[j].pStart     = NULL;
                    loader->vLoop[j].pEnd       = NULL;
                }

                if (id.fmt_ascii("loop_view%d", int(i + 1)) > 0)
                {
                    loader->wView           = pWrapper->controller()->widgets()->get<tk::AudioSample>(&id);
                    if (loader->wView != NULL)
                        loader->wView->slots()->bind(tk::SLOT_SUBMIT, slot_loop_submit, this);
                }

                if (id.fmt_ascii("sample_edit%d", int(i + 1)) > 0)
                    loader->wEditor         = pWrapper->controller()->widgets()->get<tk::AudioSample>(&id);
                if (id.fmt_ascii("ls_%d", int(i + 1)) > 0)
                    loader->pLoopSel        = bind_port(&id);
                if (id.fmt_ascii("fs_%d", int(i + 1)) > 0)
                    loader->pStatus         = bind_port(&id);
                if (id.fmt_ascii("sf_%d", int(i + 1)) > 0)
                    loader->pFileName       = bind_port(&id);

                for (size_t j=0; j<meta::referencer::AUDIO_LOOPS; ++j)
                {
                    if (id.fmt_ascii("lb_%d_%d", int(i + 1), int(j + 1)) > 0)
                        loader->vLoop[j].pStart     = bind_port(&id);
                    if (id.fmt_ascii("le_%d_%d", int(i + 1), int(j + 1)) > 0)
                        loader->vLoop[j].pEnd       = bind_port(&id);
                }
            }

            return STATUS_OK;
        }

        status_t referencer_ui::init_fft_meters()
        {
            fft_meters_t *fm    = &sFftMeters;

            fm->pHorLevel       = bind_port("famhor");
            fm->pVerSel         = bind_port("famvers");
            fm->pVerFreq        = bind_port("famver");
            fm->pVerMeter       = bind_port("famverv");
            fm->wHorText        = pWrapper->controller()->widgets()->get<tk::GraphText>("freq_analysis_hor");
            fm->wVerText        = pWrapper->controller()->widgets()->get<tk::GraphText>("freq_analysis_ver");

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
            LSP_STATUS_ASSERT(init_fft_meters());

            // Synchronize state of the matrix
            sync_matrix_state(NULL, ui::PORT_NONE);
            sync_waveform_state(NULL, ui::PORT_NONE);
            sync_meter_state(NULL);

            return STATUS_OK;
        }

        void referencer_ui::notify(ui::IPort *port, size_t flags)
        {
            if (port == NULL)
                return;

            sync_matrix_state(port, flags);
            sync_waveform_state(port, flags);
            sync_meter_state(port);
        }

        void referencer_ui::sync_matrix_state(ui::IPort *port, size_t flags)
        {
            // Activate playback of specific sample if sample or loop selector has triggered
            if ((port == NULL) || (port == sPlayMatrix.pPlayLoop) || (port == sPlayMatrix.pPlaySample))
            {
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

            // Reset loop range if file name has been changed by user
            if ((port != NULL) && (flags & ui::PORT_USER_EDIT))
            {
                for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
                {
                    sample_loader_t *sl = &sPlayMatrix.vLoaders[i];
                    if (sl->pFileName == port)
                    {
                        ssize_t index = (sl->pLoopSel != NULL) ? sl->pLoopSel->value() : -1;
                        if (index >= 0)
                        {
                            sl->vLoop[index].pStart->set_default();
                            sl->vLoop[index].pEnd->set_default();

                            sl->vLoop[index].pStart->notify_all(ui::PORT_USER_EDIT);
                            sl->vLoop[index].pEnd->notify_all(ui::PORT_USER_EDIT);
                        }
                    }
                }
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

                if ((flags & ui::PORT_USER_EDIT) && (delta < meta::referencer::WAVE_SRANGE_DIFF_MIN))
                {
                    if (port == wf->pLogMin)
                    {
                        log_max             = log_min + meta::referencer::WAVE_SRANGE_DIFF_MIN;
                        if (wf->pLogMax != NULL)
                        {
                            wf->pLogMax->set_value(log_max);
                            wf->pLogMax->notify_all(ui::PORT_USER_EDIT);
                        }
                    }
                    else
                    {
                        log_min             = log_max - meta::referencer::WAVE_SRANGE_DIFF_MIN;
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

        const char *referencer_ui::get_channel_key(ssize_t index) const
        {
            if (!bStereo)
                return (index == 0) ? "mix" : "ref";

            switch (index)
            {
                case 0: return "mix_left";
                case 1: return "mix_right";
                case 2: return "mix_mid";
                case 3: return "mix_side";
                case 4: return "ref_left";
                case 5: return "ref_right";
                case 6: return "ref_mid";
                case 7: return "ref_side";
                default: break;
            }
            return "mix_mid";
        }

        void referencer_ui::sync_meter_state(ui::IPort *port)
        {
            fft_meters_t *fm    = &sFftMeters;

            if ((fm->pHorLevel != NULL) && ((port == NULL) || (port == fm->pHorLevel)))
            {
                float mlvalue   = fm->pHorLevel->value();
                LSPString text;

                SET_LOCALE_SCOPED(LC_NUMERIC, "C");
                text.fmt_ascii("%.1f", dspu::gain_to_db(mlvalue));

                fm->wHorText->text()->params()->set_string("value", &text);
                fm->wHorText->text()->set_key("labels.values.x_db");
            }

            if (((fm->pVerFreq != NULL) && (fm->pVerMeter != NULL) && (fm->pVerSel != NULL)) &&
                ((port == NULL) || (port == fm->pVerFreq) || (port == fm->pVerMeter) || (port == fm->pVerSel)))
            {
                float freq = fm->pVerFreq->value();
                float level = fm->pVerMeter->value();

                // Update the note name displayed in the text
                // Fill the parameters
                expr::Parameters params;
                tk::prop::String snote;
                LSPString text;
                snote.bind(fm->wVerText->style(), pDisplay->dictionary());
                SET_LOCALE_SCOPED(LC_NUMERIC, "C");

                // Channels
                text.fmt_ascii("lists.referencer.fft.%s", get_channel_key(fm->pVerSel->value()));
                snote.set(&text);
                snote.format(&text);
                params.set_string("channel", &text);

                // Frequency
                text.fmt_ascii("%.2f", freq);
                params.set_string("frequency", &text);

                // Gain Level
                params.set_float("level", level);
                params.set_float("level_db", dspu::gain_to_db(level));

                // Note
                float note_full = dspu::frequency_to_note(freq);
                if (note_full != dspu::NOTE_OUT_OF_RANGE)
                {
                    note_full += 0.5f;
                    ssize_t note_number = ssize_t(note_full);

                    // Note name
                    ssize_t note        = note_number % 12;
                    text.fmt_ascii("lists.notes.names.%s", note_names[note]);
                    snote.set(&text);
                    snote.format(&text);
                    params.set_string("note", &text);

                    // Octave number
                    ssize_t octave      = (note_number / 12) - 1;
                    params.set_int("octave", octave);

                    // Cents
                    ssize_t note_cents  = (note_full - float(note_number)) * 100 - 50;
                    if (note_cents < 0)
                        text.fmt_ascii(" - %02d", -note_cents);
                    else
                        text.fmt_ascii(" + %02d", note_cents);
                    params.set_string("cents", &text);

                    fm->wVerText->text()->set("lists.referencer.display.full", &params);
                }
                else
                    fm->wVerText->text()->set("lists.referencer.display.unknown", &params);
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

        status_t referencer_ui::on_view_submit(tk::AudioSample *s)
        {
            ssize_t idx = -1;
            for (size_t i=0; i<meta::referencer::AUDIO_SAMPLES; ++i)
            {
                if (sPlayMatrix.vLoaders[i].wView == s)
                {
                    idx     = i;
                    break;
                }
            }
            if (idx < 0)
                return STATUS_OK;

            sample_loader_t *sl = &sPlayMatrix.vLoaders[idx];

            if (sPlayMatrix.pTabSel != NULL)
            {
                sPlayMatrix.pTabSel->set_value(meta::referencer::TAB_SAMPLES);
                sPlayMatrix.pTabSel->notify_all(ui::PORT_USER_EDIT);
            }

            if (sPlayMatrix.pPlaySample != NULL)
            {
                sPlayMatrix.pSampleSel->set_value(idx);
                sPlayMatrix.pSampleSel->notify_all(ui::PORT_USER_EDIT);
            }

            if (sPlayMatrix.pPlayLoop != NULL)
            {
                uint32_t loop_id  = sPlayMatrix.pPlayLoop->value() - meta::referencer::LOOP_SELECTOR_MIN;
                if (sl->pLoopSel != NULL)
                {
                    sl->pLoopSel->set_value(loop_id);
                    sl->pLoopSel->notify_all(ui::PORT_USER_EDIT);
                }
            }

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

        status_t referencer_ui::slot_loop_submit(tk::Widget *sender, void *ptr, void *data)
        {
            tk::AudioSample *s = tk::widget_cast<tk::AudioSample>(sender);
            if (s == NULL)
                return STATUS_OK;

            referencer_ui *self = static_cast<referencer_ui *>(ptr);
            return (self != NULL) ? self->on_view_submit(s) : STATUS_OK;
        }

    } /* namespace plugins */
} /* namespace lsp */


