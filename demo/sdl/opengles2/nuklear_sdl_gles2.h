/*
 * Nuklear - 1.40.8 - public domain
 * no warrenty implied; use at your own risk.
 * authored from 2015-2017 by Micha Mettke
 * emscripten from 2016 by Chris Willcocks
 * OpenGL ES 2.0 from 2017 by Dmitry Hrabrov a.k.a. DeXPeriX
 */
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_SDL_GLES2_H_
#define NK_SDL_GLES2_H_

#include "../nuklear_sdl_common.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

struct nk_sdl_device {
    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    GLuint vbo, ebo;
    GLuint prog;
    GLuint vert_shdr;
    GLuint frag_shdr;
    GLint attrib_pos;
    GLint attrib_uv;
    GLint attrib_col;
    GLint uniform_tex;
    GLint uniform_proj;
    GLuint font_tex;
    GLsizei vs;
    size_t vp, vt, vc;
};

NK_API void nk_sdl_init(struct nk_context *ctx, struct nk_sdl_device *dev, Uint64 *time_of_last_frame);
NK_API void nk_sdl_render(struct nk_context *ctx, struct nk_sdl_device *dev, SDL_Window *win, enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer, Uint64 *time_of_last_frame);
NK_API void nk_sdl_shutdown(struct nk_context *ctx, struct nk_font_atlas *atlas, struct nk_sdl_device *dev);
NK_API void nk_sdl_device_destroy(struct nk_sdl_device *dev);
NK_API void nk_sdl_device_create(struct nk_sdl_device *dev);

#endif

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_SDL_GLES2_IMPLEMENTATION
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct nk_sdl_vertex {
    GLfloat position[2];
    GLfloat uv[2];
    nk_byte col[4];
};

struct nk_sdl {
    SDL_Window *win;
    struct nk_sdl_device ogl;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    Uint64 time_of_last_frame;
};


#define NK_SHADER_VERSION "#version 100\n"


