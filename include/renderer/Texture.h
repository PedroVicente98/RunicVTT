#pragma once

#include "Renderer.h"

class Texture
{
private:
    unsigned char* m_LocalBuffer;
    int m_Width, m_Height, m_BPP;
    unsigned int m_RendererID;

public:
    std::string m_FilePath;
    Texture(const std::string& path);
    ~Texture();
    unsigned int GetRendererID();
    void SetFilePath(const std::string& path);

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    inline int GetWidth() const
    {
        return m_Width;
    };
    inline int GetHeight() const
    {
        return m_Height;
    };
};