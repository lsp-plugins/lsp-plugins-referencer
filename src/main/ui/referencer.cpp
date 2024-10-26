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

        status_t referencer_ui::post_init()
        {
            // Initialize parent class
            status_t res = ui::Module::post_init();
            if (res != STATUS_OK)
                return res;

            // Initialize playback matrix
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


            // Synchronize state of the matrix
            sync_matrix_state();

            return STATUS_OK;
        }

        void referencer_ui::notify(ui::IPort *port, size_t flags)
        {
            if ((port == sPlayMatrix.pPlayLoop) || (port == sPlayMatrix.pPlaySample))
                sync_matrix_state();
        }

        void referencer_ui::sync_matrix_state()
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


