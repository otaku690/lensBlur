// Lens Blur
// 
// University of Pennsylvania CIS660 final project
// copyright (c) 2014 Cheng-Tso Lin  

#ifndef _SHADER_H
#define _SHADER_H

namespace shader
{

enum shaderAttrib{
    i1, fv3, fv4, f1, mat4x4, mat3x3, tex, img
};

class ShaderProgram
{
public:
    ShaderProgram();
    virtual ~ShaderProgram();
    int init( const char* vs_source, const char* fs_source, const char* gs_source = 0 );
    void use() const;
    void unuse() const;
    void setParameter( shaderAttrib type, void* param, const char* name ) const;
    void setParameter( shaderAttrib type, void* param, int loc ) const;
    void setTexParameter( int idx, const char* name ) const;
    void setTexParameter( int idx, int loc ) const;
    void bindAttribLocation( unsigned int idx, char* name ) const;
    void bindFragDataLocation( unsigned int idx, char* name ) const;
    int queryUniformLocation( const char* name ) const;
protected:
    GLuint vs; //vertex shader
    GLuint fs; //fragment shader
    GLuint gs; //geometry shader
    GLuint program;
};

class ComputeShader: public ShaderProgram
{
public:
    ComputeShader();
    ~ComputeShader();
    int init( const char* cs_source );
};

}
#endif