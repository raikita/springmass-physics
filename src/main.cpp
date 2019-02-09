
/**
 * Author:	Andrew Robert Owens
 * Email:	arowens [at] ucalgary.ca
 * Date:	January, 2017
 * Course:	CPSC 587/687 Fundamental of Computer Animation
 * Organization: University of Calgary
 *
 * Copyright (c) 2017 - Please give credit to the author.
 *
 * File:	main.cpp
 *
 * Summary:
 *
 * This is a (very) basic program to
 * 1) load shaders from external files, and make a shader program
 * 2) make Vertex Array Object and Vertex Buffer Object for the quad
 *
 * take a look at the following sites for further readings:
 * opengl-tutorial.org -> The first triangle (New OpenGL, great start)
 * antongerdelan.net -> shaders pipeline explained
 * ogldev.atspace.co.uk -> good resource
 *
 * The actual animated stuff written by Rukiya Hassan
 *
 */

#include <iostream>
#include <cmath>
#include <chrono>
#include <limits>
#include <cstdlib>

#include "glad/glad.h"
#include <GLFW/glfw3.h>

#include "ShaderTools.h"
#include "Vec3f.h"
#include "Mat4f.h"
#include "OpenGLMatrixTools.h"
#include "Camera.h"

//==================== GLOBAL VARIABLES ====================//
/*	Put here for simplicity. Feel free to restructure into
*	appropriate classes or abstractions.
*/

// Drawing Program
GLuint basicProgramID;

// Data needed for Quad
GLuint vaoID;
GLuint vertBufferID;
Mat4f M;

// Data needed for Line
GLuint line_vaoID;
GLuint line_vertBufferID;
Mat4f line_M;

// Only one camera so only one veiw and perspective matrix are needed.
Mat4f V;
Mat4f P;

// Only one thing is rendered at a time, so only need one MVP
// When drawing different objects, update M and MVP = M * V * P
Mat4f MVP;

// Camera and veiwing Stuff
Camera camera;
int g_moveUpDown = 0;
int g_moveLeftRight = 0;
int g_moveBackForward = 0;
int g_rotateLeftRight = 0;
int g_rotateUpDown = 0;
int g_rotateRoll = 0;
float g_rotationSpeed = 0.015625;
float g_panningSpeed = 0.25;
bool g_cursorLocked;
float g_cursorX, g_cursorY;

bool g_play = false;

int WIN_WIDTH = 1024, WIN_HEIGHT = 1024;
int FB_WIDTH = 1024, FB_HEIGHT = 1024;
float WIN_FOV = 60;
float WIN_NEAR = 0.01;
float WIN_FAR = 1000;

// My own globals... //
float pi = 3.14159265359f;
float e = 2.7182818f;
Vec3f g = Vec3f(0,-9.81,0);

int view = 1;
bool replay = false;
float ground = -50;
float masswidth = 0.25;

Vec3f wind = Vec3f(0,0,0);
bool increaseX = false;
float windX = 0;

bool DEBUG = false;

//==================== FUNCTION DECLARATIONS ====================//
void displayFunc();
void resizeFunc();
void init();
void generateIDs();
void deleteIDs();
void setupVAO();
void loadQuadGeometryToGPU();
void reloadProjectionMatrix();
void loadModelViewMatrix();
void setupModelViewProjectionTransform();

void windowSetSizeFunc();
void windowKeyFunc(GLFWwindow *window, int key, int scancode, int action,
                   int mods);
void windowMouseMotionFunc(GLFWwindow *window, double x, double y);
void windowSetSizeFunc(GLFWwindow *window, int width, int height);
void windowSetFramebufferSizeFunc(GLFWwindow *window, int width, int height);
void windowMouseButtonFunc(GLFWwindow *window, int button, int action,
                           int mods);
void windowMouseMotionFunc(GLFWwindow *window, double x, double y);
void windowKeyFunc(GLFWwindow *window, int key, int scancode, int action,
                   int mods);
void animatePoints(float t);
void moveCamera();
void reloadMVPUniform();
void reloadColorUniform(float r, float g, float b);
std::string GL_ERROR();
int main(int, char **);

//==================== FUNCTION DEFINITIONS ====================//

struct Mass {
  float mass;
  Vec3f position;
  Vec3f velocity;
  Vec3f force;
  bool fixed;
};

struct Spring {
  Mass *a, *b;
  float stiffness;    // k
  float restLength;   // Xo
  float damping;      // spring damping (not air damping)
};

