// Lens Blur
// 
// University of Pennsylvania CIS660 final project
// copyright (c) 2014 Cheng-Tso Lin  

#define GLM_SWIZZLE
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "shader.h"
#include "glRoutine.h"
#include "objLoader.h"
#include "camera.h"
#include "variables.h"
#include "util.h"

using namespace std;
using namespace glm;

const int NUM_RENDERTARGET = 3;

mat4 modelview;
mat4 model;
mat4 view;
mat4 projection;
mat4 normalMat;


vec3 eyePos = vec3(0,0,0 );
vec3 eyeLook = vec3(-1,0,-1);
vec3 upDir = vec3(0,1,0);
Camera cam( eyePos, eyeLook, upDir );

float FOV = 60.0f;
float zNear = 0.05f;
float zFar = 10.0f;
float aspect;

//lighting
Light light1;

//Use this matrix to shift the shadow map coordinate
glm::mat4 biasMatrix(
0.5, 0.0, 0.0, 0.0,
0.0, 0.5, 0.0, 0.0,
0.0, 0.0, 0.5, 0.0,
0.5, 0.5, 0.5, 1.0
);

//Shader program - deferred shading
shader::ShaderProgram passProgram;
shader::ShaderProgram deferredProgram;

//Shader program - Shadow map 
shader::ShaderProgram shadowmapProgram;

//Shader program - depth peeling
shader::ShaderProgram dpProgram;

//Shader uniform locations
map<string,int> passProgramUniformLocations;
map<string,int> deferredProgramUniformLocations;
map<string,int> shadowmapProgramUniformLocations;
map<string,int> dpProgramUniformLocations;

//OpenGL buffer objects for loaded mesh
GLuint vbo[10] = {0}; 
GLuint nbo[10] = {0};
GLuint tbo[10] = {0};
GLuint ibo[10] = {0};
GLuint vao[10] = {0};

const int QUAD = 8;
const int VOXEL3DTEX = 9;
int depth_peeling_layer = 2;

//Textures for deferred shading
GLuint depthFBTex = 0;
GLuint normalFBTex = 0;
GLuint positionFBTex = 0;
GLuint colorFBTex = 0;
GLuint postFBTex = 0;

//Textures for shadowmaping
GLuint shadowmapTex = 0;

//Depth textures for depth peeling
const int NUM_DETPH_PEELING_BUF = 2;
const int NUM_DEPTH_LAYER_TEXTURE = 1;

GLuint layerDepthFBTex[NUM_DETPH_PEELING_BUF]; //Firs two are used to to do peeling iteratively
GLuint layerDepthStorageTex[NUM_DEPTH_LAYER_TEXTURE]; //These are used to store a copy of each layer

//Framebuffer objects
GLuint FBO[2] = {0}; //
GLuint layerFBO[NUM_DETPH_PEELING_BUF] = {0}; //FBO used for depth peeling

enum RenderMode render_mode = RENDERSCENE; 
enum Display display_type = DISPLAY_NORMAL; 

void glut_display()
{
    depthPeeling();
    renderScene();

    glutSwapBuffers();
}

void glut_idle()
{
    glutPostRedisplay();
}

void glut_reshape( int w, int h )
{
    if( h == 0 || w == 0 )
        return;

    g_width = w;
    g_height = h;
    glViewport( 0, 0, w, h );
    glBindFramebuffer(GL_FRAMEBUFFER,0);

    if (FBO[0] != 0 || depthFBTex != 0 || normalFBTex != 0 ) {
        freeFBO();
    }
  
    initFBO(w,h);

    projection = glm::perspective( FOV, (float)w/(float)h, zNear, zFar );
    //projection = glm::ortho( -1.0f, 1.0f, -1.0f, 1.0f, zNear, zFar );
}

int mouse_buttons = 0;
int mouse_old_x = 0;
int mouse_old_y = 0;

void glut_mouse( int button, int state, int x, int y )
{
    if (state == GLUT_DOWN) 
    {
        mouse_buttons |= 1<<button; 
    } 
    else if (state == GLUT_UP) 
    {
        mouse_buttons = 0;
    }

    mouse_old_x = x;
    mouse_old_y = y;
}

