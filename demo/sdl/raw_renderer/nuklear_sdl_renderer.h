/*
 * Nuklear - 4.9.4 - public domain
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_SDL_RENDERER_H_
#define NK_SDL_RENDERER_H_

#include "../nuklear_sdl_common.h"

#ifndef NK_SDL_RENDERER_SDL_H
#define NK_SDL_RENDERER_SDL_H <SDL.h>
#endif
#include NK_SDL_RENDERER_SDL_H
NK_API struct nk_context*   nk_sdl_init(SDL_Window *win, SDL_Renderer *renderer);
NK_API void                 nk_sdl_font_stash_end(void);
NK_API int                  nk_sdl_handle_event(SDL_Event *evt);
NK_API void                 nk_sdl_render(enum nk_anti_aliasing);
NK_API void                 nk_sdl_shutdown(void);

#if SDL_COMPILEDVERSION < SDL_VERSIONNUM(2, 0, 22)
/* Metal API does not support cliprects with negative coordinates or large
 * dimensions. The issue is fixed in SDL2 with version 2.0.22 but until
 * that version is released, the NK_SDL_CLAMP_CLIP_RECT flag can be used to
 * ensure the cliprect is itself clipped to the viewport.
 * See discussion at https://discourse.libsdl.org/t/rendergeometryraw-producing-different-results-in-metal-vs-opengl/34953
 */
#define NK_SDL_CLAMP_CLIP_RECT
#endif

#endif /* NK_SDL_RENDERER_H_ */

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_SDL_RENDERER_IMPLEMENTATION
#include <string.h>
#include <stdlib.h>

struct nk_sdl_device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    SDL_Texture *font_tex;
};

struct nk_sdl_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

NK_INTERN void
nk_sdl_device_upload_atlas(const void *image, int width, int height)
{
    struct nk_sdl_device *dev = &sdl.ogl;

    SDL_Texture *g_SDLFontTexture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (g_SDLFontTexture == NULL) {
        SDL_Log("error creating texture");
        return;
    }
    SDL_UpdateTexture(g_SDLFontTexture, NULL, image, 4 * width);
    SDL_SetTextureBlendMode(g_SDLFontTexture, SDL_BLENDMODE_BLEND);
    dev->font_tex = g_SDLFontTexture;
}

NK_API void
nk_sdl_render(enum nk_anti_aliasing AA)
{
    /* setup global state */
    struct nk_sdl_device *dev = &sdl.ogl;

    {
        SDL_Rect saved_clip;
#ifdef NK_SDL_CLAMP_CLIP_RECT
        SDL_Rect viewport;
#endif
        SDL_bool clipping_enabled;
        int vs = sizeof(struct nk_sdl_vertex);
        size_t vp = offsetof(struct nk_sdl_vertex, position);
        size_t vt = offsetof(struct nk_sdl_vertex, uv);
        size_t vc = offsetof(struct nk_sdl_vertex, col);

        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        const nk_draw_index *offset = NULL;
        struct nk_buffer vbuf, ebuf;

        /* fill converting configuration */
        struct nk_convert_config config;
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, uv)},
            {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(struct nk_sdl_vertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };

        Uint64 now = SDL_GetTicks64();
        sdl.ctx.delta_time_seconds = (float)(now - sdl.time_of_last_frame) / 1000;
        sdl.time_of_last_frame = now;

        NK_MEMSET(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(struct nk_sdl_vertex);
        config.vertex_alignment = NK_ALIGNOF(struct nk_sdl_vertex);
        config.tex_null = dev->tex_null;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;
        config.global_alpha = 1.0f;
        config.shape_AA = AA;
        config.line_AA = AA;

        /* convert shapes into vertexes */
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ebuf);
        nk_convert(&sdl.ctx, &dev->cmds, &vbuf, &ebuf, &config);

        /* iterate over and execute each draw command */
        offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);

        clipping_enabled = SDL_RenderIsClipEnabled(sdl.renderer);
        SDL_RenderGetClipRect(sdl.renderer, &saved_clip);