std::vector<Mass> points;
std::vector<Spring> springs;
std::vector<Vec3f> tempPoints;

float length(Vec3f A, Vec3f B) {
  float x = A.x() - B.x();
  float y = A.y() - B.y();
  float z = A.z() - B.z();

  return sqrt(x*x + y*y + z*z);
}

void displayFunc() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Use our shader
  glUseProgram(basicProgramID);

  // ===== DRAW QUAD ====== //
  MVP = P * V * M;
  reloadMVPUniform();
  reloadColorUniform(0.5, 0, 1);

  // Use VAO that holds buffer bindings
  // and attribute config of buffers
  glBindVertexArray(vaoID);
  // Draw Quads, start at vertex 0, draw 4 of them (for a quad)
  glDrawArrays(GL_TRIANGLES, 0, 6 * points.size());

  // ==== DRAW LINE ===== //
  MVP = P * V * line_M;
  reloadMVPUniform();

  reloadColorUniform(0.2, 0.2, 0.2);

  // Use VAO that holds buffer bindings
  // and attribute config of buffers
  glBindVertexArray(line_vaoID);
  // Draw lines
  glDrawArrays(GL_LINES, 0, 2 * springs.size());

}

void calculateSprings(int i, float dt){

  Vec3f a = (springs[i].a)->position;
  Vec3f b = (springs[i].b)->position;
  float Xo = springs[i].restLength;
  float k = springs[i].stiffness;

  float currentLength = length(b,a);
  Vec3f abUnit = (b-a)/currentLength;

  if (DEBUG == true)
    printf("currentLength = %f \n", currentLength);
  Vec3f Fs = -k * (currentLength-Xo) * abUnit;

  (springs[i].a)->force += -Fs;
  (springs[i].b)->force += Fs;


}

int collision(unsigned int i, Vec3f xtdt){
  float min = -0.005;
  float max = 0.005;
  Vec3f diff;
  for (unsigned j = 0; j < points.size(); j++) {
    if (j != i) {
      diff = xtdt - points[j].position;
      if ((diff.x() < max && diff.x() > min) && (diff.y() < max && diff.y() > min) && (diff.z() < max && diff.z() > min)) {
        return j;
      }
    }
  }
  return -1;
}

void updatePoints(int i, float dt){


  float airDamping = 0.7;
  if (view == 4)
    airDamping = 0.2;

  float mass = points[i].mass;
  Vec3f force = points[i].force;
  Vec3f xta = points[i].position;
  Vec3f vta = points[i].velocity;
  force += (mass * g) + (-vta * airDamping);

  Vec3f ata = force/mass;


  if (DEBUG == true){
    printf("calculated forces: \n");
  }

  // the actual Semi-Implicit Euler switch to verlet later
  Vec3f vtdta = Vec3f(vta.x()+ata.x()*dt, vta.y()+ata.y()*dt, vta.z()+ata.z()*dt);
  Vec3f xtdta = Vec3f(xta.x()+vtdta.x()*dt, xta.y()+vtdta.y()*dt, xta.z()+vtdta.z()*dt);


  if (view == 5) {
    int c = collision(i, xtdta);
    if (c >= 0) {
      vtdta = points[c].velocity;
      xtdta = Vec3f(xta.x()+vtdta.x()*dt, xta.y()+vtdta.y()*dt, xta.z()+vtdta.z()*dt);
    }
  }

  // make it collide with the ground
 if (view == 3)
  {
    float yvalueX = xta.y()+vtdta.y()*dt;
    if (yvalueX <= ground) {
      vtdta = Vec3f(vta.x()+ata.x()*dt, -(vta.y()+ata.y()*dt), vta.z()+ata.z()*dt)/2;
      float yvalueV = vtdta.y();
      if (yvalueV < 1) {
        vtdta = Vec3f(vta.x()+ata.x()*dt, 0, vta.z()+ata.z()*dt);
        xtdta.y() = ground;
      }
      else
        xtdta = Vec3f(xta.x()+vtdta.x()*dt, xta.y()+vtdta.y()*dt, xta.z()+vtdta.z()*dt);
    }
  }
  else if (view == 5) {
    float tableHeight = -30;
    float tableWidth = 50;
    if (xtdta.y() <= tableHeight) {
      if (xtdta.x() >= tableWidth/2 && xtdta.x() <= tableWidth) {
        if (xtdta.z() <= -tableWidth/2 && xtdta.z() >= -tableWidth) {
          vtdta = Vec3f(0,0,0);
          xtdta.y() = tableHeight;
        }
      }
      xtdta = Vec3f(xta.x()+vtdta.x()*dt, xta.y()+vtdta.y()*dt, xta.z()+vtdta.z()*dt);
    }
  }

  if (DEBUG == true){
    printf("mass position = %f %f %f \n", xta.x(), xta.y(), xta.z());
    printf("mass y velocity = %f, acceleration = %f \n", vtdta.y(), ata.y());
  }
  // update all points
  if (!points[i].fixed) {
    points[i].position = xtdta;
    points[i].velocity = vtdta;
  }
  // reset forces
  points[i].force = Vec3f(0,0,0);

}