void glut_motion( int x, int y )
{
    float dx, dy;
    dx = (float)(x - mouse_old_x);
    dy = (float)(y - mouse_old_y);

    if (mouse_buttons & 1<<GLUT_RIGHT_BUTTON) {
        cam.adjust(0,0,dx,0,0,0);;
    }
    else {
        cam.adjust(-dx*0.2f,-dy*0.2f,0.0f,0,0,0);
    }

    mouse_old_x = x;
    mouse_old_y = y;
}

void glut_keyboard( unsigned char key, int x, int y )
{
    float tx = 0;
    float ty = 0;
    float tz = 0;
    switch(key) {
        case (27):
            exit(0);
            break;
        case '1':
            //show depth layer 1
            depth_peeling_layer = 1;
            cout<<"depth layer: "<<depth_peeling_layer<<"\n";
            break;
        case '2':
            //show depth layer 2
            depth_peeling_layer = 2;
            cout<<"depth layer: "<<depth_peeling_layer<<"\n";
            break;
        case '3':
            depth_peeling_layer = 3;
           cout<<"depth layer: "<<depth_peeling_layer<<"\n";
            //show depth layer 3
            break;
        case ('w'):
            tz = -0.01f;
            break;
        case ('s'):
            tz = 0.01f;
            break;
        case ('d'):
            tx = -0.01f;
            break;
        case ('a'):
            tx = 0.01f;
            break;
        case ('q'):
            ty = 0.01f;
            break;
        case ('z'):
            ty = -0.01f;
            break;
        case 'r':
            initShader();
            break;
   
    }

    if (abs(tx) > 0 ||  abs(tz) > 0 || abs(ty) > 0) {
        cam.adjust(0,0,0,tx,ty,tz);
       
    }
}