NK_API void
nk_sdl_device_create(struct nk_sdl_device *dev)
{
    GLint status;
    static const GLchar *vertex_shader =
        NK_SHADER_VERSION
        "uniform mat4 ProjMtx;\n"
        "attribute vec2 Position;\n"
        "attribute vec2 TexCoord;\n"
        "attribute vec4 Color;\n"
        "varying vec2 Frag_UV;\n"
        "varying vec4 Frag_Color;\n"
        "void main() {\n"
        "   Frag_UV = TexCoord;\n"
        "   Frag_Color = Color;\n"
        "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
        "}\n";
    static const GLchar *fragment_shader =
        NK_SHADER_VERSION
        "precision mediump float;\n"
        "uniform sampler2D Texture;\n"
        "varying vec2 Frag_UV;\n"
        "varying vec4 Frag_Color;\n"
        "void main(){\n"
        "   gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV);\n"
        "}\n";

    nk_buffer_init_default(&dev->cmds);
    dev->prog = glCreateProgram();
    dev->vert_shdr = glCreateShader(GL_VERTEX_SHADER);
    dev->frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dev->vert_shdr, 1, &vertex_shader, 0);
    glShaderSource(dev->frag_shdr, 1, &fragment_shader, 0);
    glCompileShader(dev->vert_shdr);
    glCompileShader(dev->frag_shdr);
    glGetShaderiv(dev->vert_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glGetShaderiv(dev->frag_shdr, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);
    glAttachShader(dev->prog, dev->vert_shdr);
    glAttachShader(dev->prog, dev->frag_shdr);
    glLinkProgram(dev->prog);
    glGetProgramiv(dev->prog, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);


    dev->uniform_tex = glGetUniformLocation(dev->prog, "Texture");
    dev->uniform_proj = glGetUniformLocation(dev->prog, "ProjMtx");
    dev->attrib_pos = glGetAttribLocation(dev->prog, "Position");
    dev->attrib_uv = glGetAttribLocation(dev->prog, "TexCoord");
    dev->attrib_col = glGetAttribLocation(dev->prog, "Color");
    {
        dev->vs = sizeof(struct nk_sdl_vertex);
        dev->vp = offsetof(struct nk_sdl_vertex, position);
        dev->vt = offsetof(struct nk_sdl_vertex, uv);
        dev->vc = offsetof(struct nk_sdl_vertex, col);

        /* Allocate buffers */
        glGenBuffers(1, &dev->vbo);
        glGenBuffers(1, &dev->ebo);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

NK_API void
nk_sdl_device_destroy(struct nk_sdl_device *dev)
{
    glDetachShader(dev->prog, dev->vert_shdr);
    glDetachShader(dev->prog, dev->frag_shdr);
    glDeleteShader(dev->vert_shdr);
    glDeleteShader(dev->frag_shdr);
    glDeleteProgram(dev->prog);
    glDeleteTextures(1, &dev->font_tex);
    glDeleteBuffers(1, &dev->vbo);
    glDeleteBuffers(1, &dev->ebo);
    nk_buffer_free(&dev->cmds);
}

NK_API void
nk_sdl_render(struct nk_context *ctx, struct nk_sdl_device *dev, SDL_Window *win, enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer, Uint64 *time_of_last_frame)
{
    int width, height;
    int display_width, display_height;
    struct nk_vec2 scale;
    GLfloat ortho[4][4] = {
        {  2.0f,  0.0f,  0.0f, 0.0f },
        {  0.0f, -2.0f,  0.0f, 0.0f },
        {  0.0f,  0.0f, -1.0f, 0.0f },
        { -1.0f,  1.0f,  0.0f, 1.0f },
    };

    Uint64 now = SDL_GetTicks64();
    ctx->delta_time_seconds = (float)(now - *time_of_last_frame) / 1000;
    *time_of_last_frame = now;

    SDL_GetWindowSize(win, &width, &height);
    SDL_GL_GetDrawableSize(win, &display_width, &display_height);
    ortho[0][0] /= (GLfloat)width;
    ortho[1][1] /= (GLfloat)height;

    scale.x = (float)display_width/(float)width;
    scale.y = (float)display_height/(float)height;

    /* setup global state */
    glViewport(0,0,display_width,display_height);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    /* setup program */
    glUseProgram(dev->prog);
    glUniform1i(dev->uniform_tex, 0);
    glUniformMatrix4fv(dev->uniform_proj, 1, GL_FALSE, &ortho[0][0]);
    {
        /* convert from command queue into draw list and draw to screen */
        const struct nk_draw_command *cmd;
        void *vertices, *elements;
        const nk_draw_index *offset = NULL;

        /* Bind buffers */
        glBindBuffer(GL_ARRAY_BUFFER, dev->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dev->ebo);

        {
            /* buffer setup */
            glEnableVertexAttribArray((GLuint)dev->attrib_pos);
            glEnableVertexAttribArray((GLuint)dev->attrib_uv);
            glEnableVertexAttribArray((GLuint)dev->attrib_col);

            glVertexAttribPointer((GLuint)dev->attrib_pos, 2, GL_FLOAT, GL_FALSE, dev->vs, (void*)dev->vp);
            glVertexAttribPointer((GLuint)dev->attrib_uv, 2, GL_FLOAT, GL_FALSE, dev->vs, (void*)dev->vt);
            glVertexAttribPointer((GLuint)dev->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, dev->vs, (void*)dev->vc);
        }

        glBufferData(GL_ARRAY_BUFFER, max_vertex_buffer, NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_element_buffer, NULL, GL_STREAM_DRAW);

        /* load vertices/elements directly into vertex/element buffer */
        vertices = malloc((size_t)max_vertex_buffer);
        elements = malloc((size_t)max_element_buffer);
        {
            /* fill convert configuration */
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
            config.tex_null = dev->tex_null;
            config.circle_segment_count = 22;
            config.curve_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.0f;
            config.shape_AA = AA;
            config.line_AA = AA;

            /* setup buffers to load vertices and elements */
            {struct nk_buffer vbuf, ebuf;
            nk_buffer_init_fixed(&vbuf, vertices, (nk_size)max_vertex_buffer);
            nk_buffer_init_fixed(&ebuf, elements, (nk_size)max_element_buffer);
            nk_convert(ctx, &dev->cmds, &vbuf, &ebuf, &config);}
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, (size_t)max_vertex_buffer, vertices);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (size_t)max_element_buffer, elements);
        free(vertices);
        free(elements);

        /* iterate over and execute each draw command */
        nk_draw_foreach(cmd, ctx, &dev->cmds) {
            if (!cmd->elem_count) continue;
            glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
            glScissor((GLint)(cmd->clip_rect.x * scale.x),
                (GLint)((height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
                (GLint)(cmd->clip_rect.w * scale.x),
                (GLint)(cmd->clip_rect.h * scale.y));
            glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
            offset += cmd->elem_count;
        }
        nk_clear(ctx);
        nk_buffer_clear(&dev->cmds);
    }

    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
}

NK_API void
nk_sdl_init(struct nk_context *ctx, struct nk_sdl_device *dev, Uint64 *time_of_last_frame)
{
    NK_ASSERT(ctx);
    NK_ASSERT(dev);
    NK_ASSERT(time_of_last_frame);

    nk_init_default(ctx, 0);
    ctx->clip.copy = nk_sdl_clipboard_copy;
    ctx->clip.paste = nk_sdl_clipboard_paste;
    ctx->clip.userdata = nk_handle_ptr(0);
    nk_sdl_device_create(dev);
    *time_of_last_frame = SDL_GetTicks64();
}

NK_API
void nk_sdl_shutdown(struct nk_context *ctx, struct nk_font_atlas *atlas, struct nk_sdl_device *dev)
{
    NK_ASSERT(ctx);
    NK_ASSERT(atlas);
    NK_ASSERT(dev);

    nk_font_atlas_clear(atlas);
    nk_free(ctx);
    nk_sdl_device_destroy(dev);
    memset(atlas, 0, sizeof(*atlas));
    memset(dev, 0, sizeof(*dev));
}

#endif