void animatePoints(float dt) {

  // calculate spring forces
  for (int timestep = 0; timestep < 10; timestep++) {
    for (unsigned i = 0; i < springs.size(); i++) {
      calculateSprings(i, dt);
    }

    // update masses
    for (unsigned i = 0; i < points.size(); i++) {
      updatePoints(i, dt);
    }
  }
}

void setupPoints() {
  // reset points and springs vectors
  if (view == 1)
  {
    points.erase(points.begin(),points.begin()+points.size());
    springs.erase(springs.begin(),springs.begin()+springs.size());
    Mass A;
    A.mass = 0;
    A.position = Vec3f(0,0,0);
    A.velocity = Vec3f(0,0,0);
    A.force = Vec3f(0,0,0);
    A.fixed = true;

    Mass B;
    B.mass = 3;
    B.position = Vec3f(5,0,0);
    B.velocity = Vec3f(0,0,0);
    B.force = Vec3f(0,0,0);
    B.fixed = false;

    points.push_back(A);
    points.push_back(B);

    Spring ABs;
    ABs.a = &points[0];
    ABs.b = &points[1];
    ABs.stiffness = 30;
    ABs.restLength = 5;
    ABs.damping = 0;

    springs.push_back(ABs);
  }
  else if (view == 2)
  {
    points.erase(points.begin(),points.begin()+points.size());
    springs.erase(springs.begin(),springs.begin()+springs.size());
    Mass A;
    A.mass = 0;
    A.position = Vec3f(0,0,0);
    A.velocity = Vec3f(0,0,0);
    A.force = Vec3f(0,0,0);
    A.fixed = true;

    Mass B;
    B.mass = 2;
    B.position = Vec3f(5,0,0);
    B.velocity = Vec3f(0,0,0);
    B.force = Vec3f(0,0,0);
    B.fixed = false;

    Mass C;
    C.mass = 2;
    C.position = Vec3f(10,0,0);
    C.velocity = Vec3f(0,0,0);
    C.force = Vec3f(0,0,0);
    C.fixed = false;

    Mass D;
    D.mass = 2;
    D.position = Vec3f(15,0,0);
    D.velocity = Vec3f(0,0,0);
    D.force = Vec3f(0,0,0);
    D.fixed = false;

    points.push_back(A);
    points.push_back(B);
    points.push_back(C);
    points.push_back(D);

    Spring ABs;
    ABs.a = &points[0];
    ABs.b = &points[1];
    ABs.stiffness = 30;
    ABs.restLength = 5;
    ABs.damping = 0;

    Spring BCs;
    BCs.a = &points[1];
    BCs.b = &points[2];
    BCs.stiffness = 30;
    BCs.restLength = 5;
    BCs.damping = 0;

    Spring CDs;
    CDs.a = &points[2];
    CDs.b = &points[3];
    CDs.stiffness = 30;
    CDs.restLength = 5;
    CDs.damping = 0;

    springs.push_back(ABs);
    springs.push_back(BCs);
    springs.push_back(CDs);
  }
  else if (view == 3)
  {
    points.erase(points.begin(),points.begin()+points.size());
    springs.erase(springs.begin(),springs.begin()+springs.size());
    float weight = 0.5;
    float k = 10;

    // set up masses
    // front
    int x, y, z;
    x = y = z = 0;
    for (unsigned i = 0; i < 9; i++) {
      Mass m;
      m.mass = weight;
      m.position = Vec3f(x, y, z);
      m.velocity = Vec3f(0,0,0);
      m.force = Vec3f(0,0,0);
      m.fixed = false;

      x += 5;
      // every third point go down 5
      if ((i+1) % 3 == 0) {
        y -= 5;
        x = 0;
      }
      points.push_back(m);
    }

    // middle
    x = y = 0;
    z += -5;
    for (unsigned i = 0; i < 9; i++) {
      Mass m;
      m.mass = weight;
      m.position = Vec3f(x, y, z);
      m.velocity = Vec3f(0,0,0);
      m.force = Vec3f(0,0,0);
      m.fixed = false;

      x += 5;
      // every third point go down 5
      if ((i+1) % 3 == 0) {
        y -= 5;
        x = 0;
      }
      points.push_back(m);
    }

    // back
    x = y = 0;
    z += -5;
    for (unsigned i = 0; i < 9; i++) {
      Mass m;
      m.mass = weight;
      m.position = Vec3f(x, y, z);
      m.velocity = Vec3f(0,0,0);
      m.force = Vec3f(0,0,0);
      m.fixed = false;

      x += 5;
      // every third point go down 5
      if ((i+1) % 3 == 0) {
        y -= 5;
        x = 0;
      }
      points.push_back(m);
    }

    // set up springs
    int a = 0;
    for (unsigned n = 0; n < 3; n++) {
      for (unsigned j = 0; j < 9; j++) {
        // horizontal -->
        if ((a+1)%3 != 0) { // if not on right edge make horizontal spring (pointing right)
          Spring sh;
          sh.a = &points[a];
          sh.b = &points[a+1];
          sh.stiffness = k;
          sh.restLength = 5;
          sh.damping = 0;

          springs.push_back(sh);
        }
        // vertical ^
        if (a <= 23 && a >= 18) {   // if not on bottom edge make vertical spring (pointing up)
          Spring sv;
          sv.a = &points[a];
          sv.b = &points[a+3];
          sv.stiffness = k;
          sv.restLength = 5;
          sv.damping = 0;
          springs.push_back(sv);
        }
        else if (a <= 14 && a >= 9) {   // if not on bottom edge make vertical spring (pointing up)
          Spring sv;
          sv.a = &points[a];
          sv.b = &points[a+3];
          sv.stiffness = k;
          sv.restLength = 5;
          sv.damping = 0;
          springs.push_back(sv);
        }
        else if (a <= 5) {   // if not on bottom edge make vertical spring (pointing up)
          Spring sv;
          sv.a = &points[a];
          sv.b = &points[a+3];
          sv.stiffness = k;
          sv.restLength = 5;
          sv.damping = 0;
          springs.push_back(sv);
        }
        a++;
      }
    }

    // connect the three massive masses with more springs
    for (unsigned j = 0; j < 26; j++) {
      if (j <= 17) {   // if not on bottom edge make vertical spring (pointing up)
        Spring sv;
        sv.a = &points[j];
        sv.b = &points[j+9];
        sv.stiffness = k;
        sv.restLength = 5;
        sv.damping = 0;
        springs.push_back(sv);
      }

      // down right
      for (unsigned i = 0; i < 23; i++) {
        Spring sv;
        if (((i >= 0 && i < 5)|| (i > 8 && i < 14) || (i > 17  && i < 23)) && (i != 2 && i != 11 && i != 20)) {
          sv.a = &points[i];
          sv.b = &points[i+4];
          sv.stiffness = k-2;
          sv.restLength = sqrt(50);
          sv.damping = 0;
          springs.push_back(sv);
        }
      }

      // down left
      for (unsigned i = 0; i < 24; i++) {
        Spring sv;
        if (((i > 0 && i < 6)|| (i > 9 && i < 15) || (i > 18  && i < 24)) && (i != 3 && i != 12 && i != 21)) {
          sv.a = &points[i];
          sv.b = &points[i+2];
          sv.stiffness = k-2;
          sv.restLength = sqrt(50);
          sv.damping = 0;
          springs.push_back(sv);
        }
      }

    }
  }

  else if (view == 4) {
    points.erase(points.begin(),points.begin()+points.size());
    springs.erase(springs.begin(),springs.begin()+springs.size());

    float k = 50;
    float pointMass = 0.5;
    float restLength = 2;
    unsigned int clothLength = 50;
    unsigned int clothWidth = 50;
    float x, y, z;
    x = y = z = 0;
    for (unsigned j = 0; j < clothWidth; j++) {
      for (unsigned i = 0; i < clothLength; i++) {
        Mass m;
        m.mass = pointMass;
        m.position = Vec3f(x,y+x,z);
        m.velocity = Vec3f(0,0,0);
        m.force = Vec3f(0,0,0);
        if (y == 0)
          m.fixed = true;
        else
          m.fixed = false;

        points.push_back(m);
        x += restLength;
        z -= 0.1;
      }
      y -= restLength;
      x = 0;
    }

    // work with length
    for (unsigned j = 0; j < clothWidth*clothLength-1; j++) {
      if (points[j].position.x() != clothLength*restLength-restLength) {
        Spring sh;
        sh.a = &points[j];
        sh.b = &points[j+1];
        sh.stiffness = k;
        sh.restLength = restLength;
        sh.damping = 0;
        springs.push_back(sh);

        // work with down right
        if (j < clothWidth*clothLength-clothLength){
          Spring s;
          s.a = &points[j];
          s.b = &points[j+clothLength+1];
          s.stiffness = k-5;
          s.restLength = sqrt((restLength*restLength)*2);
          s.damping = 0;
          springs.push_back(s);
        }
      }

      // work with down left
      if (j < clothWidth*clothLength-clothLength && j%clothLength != 0 ){
        Spring s;
        s.a = &points[j];
        s.b = &points[j+clothLength-1];
        s.stiffness = k-5;
        s.restLength = sqrt((restLength*restLength)*2);
        s.damping = 0;
        springs.push_back(s);
      }
    }

    // work with width
    for (unsigned j = 0; j < clothWidth*clothLength-clothLength; j++) {
      Spring s;
      s.a = &points[j];
      s.b = &points[j+clothLength];
      s.stiffness = k;
      s.restLength = restLength;
      s.damping = 0;
      springs.push_back(s);
    }
    // end part 4
  }

  else if (view == 5) {
    points.erase(points.begin(),points.begin()+points.size());
    springs.erase(springs.begin(),springs.begin()+springs.size());

    float k = 50;
    float pointMass = 0.5;
    float restLength = 2;
    unsigned int clothLength = 50;
    unsigned int clothWidth = 50;
    float x, y, z;
    x = y = z = 0;
    for (unsigned j = 0; j < clothWidth; j++) {
      for (unsigned i = 0; i < clothLength; i++) {
        Mass m;
        m.mass = pointMass;
        m.position = Vec3f(x,y,z);
        m.velocity = Vec3f(0,0,0);
        m.force = Vec3f(0,0,0);
        m.fixed = false;

        points.push_back(m);
        x += restLength;
      }
      z -= restLength;
      x = 0;
    }

    // work with length
    for (unsigned j = 0; j < clothWidth*clothLength-1; j++) {
      if (points[j].position.x() != clothLength*restLength-restLength) {
        Spring sh;
        sh.a = &points[j];
        sh.b = &points[j+1];
        sh.stiffness = k;
        sh.restLength = restLength;
        sh.damping = 0;
        springs.push_back(sh);

        // work with down right
        if (j < clothWidth*clothLength-clothLength){
          Spring s;
          s.a = &points[j];
          s.b = &points[j+clothLength+1];
          s.stiffness = k-5;
          s.restLength = sqrt((restLength*restLength)*2);
          s.damping = 0;
          springs.push_back(s);
        }
      }

      // work with down left
      if (j < clothWidth*clothLength-clothLength && j%clothLength != 0 ){
        Spring s;
        s.a = &points[j];
        s.b = &points[j+clothLength-1];
        s.stiffness = k-5;
        s.restLength = sqrt((restLength*restLength)*2);
        s.damping = 0;
        springs.push_back(s);
      }
    }

    // work with width
    for (unsigned j = 0; j < clothWidth*clothLength-clothLength; j++) {
      Spring s;
      s.a = &points[j];
      s.b = &points[j+clothLength];
      s.stiffness = k;
      s.restLength = restLength;
      s.damping = 0;
      springs.push_back(s);
    }
    // end part 5
  }
}