void renderScene()
{
    bindFBO(0);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LESS );
    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );
    glEnable(GL_TEXTURE_2D);

    //PASS 1: render scene attributes to textures
    view = cam.get_view();
    model = mat4(1.0);
    modelview = cam.get_view();
    normalMat = transpose( inverse( modelview ) );

    vec4 lightPos = modelview * light1.pos;
    passProgram.use();
    //passProgram.setParameter( shader::f1, (void*)&zFar, "u_Far" );  
    //passProgram.setParameter( shader::f1, (void*)&zNear, "u_Near" );
    //passProgram.setParameter( shader::mat4x4, (void*)&model[0][0], "u_Model" );
    //passProgram.setParameter( shader::mat4x4, (void*)&view[0][0], "u_View" );
    //passProgram.setParameter( shader::mat4x4, (void*)&projection[0][0], "u_Persp" );
    //passProgram.setParameter( shader::mat4x4, (void*)&normalMat[0][0], "u_InvTrans" );
    passProgram.setParameter( shader::f1, (void*)&zFar, passProgramUniformLocations.find("u_Far")->second );
    passProgram.setParameter( shader::f1, (void*)&zNear, passProgramUniformLocations.find("u_Near")->second );
    passProgram.setParameter( shader::mat4x4, (void*)&model[0][0], passProgramUniformLocations.find("u_Model")->second );
    passProgram.setParameter( shader::mat4x4, (void*)&view[0][0], passProgramUniformLocations.find("u_View")->second );
    passProgram.setParameter( shader::mat4x4, (void*)&projection[0][0], passProgramUniformLocations.find("u_Persp")->second );
    passProgram.setParameter( shader::mat4x4, (void*)&normalMat[0][0], passProgramUniformLocations.find("u_InvTrans")->second );

    int bTextured;
    int numModel = g_meshloader.getModelCount();
    for( int i = 0; i < numModel; ++i )
    {
        glBindVertexArray( vao[i] );
        const ObjModel* model = g_meshloader.getModel(i);

        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo[i] );
        for( int i = 0; i < model->numGroup; ++i )
        {
            model->groups[i].shininess = 50;
            //passProgram.setParameter( shader::fv3, &model->groups[i].kd, "u_Color" );
            //passProgram.setParameter( shader::f1, &model->groups[i].shininess, "u_shininess" );
            passProgram.setParameter( shader::fv3, (void*)&model->groups[i].kd, passProgramUniformLocations.find("u_Color")->second );
            passProgram.setParameter( shader::f1, (void*)&model->groups[i].shininess, passProgramUniformLocations.find("u_shininess")->second );
            if( model->groups[i].texId > 0 )
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, model->groups[i].texId );
                //passProgram.setTexParameter( 0, "u_colorTex" );
                passProgram.setTexParameter( 0, passProgramUniformLocations.find( "u_colorTex" )->second );
                bTextured = 1;
            }
            else
                bTextured = 0;
            //passProgram.setParameter( shader::i1, &bTextured, "u_bTextured" );
            passProgram.setParameter( shader::i1, &bTextured, passProgramUniformLocations.find( "u_bTextured" )->second );

            if( model->groups[i].bumpTexId > 0 )
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, model->groups[i].bumpTexId );
                //passProgram.setTexParameter( 1, "u_bumpTex" );
                passProgram.setTexParameter( 1, passProgramUniformLocations.find( "u_bumpTex" )->second  );
                bTextured = 1;
            }
            else
                bTextured = 0;
            //passProgram.setParameter( shader::i1, &bTextured, "u_bBump" );
            passProgram.setParameter( shader::i1, &bTextured, passProgramUniformLocations.find( "u_bBump" )->second );

            glDrawElements( GL_TRIANGLES, 3*model->groups[i].numTri , GL_UNSIGNED_INT, (void*)model->groups[i].ibo_offset );
            
        }
    }
      

   
    //PASS 2: shadow map generation
    renderShadowMap( light1 );

    //PASS 3: shading
    deferredProgram.use();

    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer( GL_FRAMEBUFFER, 0);
   
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

    mat4 persp = perspective(45.0f,(float)g_width/(float)g_height,zNear,zFar);
    vec4 test(-2,0,10,1);
    vec4 testp = persp * test;
    vec4 testh = testp / testp.w;
    vec2 coords = vec2(testh.x, testh.y) / 2.0f + 0.5f;

    mat4 biasLightMVP = biasMatrix * light1.mvp;
    deferredProgram.setParameter( shader::mat4x4, &biasLightMVP[0][0], deferredProgramUniformLocations.find("u_lightMVP")->second );
    deferredProgram.setParameter( shader::mat4x4, &projection[0][0], deferredProgramUniformLocations.find("u_persp")->second );
    deferredProgram.setParameter( shader::mat4x4, &modelview[0][0], deferredProgramUniformLocations.find("u_modelview")->second );

    deferredProgram.setParameter( shader::i1, &g_height, deferredProgramUniformLocations.find("u_ScreenHeight")->second );
    deferredProgram.setParameter( shader::i1, &g_width, deferredProgramUniformLocations.find( "u_ScreenWidth")->second );
    deferredProgram.setParameter( shader::f1, &zFar, deferredProgramUniformLocations.find("u_Far")->second );
    deferredProgram.setParameter( shader::f1, &zNear, deferredProgramUniformLocations.find("u_Near")->second );
    deferredProgram.setParameter( shader::i1, &display_type, deferredProgramUniformLocations.find("u_DisplayType")->second );

    eyePos = cam.get_pos();

    deferredProgram.setParameter( shader::fv4, &lightPos[0], deferredProgramUniformLocations.find("u_Light")->second );
    deferredProgram.setParameter( shader::fv3, &light1.color[0], deferredProgramUniformLocations.find("u_LightColor")->second );
    deferredProgram.setParameter( shader::fv3, &eyePos[0], deferredProgramUniformLocations.find("u_eyePos")->second );

    glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, depthFBTex);
    //deferredProgram.setTexParameter( 0, deferredProgramUniformLocations.find("u_Depthtex")->second );
    glBindTexture(GL_TEXTURE_2D, layerDepthFBTex[depth_peeling_layer%2] );
    deferredProgram.setTexParameter( 0, deferredProgramUniformLocations.find("u_Depthtex")->second );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalFBTex);
    deferredProgram.setTexParameter( 1, deferredProgramUniformLocations.find("u_Normaltex")->second );

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, positionFBTex);
    deferredProgram.setTexParameter( 2, deferredProgramUniformLocations.find("u_Positiontex")->second );

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, colorFBTex);
    deferredProgram.setTexParameter( 3, deferredProgramUniformLocations.find("u_Colortex")->second );

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadowmapTex);
    deferredProgram.setTexParameter( 4, deferredProgramUniformLocations.find("u_shadowmap")->second );

    //Draw the screen space quad
    glBindVertexArray( vao[QUAD] );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo[QUAD] );

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,0);

    glBindVertexArray(0);
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

    glEnable(GL_DEPTH_TEST);

    
}

