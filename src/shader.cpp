// Lens Blur
// 
// University of Pennsylvania CIS660 final project
// copyright (c) 2014 Cheng-Tso Lin  

#include <gl/glew.h>
#include "shader.h"
#include "glslUtility.h"

using namespace glslUtility;

namespace shader
{

ShaderProgram::ShaderProgram()
{
    program = 0;
    vs = 0;
    fs = 0;
    gs = 0;
}

ShaderProgram::~ShaderProgram()
{
    if( program )
        glDeleteProgram( program );
    if(vs )
        glDeleteShader( vs );
    if( fs )
        glDeleteShader( fs );
    if( gs )
        glDeleteShader( gs );
}

int ShaderProgram::init(const char* vs_source, const char* fs_source, const char* gs_source )
{
    //load shader sources and compile
    shaders_t shaderSet = loadShaders( vs_source, fs_source, gs_source );
    vs = shaderSet.vertex;
    fs = shaderSet.fragment;
    gs = shaderSet.geometry;
   
    //create program
    program = glCreateProgram();

    //attach shader 
    attachAndLinkProgram( program, shaderSet );

    return 0;
}

void ShaderProgram::use() const
{
    glUseProgram( program );
}
void ShaderProgram::unuse() const
{
    glUseProgram( 0 );
}

void ShaderProgram::setParameter( shaderAttrib type, void* param, const char* name ) const
{
    GLint loc  =  glGetUniformLocation( program, name );
    int val = *((int*)param);
    switch( type )
    {
    case i1:
        glUniform1i( glGetUniformLocation( program, name ), *((int*)param) );
        break;
    case f1:
        glUniform1f( glGetUniformLocation( program, name ), *((float*)param) );
        break;
    case fv3:
        glUniform3fv( glGetUniformLocation( program, name ), 1, (float*)param );
        break;
    case fv4:
        glUniform4fv( glGetUniformLocation( program, name ), 1, (float*)param );
        break;
    case mat4x4:
        glUniformMatrix4fv( glGetUniformLocation( program, name ), 1, GL_FALSE, (float*)param );
        break;
    case mat3x3:
        glUniformMatrix3fv( glGetUniformLocation( program, name ), 1, GL_FALSE, (float*)param );
        break;
    case img:
        glUniform1i( glGetUniformLocation( program, name ), *((int*)param) );
        break;
    }
}

void ShaderProgram::setParameter( shaderAttrib type, void* param, int loc ) const
{
    switch( type )
    {
    case i1:
        glUniform1i( loc, *((int*)param) );
        break;
    case f1:
        glUniform1f( loc, *((float*)param) );
        break;
    case fv3:
        glUniform3fv( loc, 1, (float*)param );
        break;
    case fv4:
        glUniform4fv( loc, 1, (float*)param );
        break;
    case mat4x4:
        glUniformMatrix4fv( loc, 1, GL_FALSE, (float*)param );
        break;
    case mat3x3:
        glUniformMatrix3fv( loc, 1, GL_FALSE, (float*)param );
        break;
    case img:
        glUniform1i( loc, *((int*)param) );
        break;
    }
}

void ShaderProgram::setTexParameter( int idx, const char* name ) const
{
    int loc = glGetUniformLocation( program, name );
    glUniform1i( loc, idx );
}
 
void ShaderProgram::setTexParameter( int idx, int loc ) const
{
    glUniform1i( loc, idx );
}

void ShaderProgram::bindAttribLocation( unsigned int idx, char* name ) const
{
    glBindAttribLocation( program, idx, name );
}

void ShaderProgram::bindFragDataLocation( unsigned int idx, char* name ) const
{
    glBindFragDataLocation( program, idx, name );
}

int ShaderProgram::queryUniformLocation( const char* name ) const
{
    return glGetUniformLocation( program, name );
}

ComputeShader::ComputeShader()
{  
}

ComputeShader::~ComputeShader()
{
}

int ComputeShader::init( const char* cs_source )
{
    //load shader sources and compile
    shaders_t shaderSet = loadShaders( NULL, NULL, NULL, cs_source );
   
    //create program
    program = glCreateProgram();

    //attach shader 
    attachAndLinkProgram( program, shaderSet );

    return 0;
}

}