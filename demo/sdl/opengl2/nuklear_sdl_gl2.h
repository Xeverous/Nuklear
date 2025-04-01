/*
 * Nuklear - 1.32.0 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2017 by Micha Mettke
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_SDL_GL2_H_
#define NK_SDL_GL2_H_

#include "../nuklear_sdl_common.h"

#include <SDL2/SDL.h>

NK_API void nk_sdl_init(struct nk_context *ctx, struct nk_buffer *cmds, Uint64 *time_of_last_frame);
NK_API void nk_sdl_render(struct nk_context *ctx, struct nk_buffer *cmds,
                          struct nk_draw_null_texture *tex_null, SDL_Window *win,
                          enum nk_anti_aliasing AA, Uint64 *time_of_last_frame);
NK_API void nk_sdl_shutdown(struct nk_context *ctx, struct nk_font_atlas *atlas,
                            struct nk_buffer *cmds, GLuint font_tex);

#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_SDL_GL2_IMPLEMENTATION
#include <string.h>
#include <stdlib.h>

struct nk_sdl_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

NK_API void
nk_sdl_render(struct nk_context *ctx, struct nk_buffer *cmds, struct nk_draw_null_texture *tex_null, SDL_Window *win, enum nk_anti_aliasing AA, Uint64 *time_of_last_frame)
{
    /* setup global state */
    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale;
    Uint64 now;

    NK_ASSERT(ctx);
    NK_ASSERT(cmds);
    NK_ASSERT(tex_null);
    NK_ASSERT(win);
    NK_ASSERT(time_of_last_frame);

    now = SDL_GetTicks64();
    ctx->delta_time_seconds = (float)(now - *time_of_last_frame) / 1000;
    *time_of_last_frame = now;

    SDL_GetWindowSize(win, &width, &height);
    SDL_GL_GetDrawableSize(win, &display_width, &display_height);
    scale.x = (float)display_width/(float)width;
    scale.y = (float)display_height/(float)height;

    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_TRANSFORM_BIT);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* setup viewport/project */
    glViewport(0,0,(GLsizei)display_width,(GLsizei)display_height);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    {
        GLsizei vs = sizeof(struct nk_sdl_vertex);
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
        memset(&config, 0, sizeof(config));
        config.vertex_layout = vertex_layout;
        config.vertex_size = sizeof(struct nk_sdl_vertex);
        config.vertex_alignment = NK_ALIGNOF(struct nk_sdl_vertex);
        config.tex_null = *tex_null;
        config.circle_segment_count = 22;
        config.curve_segment_count = 22;
        config.arc_segment_count = 22;
        config.global_alpha = 1.0f;
        config.shape_AA = AA;
        config.line_AA = AA;

        /* convert shapes into vertexes */
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ebuf);
        nk_convert(ctx, cmds, &vbuf, &ebuf, &config);

        /* setup vertex buffer pointer */
        {const void *vertices = nk_buffer_memory_const(&vbuf);
        glVertexPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vp));
        glTexCoordPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vt));
        glColorPointer(4, GL_UNSIGNED_BYTE, vs, (const void*)((const nk_byte*)vertices + vc));}

        /* iterate over and execute each draw command */
        offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);
        nk_draw_foreach(cmd, ctx, cmds)
        {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor(
                (GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(ctx);
        nk_buffer_clear(cmds);
        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }

    /* default OpenGL state */
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();
}

NK_API void
nk_sdl_init(struct nk_context *ctx, struct nk_buffer *cmds, Uint64 *time_of_last_frame)
{
    NK_ASSERT(ctx);
    NK_ASSERT(cmds);
    NK_ASSERT(time_of_last_frame);

    nk_init_default(ctx, 0);
    ctx->clip.copy = nk_sdl_clipboard_copy;
    ctx->clip.paste = nk_sdl_clipboard_paste;
    ctx->clip.userdata = nk_handle_ptr(0);
    nk_buffer_init_default(cmds);
    *time_of_last_frame = SDL_GetTicks64();
}

NK_API
void nk_sdl_shutdown(struct nk_context *ctx, struct nk_font_atlas *atlas, struct nk_buffer *cmds, GLuint font_tex)
{
    NK_ASSERT(ctx);
    NK_ASSERT(atlas);
    NK_ASSERT(cmds);

    nk_font_atlas_clear(atlas);
    nk_free(ctx);
    glDeleteTextures(1, &font_tex);
    nk_buffer_free(cmds);
    memset(atlas, 0, sizeof(*atlas));
    memset(cmds, 0, sizeof(*cmds));
}

#endif