void renderShadowMap( Light &light )
{
    glBindFramebuffer( GL_FRAMEBUFFER, FBO[1] );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glDisable( GL_CULL_FACE );
    glPolygonOffset(1.1, 4.0);
    shadowmapProgram.use();

    mat4 depthProj = glm::perspective<float>(60, 1, zNear, zFar );
    mat4 depthView = glm::lookAt(  vec3(light.pos), vec3(0,0,0), vec3( 1,0,0) );
    mat4 depthModel = mat4(1.0);
    light.mvp = depthProj * depthView * depthModel;

    shadowmapProgram.setParameter( shader::mat4x4, &light.mvp[0][0], shadowmapProgramUniformLocations.find("u_mvp")->second );

    int bTextured;
    int numModel = g_meshloader.getModelCount();
    for( int i = 0; i < numModel; ++i )
    {
        glBindVertexArray( vao[i] );
        const ObjModel* model = g_meshloader.getModel(i);

         glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo[i] );
        for( int i = 0; i < model->numGroup; ++i )
        {
            model->groups[i].shininess = 50;
            //shadowmapProgram.setParameter( shader::fv3, &model->groups[i].kd, shadowmapProgramUniformLocations.find("u_Color")->second );
            //shadowmapProgram.setParameter( shader::f1, &model->groups[i].shininess, shadowmapProgramUniformLocations.find("u_shininess")->second );

            if( model->groups[i].texId > 0 )
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, model->groups[i].texId );
                //shadowmapProgram.setTexParameter( 0, shadowmapProgramUniformLocations.find("u_colorTex")->second );
                bTextured = 1;
            }
            else
                bTextured = 0;

            //shadowmapProgram.setParameter( shader::i1, &bTextured, shadowmapProgramUniformLocations.find("u_bTextured")->second );

            if( model->groups[i].bumpTexId > 0 )
            {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, model->groups[i].bumpTexId );

                //shadowmapProgram.setTexParameter( 1, shadowmapProgramUniformLocations.find("u_bumpTex")->second );
                bTextured = 1;
            }
            else
                bTextured = 0;

            //shadowmapProgram.setParameter( shader::i1, &bTextured, shadowmapProgramUniformLocations.find("u_bBump")->second );

            glDrawElements( GL_TRIANGLES, 3*model->groups[i].numTri , GL_UNSIGNED_INT, (void*)model->groups[i].ibo_offset );
            
        }
    }
    glPolygonOffset(0,0);
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void depthPeeling()
{
    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LESS );
    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );

    modelview = cam.get_view();
    normalMat = transpose( inverse( modelview ) );
    mat4 mvp = projection * modelview;

    dpProgram.use();
    dpProgram.setParameter( shader::mat4x4, &mvp[0][0], dpProgramUniformLocations.find("u_mvp")->second );
    dpProgram.setParameter( shader::i1, &g_height, dpProgramUniformLocations.find("u_ScreenHeight")->second );
    dpProgram.setParameter( shader::i1, &g_width, dpProgramUniformLocations.find( "u_ScreenWidth")->second );

    for( int n = 0; n < depth_peeling_layer; ++n )
    {
        //Alternate the depth texture
        //depth texture (n+1)%2 acts as a ordinary depth buffer
        glBindFramebuffer( GL_FRAMEBUFFER, layerFBO[ (n+1)%2 ] );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        dpProgram.setParameter( shader::i1, &n, dpProgramUniformLocations.find("u_peelingPass")->second );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture( GL_TEXTURE_2D, layerDepthFBTex[n%2] ); //Depth texture n%2 is used to peeling off the current nearest layer
        dpProgram.setTexParameter( 0, dpProgramUniformLocations.find( "u_nearestDepthTex" )->second  );

        int bTextured;
        int numModel = g_meshloader.getModelCount();
        for( int i = 0; i < numModel; ++i )
        {

            glBindVertexArray( vao[i] );
            const ObjModel* model = g_meshloader.getModel(i);

            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo[i] );
            for( int i = 0; i < model->numGroup; ++i )
            {
                model->groups[i].shininess = 50;
                //dpProgram.setParameter( shader::fv3, (void*)&model->groups[i].kd, dpProgramUniformLocations.find("u_Color")->second );
                //dpProgram.setParameter( shader::f1, (void*)&model->groups[i].shininess, dpProgramUniformLocations.find("u_shininess")->second );
                if( model->groups[i].texId > 0 )
                {
                    //glActiveTexture(GL_TEXTURE0);
                    //glBindTexture(GL_TEXTURE_2D, model->groups[i].texId );                    
                    //dpProgram.setTexParameter( 0, dpProgramUniformLocations.find( "u_colorTex" )->second );
                    bTextured = 1;
                }
                else
                    bTextured = 0;
               
                //dpProgram.setParameter( shader::i1, &bTextured, dpProgramUniformLocations.find( "u_bTextured" )->second );

                if( model->groups[i].bumpTexId > 0 )
                {
                    //glActiveTexture(GL_TEXTURE1);
                    //glBindTexture(GL_TEXTURE_2D, model->groups[i].bumpTexId );
                    
                    //dpProgram.setTexParameter( 1, dpProgramUniformLocations.find( "u_bumpTex" )->second  );
                    bTextured = 1;
                }
                else
                    bTextured = 0;
                
                //dpProgram.setParameter( shader::i1, &bTextured, dpProgramUniformLocations.find( "u_bBump" )->second );

                glDrawElements( GL_TRIANGLES, 3*model->groups[i].numTri , GL_UNSIGNED_INT, (void*)model->groups[i].ibo_offset );
            
            }
        }
        glBindTexture( GL_TEXTURE_2D, 0 );
        glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    }
 
    dpProgram.unuse();
}

