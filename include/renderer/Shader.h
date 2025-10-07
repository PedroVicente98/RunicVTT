#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "glm/glm.hpp"
#include <unordered_map>

struct ShaderProgramSource
{
    std::string VertexSource;
    std::string FragmentSource;
};

class Shader
{
private:
    unsigned int m_RendererID;
    std::string m_FilePath;
    std::unordered_map<std::string, int> m_UniformLocationCache;

public:
    Shader(const std::string& filepath);
    ;
    ~Shader();

    void Bind() const;
    void Unbind() const;

    //Set uniforms

    void SetUniform1i(const std::string& name, int value);
    void SetUniform4f(const std::string& name, float v0, float v1, float f2, float f3);
    void SetUniform1f(const std::string& name, float value);
    void SetUniformMat4f(const std::string& name, glm::mat4& matrix);
    void SetUniform2f(const std::string& name, float v0, float v1);

private:
    ShaderProgramSource ParseShader(const std::string& filepath);
    unsigned int CompileShader(unsigned int type, const std::string& source);
    unsigned int CreateShader(const std::string& verxterShader, const std::string& fragmentShader);
    int GetUniformLocation(const std::string& name);
};