void loadQuadGeometryToGPU(float width) {
  std::vector<Vec3f> verts;

  for (unsigned i = 0; i < points.size(); ++i) {
    float x = points[i].position.x();
    float y = points[i].position.y();
    float z = points[i].position.z();

    if (DEBUG == true)
      std::cout << "points[" << i << "] x = " << x << ", y = " << y << ", z = " << z << std::endl;

    verts.push_back(Vec3f(-0.5*width+x, -0.5*width+y, 0*width+z));
    verts.push_back(Vec3f(-0.5*width+x, 0.5*width+y, 0*width+z));
    verts.push_back(Vec3f(0.5*width+x, 0.5*width+y, 0*width+z));

    verts.push_back(Vec3f(0.5*width+x, 0.5*width+y, 0*width+z));
    verts.push_back(Vec3f(0.5*width+x, -0.5*width+y, 0*width+z));
    verts.push_back(Vec3f(-0.5*width+x, -0.5*width+y, 0*width+z));

  }
  if (DEBUG == true)
    std::cout << " --- " << std::endl;
  glBindBuffer(GL_ARRAY_BUFFER, vertBufferID);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(Vec3f) * 6 * points.size(), // byte size of Vec3f, 6 of them
               verts.data(),      // pointer (Vec3f*) to contents of verts
               GL_STATIC_DRAW);   // Usage pattern of GPU buffer
}