void initFBO( int w, int h )
{
    GLenum FBOstatus;

    glActiveTexture(GL_TEXTURE9);

    glGenTextures(1, &depthFBTex);
    glGenTextures(1, &normalFBTex);
    glGenTextures(1, &positionFBTex);
    glGenTextures(1, &colorFBTex);

    //Set up depth FBO
    depthFBTex = gen2DTexture( w, h, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT );

    //Set up normal FBO
    normalFBTex = gen2DTexture( w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT );

    //Set up position FBO
    positionFBTex = gen2DTexture( w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT );

    //Set up color FBO
    colorFBTex = gen2DTexture( w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT );

    // create a framebuffer object
    glGenFramebuffers(1, &FBO[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO[0]);

    // Instruct openGL that we won't bind a color texture with the currently bound FBO
    //glReadBuffer(GL_NONE);

    GLenum draws [NUM_RENDERTARGET];
    draws[0] = GL_COLOR_ATTACHMENT0;
    draws[1] = GL_COLOR_ATTACHMENT1;
    draws[2] = GL_COLOR_ATTACHMENT2;

    glDrawBuffers(NUM_RENDERTARGET, draws);

    // attach the texture to FBO depth attachment point
    glBindTexture(GL_TEXTURE_2D, depthFBTex);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthFBTex, 0);

    glBindTexture(GL_TEXTURE_2D, normalFBTex);    
    glFramebufferTexture(GL_FRAMEBUFFER, draws[0], normalFBTex, 0);

    glBindTexture(GL_TEXTURE_2D, positionFBTex);    
    glFramebufferTexture(GL_FRAMEBUFFER, draws[1], positionFBTex, 0);

    glBindTexture(GL_TEXTURE_2D, colorFBTex);    
    glFramebufferTexture(GL_FRAMEBUFFER, draws[2], colorFBTex, 0);

    
    // check FBO status
    FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("GL_FRAMEBUFFER_COMPLETE failed, CANNOT use FBO[0]\n");
        checkFramebufferStatus(FBOstatus);
    }

   
    //Shadow map buffers
    shadowmapTex = gen2DTexture( 1024, 1024, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT );

    // create a framebuffer object for shadow mapping
    glGenFramebuffers(1, &FBO[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO[1]);

    glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowmapTex, 0 );
    glDrawBuffer( GL_NONE ); //Disable render

    // check FBO status
    FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("GL_FRAMEBUFFER_COMPLETE failed, CANNOT use FBO[1]\n");
        checkFramebufferStatus(FBOstatus);
    }

    //Generate textures and FBO for depth peeling
    for( int i = 0; i < NUM_DETPH_PEELING_BUF; ++i )
    {
        layerDepthFBTex[i] = gen2DTexture( w, h, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT );
    }

    for( int i = 0; i < NUM_DEPTH_LAYER_TEXTURE; ++i )
    {
        layerDepthStorageTex[i] = gen2DTexture( w, h, GL_RGBA32F, GL_RGBA, GL_FLOAT );
    }
    glBindTexture(GL_TEXTURE_2D, 0 );

    glGenFramebuffers( 2, layerFBO );
    glBindFramebuffer( GL_FRAMEBUFFER, layerFBO[0] );
    glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, layerDepthFBTex[0], 0 );

    draws[0] = GL_COLOR_ATTACHMENT0;

    glDrawBuffers(NUM_RENDERTARGET, draws);
    glDrawBuffers( GL_NONE ); //Disable render

    // check FBO status
    FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("GL_FRAMEBUFFER_COMPLETE failed, CANNOT use layerFB[0]\n");
        checkFramebufferStatus(FBOstatus);
    }

    glBindFramebuffer( GL_FRAMEBUFFER, layerFBO[1] );
    glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, layerDepthFBTex[1], 0 );
    glDrawBuffer( GL_NONE ); //Disable render

    // check FBO status
    FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("GL_FRAMEBUFFER_COMPLETE failed, CANNOT use layerFB[1]\n");
        checkFramebufferStatus(FBOstatus);
    }

    // switch back to window-system-provided framebuffer
    glClear( GL_DEPTH_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void freeFBO() 
{
    glDeleteTextures(1,&depthFBTex);
    glDeleteTextures(1,&normalFBTex);
    glDeleteTextures(1,&positionFBTex);
    glDeleteTextures(1,&colorFBTex);

    glDeleteFramebuffers(2, FBO);
  
    glDeleteTextures( NUM_DETPH_PEELING_BUF, layerDepthFBTex );
    glDeleteFramebuffers( NUM_DETPH_PEELING_BUF, layerFBO );

    glDeleteTextures( NUM_DEPTH_LAYER_TEXTURE, layerDepthStorageTex );
}

void bindFBO(int buf)
{
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,0); //Bad mojo to unbind the framebuffer using the texture
    glBindFramebuffer(GL_FRAMEBUFFER, FBO[buf]);
    glClear(GL_DEPTH_BUFFER_BIT);
    //glColorMask(false,false,false,false);
    glEnable(GL_DEPTH_TEST);
}

