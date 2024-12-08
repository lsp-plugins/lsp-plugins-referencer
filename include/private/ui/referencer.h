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
                enum key_state_t
                {
                    KS_ALT_LEFT     = 1 << 0,
                    KS_ALT_RIGHT    = 1 << 1,

                    KS_ALT          = KS_ALT_LEFT | KS_ALT_RIGHT
                };

                typedef struct sample_loop_t
                {
                    ui::IPort                      *pStart;
                    ui::IPort                      *pEnd;
                } sample_loop_t;

                typedef struct sample_loader_t
                {
                    ui::IPort                      *pStatus;                // Status of sample loading
                    ui::IPort                      *pLoopSel;               // Current loop selector
                    ui::IPort                      *pFileName;              // Current file name
                    tk::AudioSample                *wView;                  // Loop view widget
                    tk::AudioSample                *wEditor;                // Sample editor widget
                    sample_loop_t                   vLoop[meta::referencer::AUDIO_LOOPS];   // Loop parameters
                } sample_loader_t;

                typedef struct play_matrix_t
                {
                    ui::IPort                      *pPlaySample;
                    ui::IPort                      *pPlayLoop;

                    ui::IPort                      *pTabSel;
                    ui::IPort                      *pSampleSel;

                    sample_loader_t                 vLoaders[meta::referencer::AUDIO_SAMPLES];

                    lltl::parray<tk::Button>        vButtons;
                } play_matrix_t;

                typedef struct waveform_t
                {
                    ui::IPort                  *pLogScale;
                    ui::IPort                  *pZoomMin;
                    ui::IPort                  *pZoomMax;
                    ui::IPort                  *pTimePeriod;
                    ui::IPort                  *pMixShift;
                    ui::IPort                  *pRefShift;

                    float                       fScaleMin;
                    float                       fScaleMax;
                    float                       fOldMixShift;
                    float                       fOldRefShift;
                    float                       fOldZoom;
                    ssize_t                     nMouseX;
                    ssize_t                     nMouseY;
                    size_t                      nBtnState;
                    size_t                      nKeyState;
                    bool                        bLogScale;
                    bool                        bEditing;

                    tk::Graph                  *wGraph;
                    lltl::parray<tk::GraphMesh> vMeshes;
                } waveform_t;

                typedef struct fft_meters_t
                {
                    ui::IPort                  *pHorLevel;
                    ui::IPort                  *pVerSel;
                    ui::IPort                  *pVerFreq;
                    ui::IPort                  *pVerMeter;

                    size_t                      nBtnState;

                    tk::Graph                  *wGraph;
                    tk::GraphText              *wHorText;
                    tk::GraphAxis              *wXAxis;
                    tk::GraphAxis              *wYAxis;
                    tk::GraphDot               *wMouseDot;
                    tk::GraphText              *wMouseText;
                    lltl::parray<tk::GraphText> vVerText;
                } fft_meters_t;

            protected:
                play_matrix_t               sPlayMatrix;
                waveform_t                  sWaveform;
                fft_meters_t                sFftMeters;
                bool                        bStereo;

            protected:
                static bool         waveform_transform_func(float *dst, const float *src, size_t count, tk::GraphMesh::coord_t coord, void *data);
                static float        calc_zoom(waveform_t *wf, ssize_t x, ssize_t y, float accel);
                static void         sync_spectrum_freq_selector(fft_meters_t *fm, const ws::event_t *ev);
                static void         sync_spectrum_mouse_dot(fft_meters_t *fm, const ws::event_t *ev);
                static bool         fmt_note_name(tk::Widget *w, expr::Parameters *params, float freq);
                static float        log_relation(float v, float min, float max);

            protected:
                static status_t     slot_matrix_change(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_loop_submit(tk::Widget *sender, void *ptr, void *data);

                static status_t     slot_waveform_mouse_down(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_mouse_up(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_mouse_move(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_mouse_scroll(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_mouse_dbl_click(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_key_down(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_key_up(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_waveform_key_change(tk::Widget *sender, void *ptr, void *data, bool down);

                static status_t     slot_spectrum_mouse_in(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_spectrum_mouse_out(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_spectrum_mouse_down(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_spectrum_mouse_up(tk::Widget *sender, void *ptr, void *data);
                static status_t     slot_spectrum_mouse_move(tk::Widget *sender, void *ptr, void *data);

            protected:
                status_t            on_matrix_change(tk::Button *btn);
                status_t            on_view_submit(tk::AudioSample *s);

                status_t            on_waveform_mouse_down(const ws::event_t *ev);
                status_t            on_waveform_mouse_up(const ws::event_t *ev);
                status_t            on_waveform_mouse_move(const ws::event_t *ev);
                status_t            on_waveform_mouse_scroll(const ws::event_t *ev);
                status_t            on_waveform_mouse_dbl_click(const ws::event_t *ev);
                status_t            on_waveform_key_down(const ws::event_t *ev);
                status_t            on_waveform_key_up(const ws::event_t *ev);

            protected:
                ui::IPort          *bind_port(const char *id);
                ui::IPort          *bind_port(const LSPString *id);
                void                sync_matrix_state(ui::IPort *port, size_t flags);
                void                sync_waveform_state(ui::IPort *port, size_t flags);
                void                sync_meter_state(ui::IPort *port);
                status_t            init_waveform_graphs();
                status_t            init_playback_matrix();
                status_t            init_fft_meters();
                const char         *get_channel_key(ssize_t index) const;

            public:
                explicit referencer_ui(const meta::plugin_t *meta);
                virtual ~referencer_ui() override;

                virtual status_t    post_init() override;

                virtual void        notify(ui::IPort *port, size_t flags) override;
        };
    } /* namespace plugins */
} /* namespace lsp */




#endif /* PRIVATE_UI_REFERENCER_H_ */