void loadLineGeometryToGPU() {
  std::vector<Vec3f> verts;
  for (unsigned i = 0; i < springs.size(); i++){
    verts.push_back((springs[i].a)->position);
    verts.push_back((springs[i].b)->position);
  }

  glBindBuffer(GL_ARRAY_BUFFER, line_vertBufferID);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(Vec3f) * 2 * springs.size(), // byte size of Vec3f, 4 of them
               verts.data(),      // pointer (Vec3f*) to contents of verts
               GL_STATIC_DRAW);   // Usage pattern of GPU buffer
}

void setupVAO() {
  glBindVertexArray(vaoID);

  glEnableVertexAttribArray(0); // match layout # in shader
  glBindBuffer(GL_ARRAY_BUFFER, vertBufferID);
  glVertexAttribPointer(0,        // attribute layout # above
                        3,        // # of components (ie XYZ )
                        GL_FLOAT, // type of components
                        GL_FALSE, // need to be normalized?
                        0,        // stride
                        (void *)0 // array buffer offset
                        );

  glBindVertexArray(line_vaoID);

  glEnableVertexAttribArray(0); // match layout # in shader
  glBindBuffer(GL_ARRAY_BUFFER, line_vertBufferID);
  glVertexAttribPointer(0,        // attribute layout # above
                        3,        // # of components (ie XYZ )
                        GL_FLOAT, // type of components
                        GL_FALSE, // need to be normalized?
                        0,        // stride
                        (void *)0 // array buffer offset
                        );

  glBindVertexArray(0); // reset to default
}