void setTextures() 
{
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,0); 
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //glColorMask(true,true,true,true);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
}

void initShader()
{
    passProgram.init( "shader/pass.vert.glsl", "shader/pass.frag.glsl" );
    passProgramUniformLocations.insert( pair<string,int>( "u_Far", passProgram.queryUniformLocation( "u_Far" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_Near", passProgram.queryUniformLocation( "u_Near" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_Model", passProgram.queryUniformLocation( "u_Model" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_View", passProgram.queryUniformLocation( "u_View" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_Persp", passProgram.queryUniformLocation( "u_Persp" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_InvTrans", passProgram.queryUniformLocation( "u_InvTrans" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_Color", passProgram.queryUniformLocation( "u_Color" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_shininess", passProgram.queryUniformLocation( "u_shininess" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_bTextured", passProgram.queryUniformLocation( "u_bTextured" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_bumpTex", passProgram.queryUniformLocation( "u_bumpTex" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_colorTex", passProgram.queryUniformLocation( "u_colorTex" ) ) );
    passProgramUniformLocations.insert( pair<string,int>( "u_bBump", passProgram.queryUniformLocation( "u_bBump" ) ) );

    deferredProgram.init( "shader/shade.vert.glsl", "shader/diagnostic.frag.glsl" );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_lightMVP", deferredProgram.queryUniformLocation("u_lightMVP") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_persp", deferredProgram.queryUniformLocation("u_persp") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_modelview", deferredProgram.queryUniformLocation("u_modelview") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_ScreenHeight", deferredProgram.queryUniformLocation("u_ScreenHeight") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_ScreenWidth", deferredProgram.queryUniformLocation("u_SCreenWidth") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Far", deferredProgram.queryUniformLocation("u_Far") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Near", deferredProgram.queryUniformLocation("u_Near") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_DisplayType", deferredProgram.queryUniformLocation("u_DisplayType") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Light", deferredProgram.queryUniformLocation("u_Light") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_LightColor", deferredProgram.queryUniformLocation("u_LightColor") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_eyePos", deferredProgram.queryUniformLocation("u_eyePos") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Depthtex", deferredProgram.queryUniformLocation("u_Depthtex") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Normaltex", deferredProgram.queryUniformLocation("u_Normaltex") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Positiontex", deferredProgram.queryUniformLocation("u_Positiontex") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_Colortex", deferredProgram.queryUniformLocation("u_Colortex") ) );
    deferredProgramUniformLocations.insert( pair<string,int>( "u_shadowmap", deferredProgram.queryUniformLocation("u_shadowmap") ) );

    shadowmapProgram.init( "shader/shadowmap.vert.glsl", "shader/shadowmap.frag.glsl" );
    shadowmapProgramUniformLocations.insert( pair<string,int>( "u_mvp", shadowmapProgram.queryUniformLocation("u_mvp") ) );

    dpProgram.init( "shader/depthPeeling.vert.glsl", "shader/depthPeeling.frag.glsl" );
    dpProgramUniformLocations.insert( pair<string,int>( "u_mvp", dpProgram.queryUniformLocation("u_mvp") ) );
    dpProgramUniformLocations.insert( pair<string,int>( "u_nearestDepthTex", dpProgram.queryUniformLocation("u_nearestDepthTex") ) );
    dpProgramUniformLocations.insert( pair<string,int>( "u_peelingPass", dpProgram.queryUniformLocation("u_peelingPass") ) );
    dpProgramUniformLocations.insert( pair<string,int>( "u_ScreenWidth", dpProgram.queryUniformLocation("u_ScreenWidth") ) );
    dpProgramUniformLocations.insert( pair<string,int>( "u_ScreenHeight", dpProgram.queryUniformLocation("u_ScreenHeight") ) );

}

void initVertexData()
{
    //Send loaded models to GPU
    int numModel = g_meshloader.getModelCount();
    for( int i = 0; i < numModel; ++i )
    {
        glGenVertexArrays( 1, &vao[i] );
        glBindVertexArray( vao[i] );
        const ObjModel* model = g_meshloader.getModel(i);

        glGenBuffers( 1, &vbo[i] );
        glBindBuffer( GL_ARRAY_BUFFER, vbo[i] );
        glBufferData( GL_ARRAY_BUFFER, sizeof(float) * 3 * model->numVert, model->vbo, GL_STATIC_DRAW  );
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 );
        glEnableVertexAttribArray( 0 );

        if( model->numNrml > 0 )
        {
            glGenBuffers( 1, &nbo[i] );
            glBindBuffer( GL_ARRAY_BUFFER, nbo[i] );
            glBufferData( GL_ARRAY_BUFFER, sizeof(float) * 3 * model->numVert, model->nbo, GL_STATIC_DRAW  );
            glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 0, 0 );
            glEnableVertexAttribArray( 1 );
        }
        if( model->numTxcoord > 0 )
        {
            glGenBuffers( 1, &tbo[i] );
            glBindBuffer( GL_ARRAY_BUFFER, tbo[i] );
            glBufferData( GL_ARRAY_BUFFER, sizeof(float) * 2 * model->numVert, model->tbo, GL_STATIC_DRAW  );
            glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 0, 0 );
            glEnableVertexAttribArray( 2 );
        }

        glGenBuffers( 1, &ibo[i] );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo[i] );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( unsigned int ) * model->numIdx, model->ibo, GL_STATIC_DRAW );

        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
        glBindBuffer( GL_ARRAY_BUFFER,0 );
        glBindVertexArray(0);

        //Upload texture to GPU 
        for( int i = 0; i < model->numGroup; ++i )
        {
            if( model->groups[i].tex_filename.length() > 0 )
                model->groups[i].texId = loadTexturFromFile( model->groups[i].tex_filename.c_str(), GL_RGB8, GL_BGR, 2 );
            else
                model->groups[i].texId = 0;
            if( model->groups[i].bump_filename.length() > 0 )
                model->groups[i].bumpTexId = loadTexturFromFile( model->groups[i].bump_filename.c_str(), GL_RGB8, GL_BGR, 0 );
            else
                model->groups[i].bumpTexId = 0;
        }
    }
   
    //create a screen-size quad for deferred shading
    createScreenQuad();

    
}

