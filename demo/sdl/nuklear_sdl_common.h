#ifndef NK_SDL_COMMON_H_
#define NK_SDL_COMMON_H_

#include <SDL2/SDL.h>

static void nk_sdl_clipboard_paste(nk_handle usr, struct nk_text_edit *edit);
static void nk_sdl_clipboard_copy(nk_handle usr, const char *text, int len);

NK_API void nk_sdl_font_stash_begin(struct nk_font_atlas *atlas);
NK_API void nk_sdl_font_stash_end(struct nk_context *ctx, struct nk_font_atlas *atlas,
                                  struct nk_draw_null_texture *tex_null, nk_handle font_tex);

NK_API void nk_sdl_handle_grab(struct nk_context *ctx, SDL_Window *win);
NK_API int nk_sdl_handle_event(struct nk_context *ctx, SDL_Event *evt);

#ifdef NK_IMPLEMENTATION

#endif // NK_IMPLEMENTATION

static void
nk_sdl_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    const char *text = SDL_GetClipboardText();
    if (text) {
        nk_textedit_paste(edit, text, nk_strlen(text));
        SDL_free((void *)text);
    }
    (void)usr;
}

static void
nk_sdl_clipboard_copy(nk_handle usr, const char *text, int len)
{
    char *str = 0;
    (void)usr;
    if (!len) return;
    str = (char*)malloc((size_t)len+1);
    if (!str) return;
    memcpy(str, text, (size_t)len);
    str[len] = '\0';
    SDL_SetClipboardText(str);
    free(str);
}

NK_API void
nk_sdl_font_stash_begin(struct nk_font_atlas *atlas)
{
    NK_ASSERT(atlas);

    nk_font_atlas_init_default(atlas);
    nk_font_atlas_begin(atlas);
}

NK_API void
nk_sdl_font_stash_end(struct nk_context *ctx, struct nk_font_atlas *atlas, struct nk_draw_null_texture *tex_null, nk_handle font_tex)
{
    const void *image; int w, h;

    NK_ASSERT(ctx);
    NK_ASSERT(atlas);
    NK_ASSERT(tex_null);

    image = nk_font_atlas_bake(atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    nk_sdl_device_upload_atlas(font_tex, image, w, h); // XXX same in 2,3,ES different in raw
    nk_font_atlas_end(atlas, font_tex, tex_null);
    if (atlas->default_font)
        nk_style_set_font(ctx, &atlas->default_font->handle);
}

NK_API void
nk_sdl_handle_grab(struct nk_context *ctx, SDL_Window *win)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);

    if (ctx->input.mouse.grab) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    } else if (ctx->input.mouse.ungrab) {
        /* better support for older SDL by setting mode first; causes an extra mouse motion event */
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_WarpMouseInWindow(win, (int)ctx->input.mouse.prev.x, (int)ctx->input.mouse.prev.y);
    } else if (ctx->input.mouse.grabbed) {
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }
}