void reloadProjectionMatrix() {
  // Perspective Only

  // field of view angle 60 degrees
  // window aspect ratio
  // near Z plane > 0
  // far Z plane

  P = PerspectiveProjection(WIN_FOV, // FOV
                            static_cast<float>(WIN_WIDTH) /
                                WIN_HEIGHT, // Aspect
                            WIN_NEAR,       // near plane
                            WIN_FAR);       // far plane depth
}

void loadModelViewMatrix() {
  M = IdentityMatrix();
  line_M = IdentityMatrix();
  // view doesn't change, but if it did you would use this
  V = camera.lookatMatrix();
}

void reloadViewMatrix() { V = camera.lookatMatrix(); }

void setupModelViewProjectionTransform() {
  MVP = P * V * M; // transforms vertices from right to left (odd huh?)
}

void reloadMVPUniform() {
  GLint id = glGetUniformLocation(basicProgramID, "MVP");

  glUseProgram(basicProgramID);
  glUniformMatrix4fv(id,        // ID
                     1,         // only 1 matrix
                     GL_TRUE,   // transpose matrix, Mat4f is row major
                     MVP.data() // pointer to data in Mat4f
                     );
}

void reloadColorUniform(float r, float g, float b) {
  GLint id = glGetUniformLocation(basicProgramID, "inputColor");

  glUseProgram(basicProgramID);
  glUniform3f(id, // ID in basic_vs.glsl
              r, g, b);
}

void generateIDs() {
  // shader ID from OpenGL
  std::string vsSource = loadShaderStringfromFile("./shaders/basic_vs.glsl");
  std::string fsSource = loadShaderStringfromFile("./shaders/basic_fs.glsl");
  basicProgramID = CreateShaderProgram(vsSource, fsSource);

  // VAO and buffer IDs given from OpenGL
  glGenVertexArrays(1, &vaoID);
  glGenBuffers(1, &vertBufferID);
  glGenVertexArrays(1, &line_vaoID);
  glGenBuffers(1, &line_vertBufferID);
}

void deleteIDs() {
  glDeleteProgram(basicProgramID);

  glDeleteVertexArrays(1, &vaoID);
  glDeleteBuffers(1, &vertBufferID);
  glDeleteVertexArrays(1, &line_vaoID);
  glDeleteBuffers(1, &line_vertBufferID);
}