unsigned int genFBO( unsigned int* depthAttch, unsigned int* attch0, unsigned int* attch1, unsigned int* attch2, unsigned int* attch3 )
{
    GLuint fbo;
    GLenum FBOstatus;

    int numAttch = 0;

    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );

    if( depthAttch != NULL )
    {
        glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, *depthAttch, 0 );
    }
    if( attch0 != NULL )
    {
        glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *attch0, 0 );
        numAttch++;
    }
    if( attch1 != NULL )
    {
        glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, *attch1, 0 );
        numAttch++;
    }
    if( attch2 != NULL )
    {
        glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, *attch2, 0 );
        numAttch++;
    }
    if( attch3 != NULL )
    {
        glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, *attch3, 0 );
        numAttch++;
    }


    GLenum draws[4];
    draws[0] = GL_COLOR_ATTACHMENT0;
    draws[1] = GL_COLOR_ATTACHMENT1;
    draws[2] = GL_COLOR_ATTACHMENT2;
    draws[3] = GL_COLOR_ATTACHMENT3;

    glDrawBuffers(numAttch, draws);

    // check FBO status
    FBOstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(FBOstatus != GL_FRAMEBUFFER_COMPLETE) {
        printf("GL_FRAMEBUFFER_COMPLETE failed, CANNOT use FBO[0]\n");
        checkFramebufferStatus(FBOstatus);
    }

    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    return fbo;
}