NK_API int
nk_sdl_handle_event(struct nk_context *ctx, SDL_Event *evt)
{
    NK_ASSERT(ctx);
    NK_ASSERT(evt);

    switch(evt->type)
    {
        case SDL_KEYUP: /* KEYUP & KEYDOWN share same routine */
        case SDL_KEYDOWN:
            {
                int down = evt->type == SDL_KEYDOWN;
                int ctrl_down = SDL_GetModState() & (KMOD_LCTRL | KMOD_RCTRL);
                switch(evt->key.keysym.sym)
                {
                    case SDLK_RSHIFT: /* RSHIFT & LSHIFT share same routine */
                    case SDLK_LSHIFT:    nk_input_key(ctx, NK_KEY_SHIFT, down); break;
                    case SDLK_DELETE:    nk_input_key(ctx, NK_KEY_DEL, down); break;

                    case SDLK_KP_ENTER:
                    case SDLK_RETURN:    nk_input_key(ctx, NK_KEY_ENTER, down); break;

                    case SDLK_TAB:       nk_input_key(ctx, NK_KEY_TAB, down); break;
                    case SDLK_BACKSPACE: nk_input_key(ctx, NK_KEY_BACKSPACE, down); break;
                    case SDLK_HOME:      nk_input_key(ctx, NK_KEY_TEXT_START, down);
                                         nk_input_key(ctx, NK_KEY_SCROLL_START, down); break;
                    case SDLK_END:       nk_input_key(ctx, NK_KEY_TEXT_END, down);
                                         nk_input_key(ctx, NK_KEY_SCROLL_END, down); break;
                    case SDLK_PAGEDOWN:  nk_input_key(ctx, NK_KEY_SCROLL_DOWN, down); break;
                    case SDLK_PAGEUP:    nk_input_key(ctx, NK_KEY_SCROLL_UP, down); break;
                    case SDLK_z:         nk_input_key(ctx, NK_KEY_TEXT_UNDO, down && ctrl_down); break;
                    case SDLK_r:         nk_input_key(ctx, NK_KEY_TEXT_REDO, down && ctrl_down); break;
                    case SDLK_c:         nk_input_key(ctx, NK_KEY_COPY, down && ctrl_down); break;
                    case SDLK_v:         nk_input_key(ctx, NK_KEY_PASTE, down && ctrl_down); break;
                    case SDLK_x:         nk_input_key(ctx, NK_KEY_CUT, down && ctrl_down); break;
                    case SDLK_b:         nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl_down); break;
                    case SDLK_e:         nk_input_key(ctx, NK_KEY_TEXT_LINE_END, down && ctrl_down); break;
                    case SDLK_UP:        nk_input_key(ctx, NK_KEY_UP, down); break;
                    case SDLK_DOWN:      nk_input_key(ctx, NK_KEY_DOWN, down); break;
                    case SDLK_a:
                        if (ctrl_down)
                            nk_input_key(ctx,NK_KEY_TEXT_SELECT_ALL, down);
                        break;
                    case SDLK_LEFT:
                        if (ctrl_down)
                            nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, down);
                        else nk_input_key(ctx, NK_KEY_LEFT, down);
                        break;
                    case SDLK_RIGHT:
                        if (ctrl_down)
                            nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, down);
                        else nk_input_key(ctx, NK_KEY_RIGHT, down);
                        break;
                }
            }
            return 1;

        case SDL_MOUSEBUTTONUP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
        case SDL_MOUSEBUTTONDOWN:
            {
                int down = evt->type == SDL_MOUSEBUTTONDOWN;
                const int x = evt->button.x, y = evt->button.y;
                switch(evt->button.button)
                {
                    case SDL_BUTTON_LEFT:
                        if (evt->button.clicks > 1)
                            nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
                        nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down); break;
                    case SDL_BUTTON_MIDDLE: nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down); break;
                    case SDL_BUTTON_RIGHT:  nk_input_button(ctx, NK_BUTTON_RIGHT, x, y, down); break;
                }
            }
            return 1;

        case SDL_MOUSEMOTION:
            if (ctx->input.mouse.grabbed) {
                int x = (int)ctx->input.mouse.prev.x, y = (int)ctx->input.mouse.prev.y;
                nk_input_motion(ctx, x + evt->motion.xrel, y + evt->motion.yrel);
            }
            else nk_input_motion(ctx, evt->motion.x, evt->motion.y);
            return 1;

        case SDL_TEXTINPUT:
            {
                nk_glyph glyph;
                memcpy(glyph, evt->text.text, NK_UTF_SIZE);
                nk_input_glyph(ctx, glyph);
            }
            return 1;

        case SDL_MOUSEWHEEL:
            nk_input_scroll(ctx,nk_vec2(evt->wheel.preciseX, evt->wheel.preciseY));
            return 1;
    }
    return 0;
}

#endif // NK_SDL_COMMON_H_