#ifdef NK_SDL_CLAMP_CLIP_RECT
        SDL_RenderGetViewport(sdl.renderer, &viewport);
#endif

        nk_draw_foreach(cmd, &sdl.ctx, &dev->cmds)
        {
            if (!cmd->elem_count) continue;

            {
                SDL_Rect r;
                r.x = cmd->clip_rect.x;
                r.y = cmd->clip_rect.y;
                r.w = cmd->clip_rect.w;
                r.h = cmd->clip_rect.h;
#ifdef NK_SDL_CLAMP_CLIP_RECT
                if (r.x < 0) {
                    r.w += r.x;
                    r.x = 0;
                }
                if (r.y < 0) {
                    r.h += r.y;
                    r.y = 0;
                }
                if (r.h > viewport.h) {
                    r.h = viewport.h;
                }
                if (r.w > viewport.w) {
                    r.w = viewport.w;
                }
#endif
                SDL_RenderSetClipRect(sdl.renderer, &r);
            }

            {
                const void *vertices = nk_buffer_memory_const(&vbuf);

                SDL_RenderGeometryRaw(sdl.renderer,
                        (SDL_Texture *)cmd->texture.ptr,
                        (const float*)((const nk_byte*)vertices + vp), vs,
                        (const SDL_Color*)((const nk_byte*)vertices + vc), vs,
                        (const float*)((const nk_byte*)vertices + vt), vs,
                        (vbuf.needed / vs),
                        (void *) offset, cmd->elem_count, 2);

                offset += cmd->elem_count;
            }
        }

        SDL_RenderSetClipRect(sdl.renderer, &saved_clip);
        if (!clipping_enabled) {
            SDL_RenderSetClipRect(sdl.renderer, NULL);
        }

        nk_clear(&sdl.ctx);
        nk_buffer_clear(&dev->cmds);
        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }
}

NK_API struct nk_context*
nk_sdl_init(SDL_Window *win, SDL_Renderer *renderer)
{
#ifndef NK_SDL_CLAMP_CLIP_RECT
    SDL_RendererInfo info;
    SDL_version runtimeVer;

    /* warn for cases where NK_SDL_CLAMP_CLIP_RECT should have been set but isn't */
    SDL_GetRendererInfo(renderer, &info);
    SDL_GetVersion(&runtimeVer);
    if (strncmp("metal", info.name, 5) == 0 &&
        SDL_VERSIONNUM(runtimeVer.major, runtimeVer.minor, runtimeVer.patch) < SDL_VERSIONNUM(2, 0, 22))
    {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "renderer is using Metal API but runtime SDL version %d.%d.%d is older than compiled version %d.%d.%d, "
            "which may cause issues with rendering",
            runtimeVer.major, runtimeVer.minor, runtimeVer.patch,
            SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL
        );
    }
#endif
    sdl.win = win;
    sdl.renderer = renderer;
    sdl.time_of_last_frame = SDL_GetTicks64();
    nk_init_default(&sdl.ctx, 0);
    sdl.ctx.clip.copy = nk_sdl_clipboard_copy;
    sdl.ctx.clip.paste = nk_sdl_clipboard_paste;
    sdl.ctx.clip.userdata = nk_handle_ptr(0);
    nk_buffer_init_default(&sdl.ogl.cmds);
    return &sdl.ctx;
}

NK_API
void nk_sdl_shutdown(void)
{
    struct nk_sdl_device *dev = &sdl.ogl;
    nk_font_atlas_clear(&sdl.atlas);
    nk_free(&sdl.ctx);
    SDL_DestroyTexture(dev->font_tex);
    /* glDeleteTextures(1, &dev->font_tex); */
    nk_buffer_free(&dev->cmds);
    memset(&sdl, 0, sizeof(sdl));
}

#endif /* NK_SDL_RENDERER_IMPLEMENTATION */
