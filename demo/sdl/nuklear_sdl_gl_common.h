#ifndef NK_SDL_GL_COMMON_H_
#define NK_SDL_GL_COMMON_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define NK_INTERN // TODO

NK_INTERN void
nk_sdl_device_upload_atlas(GLuint *font_tex, const void *image, int width, int height)
{
    glGenTextures(1, font_tex);
    glBindTexture(GL_TEXTURE_2D, *font_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, image);
}

#endif // NK_SDL_GL_COMMON_H_
