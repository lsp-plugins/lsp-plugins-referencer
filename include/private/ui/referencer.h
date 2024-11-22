/*
 * Copyright (C) 2024 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2024 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-referencer
 * Created on: 26 окт. 2024 г.
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

#ifndef PRIVATE_UI_REFERENCER_H_
#define PRIVATE_UI_REFERENCER_H_

#include <lsp-plug.in/plug-fw/ui.h>
#include <lsp-plug.in/lltl/darray.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * UI for Referencer plugin series
         */
        class referencer_ui: public ui::Module, public ui::IPortListener
        {
            protected:
                typedef struct play_matrix_t
                {
                    ui::IPort                  *pPlaySample;
                    ui::IPort                  *pPlayLoop;

                    lltl::parray<tk::Button>    vButtons;
                } play_matrix_t;

                typedef struct waveform_t
                {
                    ui::IPort                  *pLogScale;
                    ui::IPort                  *pLinMax;
                    ui::IPort                  *pLogMin;
                    ui::IPort                  *pLogMax;

                    float                       fScaleMin;
                    float                       fScaleMax;
                    bool                        bLogScale;
                    bool                        bEditing;

                    lltl::parray<tk::GraphMesh> vMeshes;
                } waveform_t;

            protected:
                play_matrix_t               sPlayMatrix;
                waveform_t                  sWaveform;

            protected:
                static bool         waveform_transform_func(float *dst, const float *src, size_t count, tk::GraphMesh::coord_t coord, void *data);

                static status_t     slot_matrix_change(tk::Widget *sender, void *ptr, void *data);

            protected:
                ui::IPort          *bind_port(const char *id);
                void                sync_matrix_state(ui::IPort *port);
                void                sync_waveform_state(ui::IPort *port, size_t flags);
                status_t            init_waveform_graphs();
                status_t            init_playback_matrix();
                status_t            on_matrix_change(tk::Button *btn);

            public:
                explicit referencer_ui(const meta::plugin_t *meta);
                virtual ~referencer_ui() override;

                virtual status_t    post_init() override;

                virtual void        notify(ui::IPort *port, size_t flags) override;
        };
    } /* namespace plugins */
} /* namespace lsp */




#endif /* PRIVATE_UI_REFERENCER_H_ */