void init() {
  glEnable(GL_DEPTH_TEST);
  glPointSize(50);

  camera = Camera(Vec3f{0, -25, 50}, Vec3f{0, 0, -1}, Vec3f{0, 1, 0});

  setupPoints();

  // SETUP SHADERS, BUFFERS, VAOs

  generateIDs();
  setupVAO();
  loadQuadGeometryToGPU(masswidth);
  loadLineGeometryToGPU();

  loadModelViewMatrix();
  reloadProjectionMatrix();
  setupModelViewProjectionTransform();
  reloadMVPUniform();
}

int main(int argc, char **argv) {
  GLFWwindow *window;



  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  window =
      glfwCreateWindow(1024, 1024, "Assignment 3", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  glfwSetWindowSizeCallback(window, windowSetSizeFunc);
  glfwSetFramebufferSizeCallback(window, windowSetFramebufferSizeFunc);
  glfwSetKeyCallback(window, windowKeyFunc);
  glfwSetCursorPosCallback(window, windowMouseMotionFunc);
  glfwSetMouseButtonCallback(window, windowMouseButtonFunc);

  glfwGetFramebufferSize(window, &WIN_WIDTH, &WIN_HEIGHT);

  // Initialize glad
  if (!gladLoadGL()) {
    std::cerr << "Failed to initialise GLAD" << std::endl;
    return -1;
  }

  std::cout << "GL Version: :" << glGetString(GL_VERSION) << std::endl;
  std::cout << GL_ERROR() << std::endl;

  init(); // our own initialize stuff func



  //              =============================================              //
  //       ===========================================================       //
  // ======================================================================= //
  // ============================ START PROGRAM ============================ //
  // ======================================================================= //

  float t = 0;
  float dt = 0.0001;
  int currentView = view;

  while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
         !glfwWindowShouldClose(window)) {

    if (view == 4 || view == 5)
      dt = 0.00001;
    else
      dt = 0.0001;

    if (currentView != view) {  // change views, restart simulation
      t = 0;
      currentView = view;

      if (view == 5) {
        camera = Camera(Vec3f{-100, -30, 70}, Vec3f{1, 0, -1}, Vec3f{0, 1, 0});
        reloadViewMatrix();

      }
      if (view == 4) {
        camera = Camera(Vec3f{25, -15, 250}, Vec3f{0, 0, -1}, Vec3f{0, 1, 0});
        reloadViewMatrix();
      }
      else if (view < 4) {
        camera = Camera(Vec3f{0, -25, 50}, Vec3f{0, 0, -1}, Vec3f{0, 1, 0});
        reloadViewMatrix();
      }

      loadLineGeometryToGPU();
      loadQuadGeometryToGPU(masswidth);
    }
    if (g_play) {
      if (replay) {
        t = 0;
        animatePoints(t);
        loadLineGeometryToGPU();
        loadQuadGeometryToGPU(masswidth);
        replay = false;
      }

      t += dt;
      animatePoints(t);
      loadLineGeometryToGPU();
      loadQuadGeometryToGPU(masswidth);

    }

    displayFunc();
    moveCamera();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // clean up after loop
  deleteIDs();

  return 0;
}

// ======================================================================= //
// ============================= END PROGRAM ============================= //
// ======================================================================= //
//       ===========================================================       //
//              =============================================              //


//==================== CALLBACK FUNCTIONS ====================//

void windowSetSizeFunc(GLFWwindow *window, int width, int height) {
  WIN_WIDTH = width;
  WIN_HEIGHT = height;

  reloadProjectionMatrix();
  setupModelViewProjectionTransform();
  reloadMVPUniform();
}

void windowSetFramebufferSizeFunc(GLFWwindow *window, int width, int height) {
  FB_WIDTH = width;
  FB_HEIGHT = height;

  glViewport(0, 0, FB_WIDTH, FB_HEIGHT);
}

void windowMouseButtonFunc(GLFWwindow *window, int button, int action,
                           int mods) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    if (action == GLFW_PRESS) {
      g_cursorLocked = GL_TRUE;
    } else {
      g_cursorLocked = GL_FALSE;
    }
  }
}

void windowMouseMotionFunc(GLFWwindow *window, double x, double y) {
  if (g_cursorLocked) {
    float deltaX = (x - g_cursorX) * 0.01;
    float deltaY = (y - g_cursorY) * 0.01;
    camera.rotateAroundFocus(deltaX, deltaY);

    reloadViewMatrix();
    setupModelViewProjectionTransform();
    reloadMVPUniform();
  }

  g_cursorX = x;
  g_cursorY = y;
}