unsigned int gen2DTexture( int w, int h, GLenum internalFormat,  GLenum format, GLenum type )
{
    GLuint texId;
    glGenTextures(1, &texId);
    glBindTexture( GL_TEXTURE_2D, texId );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

    glTexImage2D( GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, 0);

    glBindTexture( GL_TEXTURE_2D, 0 );
    return texId;
}

unsigned int gen3DTexture( int dim )
{
    float* data = new float[dim*dim*dim];
    memset( data, 0, sizeof(float)*dim*dim*dim );

    GLuint texId;
    glGenTextures( 1, &texId );
    glBindTexture( GL_TEXTURE_3D, texId );
    //glActiveTexture(GL_TEXTURE0 );
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D,  GL_TEXTURE_MIN_FILTER, GL_NEAREST);  
    glTexParameteri(GL_TEXTURE_3D,  GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    glTexImage3D( GL_TEXTURE_3D, 0, GL_R8, dim, dim, dim, 0, GL_RED, GL_UNSIGNED_BYTE, data );
    glBindTexture( GL_TEXTURE_3D, 0 );
    GLenum err = glGetError();
    cout<<glewGetErrorString(err)<<" "<<err<<endl;
    delete [] data;
    return texId;
}


//create a screen-size quad
void createScreenQuad()
{
    GLenum err;
    vertex2_t verts [] = { {vec3(-1,1,0),vec2(0,1)},
        {vec3(-1,-1,0),vec2(0,0)},
        {vec3(1,-1,0),vec2(1,0)},
        {vec3(1,1,0),vec2(1,1)}};

    unsigned short indices[] = { 0,1,2,0,2,3};

    glGenVertexArrays( 1, &vao[QUAD] );
    glBindVertexArray( vao[QUAD] );

    //Allocate vbos for data
    glGenBuffers(1,&vbo[QUAD]);
    glGenBuffers(1,&ibo[QUAD]);

    //Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[QUAD]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    //Use of strided data, Array of Structures instead of Structures of Arrays
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,sizeof(vertex2_t),(void*)sizeof(vec3));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    //indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo[QUAD]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(GLushort), indices, GL_STATIC_DRAW);

    //Unplug Vertex Array
    glBindVertexArray(0);

    err = glGetError();
}

void initLight()
{
    light1.pos = light1.initialPos = vec4( 0, 0.43, 0, 1 );
    light1.color = vec3( 1, 1, 1 );
}