void windowKeyFunc(GLFWwindow *window, int key, int scancode, int action,
                   int mods) {
  bool set = action != GLFW_RELEASE && GLFW_REPEAT;
  switch (key) {
  case GLFW_KEY_ESCAPE:
    glfwSetWindowShouldClose(window, GL_TRUE);
    break;
  case GLFW_KEY_ENTER:
    if (view < 5) {
      set ? 1: view++;
      setupPoints();
    }
    else
    {
      set ? 1: view = 1;
      setupPoints();
    }
    break;
  case GLFW_KEY_W:
    g_moveBackForward = set ? 1 : 0;
    break;
  case GLFW_KEY_0:
    replay = set ? 1 : 0;
    setupPoints();
    break;
  case GLFW_KEY_S:
    g_moveBackForward = set ? -1 : 0;
    break;
  case GLFW_KEY_A:
    g_moveLeftRight = set ? 1 : 0;
    break;
  case GLFW_KEY_D:
    g_moveLeftRight = set ? -1 : 0;
    break;
  case GLFW_KEY_Q:
    g_moveUpDown = set ? -1 : 0;
    break;
  case GLFW_KEY_E:
    g_moveUpDown = set ? 1 : 0;
    break;
  case GLFW_KEY_UP:
    g_rotateUpDown = set ? -1 : 0;
    break;
  case GLFW_KEY_DOWN:
    g_rotateUpDown = set ? 1 : 0;
    break;
  case GLFW_KEY_LEFT:
    if (mods == GLFW_MOD_SHIFT)
      g_rotateRoll = set ? -1 : 0;
    else
      g_rotateLeftRight = set ? 1 : 0;
    break;
  case GLFW_KEY_RIGHT:
    if (mods == GLFW_MOD_SHIFT)
      g_rotateRoll = set ? 1 : 0;
    else
      g_rotateLeftRight = set ? -1 : 0;
    break;
  case GLFW_KEY_SPACE:
    g_play = set ? !g_play : g_play;
    break;
  case GLFW_KEY_LEFT_BRACKET:
    if (mods == GLFW_MOD_SHIFT) {
      g_rotationSpeed *= 0.5;
    } else {
      g_panningSpeed *= 0.5;
    }
    break;
  case GLFW_KEY_RIGHT_BRACKET:
    if (mods == GLFW_MOD_SHIFT) {
      g_rotationSpeed *= 1.5;
    } else {
      g_panningSpeed *= 1.5;
    }
    break;
    default:
    break;
  }
}

//==================== OPENGL HELPER FUNCTIONS ====================//

void moveCamera() {
  Vec3f dir;

  if (g_moveBackForward) {
    dir += Vec3f(0, 0, g_moveBackForward * g_panningSpeed);
  }
  if (g_moveLeftRight) {
    dir += Vec3f(g_moveLeftRight * g_panningSpeed, 0, 0);
  }
  if (g_moveUpDown) {
    dir += Vec3f(0, g_moveUpDown * g_panningSpeed, 0);
  }

  if (g_rotateUpDown) {
    camera.rotateUpDown(g_rotateUpDown * g_rotationSpeed);
  }
  if (g_rotateLeftRight) {
    camera.rotateLeftRight(g_rotateLeftRight * g_rotationSpeed);
  }
  if (g_rotateRoll) {
    camera.rotateRoll(g_rotateRoll * g_rotationSpeed);
  }

  if (g_moveUpDown || g_moveLeftRight || g_moveBackForward ||
      g_rotateLeftRight || g_rotateUpDown || g_rotateRoll) {
    camera.move(dir);
    reloadViewMatrix();
    setupModelViewProjectionTransform();
    reloadMVPUniform();
  }
}

std::string GL_ERROR() {
  GLenum code = glGetError();

  switch (code) {
  case GL_NO_ERROR:
    return "GL_NO_ERROR";
  case GL_INVALID_ENUM:
    return "GL_INVALID_ENUM";
  case GL_INVALID_VALUE:
    return "GL_INVALID_VALUE";
  case GL_INVALID_OPERATION:
    return "GL_INVALID_OPERATION";
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return "GL_INVALID_FRAMEBUFFER_OPERATION";
  case GL_OUT_OF_MEMORY:
    return "GL_OUT_OF_MEMORY";
  default:
    return "Non Valid Error Code";
  }
}
