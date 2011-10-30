#include <iomanip>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "mat.h"

#include "Rect.h"
#include "Poly.h"
#include "LineSegs.h"
#include "Point.h"

#include "InitShader.h"
#include "SketchPadDefs.h"

using namespace std;

const char VSHADER_NAME[] = "../src/shaders/vshader.glsl";
const char FSHADER_NAME[] = "../src/shaders/fshader.glsl";

unsigned int WIN_HEIGHT = 500;
unsigned int WIN_WIDTH = 500;

typedef boost::shared_ptr<Shape> ShapePtr;

mat4 projectionMatrix;
vector<ShapePtr> shapes;

ShapePtr drawingShape;
ShapePtr copyShape;

// used when drawing shapes, some parameters may not apply to all objects
ShapeParameters params;

// used for translate rotate and scale 3rd dim used to store extra info
vec3 point1(0.0,0.0,-1.0), point2(0.0,0.0,-1.0), point3(0.0,0.0,-1.0);

// the current mode of the program
int programMode = MODE_NONE;

int selectedIndex = -1;
ShapePtr selectedShape;

// global shader indecies
GLuint program;
GLuint projection;

// tells the program if the mouse is being held down
bool mousePressed = false;

vec4 allColors[] = {
  COLOR_WHITE,
  COLOR_BLACK,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_YELLOW,
  COLOR_ORANGE,
  COLOR_PURPLE,
  COLOR_VIOLET,
  COLOR_BROWN
};

vector<vec4> Colors(allColors,allColors+sizeof(allColors)/sizeof(vec4));
vector<vec4>::iterator ColorIt = Colors.begin();

// TODO: implement an unProject function to avoid deprecated glu function

vec4 colorInv(vec4 input)
{
  return vec4(1.0,1.0,1.0,2.0)-input;
}

void init()
{
  program = InitShader(VSHADER_NAME, FSHADER_NAME);
  projection = glGetUniformLocation(program,"projection");

  params.thickness = 3.0;
  params.color = *ColorIt; 

  glClearColor(0.5,0.5,0.5,1.0);
}

void display()
{
  glClear( GL_COLOR_BUFFER_BIT );

  // set the projections matrix
  glUniformMatrix4fv(projection,1,GL_TRUE,projectionMatrix);

  int index = 0;
  for ( vector<ShapePtr>::iterator i = shapes.begin(); i != shapes.end(); ++i )
  {
    if ( selectedIndex == index )
      selectedShape->draw();
    (*i)->draw();
    index++;
  }
 
  if ( drawingShape )
    drawingShape->draw();

  glutSwapBuffers();
}

void clearSelection()
{
  if ( selectedIndex >= 0 )
  {
    selectedIndex = -1;
    glutPostRedisplay();
  }
}

void resize(int width, int height)
{
  glViewport(0,0,width,height);
  projectionMatrix = Ortho2D(
    -(1.0+(width-(GLfloat)WIN_WIDTH)/WIN_WIDTH),  // left
    1.0+(width-(GLfloat)WIN_WIDTH)/WIN_WIDTH,       // right
    -(1.0+(height-(GLfloat)WIN_HEIGHT)/WIN_HEIGHT),  // bottom
    1.0+(height-(GLfloat)WIN_HEIGHT)/WIN_HEIGHT     // top
  );

  glutPostRedisplay();
}

vec2 mouse2Camera(int x, int y)
{
  const mat4* m = &projectionMatrix;

  GLdouble proj[16] =
    {(*m)[0][0], (*m)[1][0], (*m)[2][0], (*m)[3][0],
     (*m)[0][1], (*m)[1][1], (*m)[2][1], (*m)[3][1],
     (*m)[0][2], (*m)[1][2], (*m)[2][2], (*m)[3][2],
     (*m)[0][3], (*m)[1][3], (*m)[2][3], (*m)[3][3]};

  static GLdouble model[16] = {1.0, 0.0, 0.0, 0.0,
                               0.0, 1.0, 0.0, 0.0,
                               0.0, 0.0, 1.0, 0.0,
                               0.0, 0.0, 0.0, 1.0};
  
  GLdouble mouseX = static_cast<GLdouble>(x);
  GLdouble mouseY = static_cast<GLdouble>(y);
  
  GLdouble objX, objY, objZ;
  GLint view[4];

  glGetIntegerv(GL_VIEWPORT, view);

  gluUnProject(mouseX, mouseY, 0, model, proj, view,
              &objX, &objY, &objZ);

  // for some reason objY comes out negative
  return vec2(static_cast<GLfloat>(objX),
              static_cast<GLfloat>(-objY));
}

void selectObject(vec2 coords)
{
  // can't select from an empty screen :P
  if ( shapes.size() <= 0 )
    return;

  unsigned start = 0;
  if ( selectedIndex >= 0 )
    start = selectedIndex;
  
  unsigned index = start;
  do {
    if ( index == 0 )
      index = shapes.size();
    index--;
    if ( shapes[index]->isInside(coords) )
    {
      if ( selectedIndex != index )
      {
        selectedIndex = index;

        ///////////////////// Add new types here for selection /////////////
        if      ( typeid(*shapes[index]) == typeid(Rect) )
        {
          cout << "Selected" << endl;
          selectedShape = ShapePtr(new
            Rect(*(reinterpret_cast<Rect*>(&(*shapes[index])))));
        }
        else if ( typeid(*shapes[index]) == typeid(Poly) )
          selectedShape = ShapePtr(new
            Poly(*(reinterpret_cast<Poly*>(&(*shapes[index])))));
        else if ( typeid(*shapes[index]) == typeid(LineSegs) )
          selectedShape = ShapePtr(new
            LineSegs(*(reinterpret_cast<LineSegs*>(&(*shapes[index])))));
        else if ( typeid(*shapes[index]) == typeid(Point) )
        {
          vec2 uL, lR;
          uL = vec2(shapes[index]->getVertices()[0].x,
                    shapes[index]->getVertices()[0].y) +
                    shapes[index]->getTranslation() +
               vec2(-0.01,0.01);
          lR = vec2(shapes[index]->getVertices()[0].x,
                    shapes[index]->getVertices()[0].y) +
                    shapes[index]->getTranslation() +
               vec2(0.01,-0.01);

          selectedShape = ShapePtr(new Rect(uL,lR,true));
          selectedShape->init(program);
        }
        ////////////////////////////////////////////////////////////////////

        if ( selectedIndex >= 0 )
          shapes[selectedIndex]->unSelectShape();
        shapes[index]->selectShape();

        if ( typeid(*shapes[index]) != typeid(Point) )
        {
          selectedShape->unFillShape();
          if ( shapes[selectedIndex]->getFilled() )
            selectedShape->setThickness(5.0);
          else
            selectedShape->setThickness(
              shapes[selectedIndex]->getThickness()+2.0);
        }

        selectedShape->setColor(colorInv(shapes[index]->getColor()));
        glutPostRedisplay();

      }
      break;
    }

  } while ( index != start );
  if ( index == start && !shapes[index]->isInside(coords) )
    selectedIndex = -1;
}

float getAngle(vec2 center, vec2 pt1, vec2 pt2)
{
  // uses the law of cosines
  float lenC1, len12, lenC2;
  lenC1 = sqrt((center.x-pt1.x)*(center.x-pt1.x)+
               (center.y-pt1.y)*(center.y-pt1.y));
  len12 = sqrt((pt1.x-pt2.x)*(pt1.x-pt2.x)+
               (pt1.y-pt2.y)*(pt1.y-pt2.y));
  lenC2 = sqrt((center.x-pt2.x)*(center.x-pt2.x)+
               (center.y-pt2.y)*(center.y-pt2.y));
 
  float angle = ( std::acos((lenC1*lenC1-len12*len12+lenC2*lenC2) /
                  (2.0*lenC1*lenC2)) * 180.0/M_PI );

  // cross product to figure out clockwise or counter-clockwise
  if ( cross(pt1-center,pt2-center).z > 0 ) 
    return angle;
  else
    return -angle;
}

void mouseDown(vec2 coords)
{
  mousePressed = true;

  switch (programMode)
  {
    case MODE_DRAW_RECT:
      drawingShape = ShapePtr(
          new Rect(coords, params.filled, params.color, params.thickness));
      drawingShape->init(program);
      drawingShape->mouseDown(coords, programMode);
      break;
    case MODE_DRAW_LINE_SEG:
    case MODE_DRAW_POLY:
      if ( !drawingShape )
      {
        cout << "Building shape" << endl;
        drawingShape = ShapePtr(
            new LineSegs(coords, params.color, params.thickness));
        drawingShape->init(program);
      }
      drawingShape->mouseDown(coords, programMode);
      break;
    case MODE_DRAW_POINT:
    {
      ShapePtr p = ShapePtr(new Point(coords, params.color, params.thickness));
      shapes.push_back(p);
      shapes.back()->init(program);
      break;
    }
    case MODE_SELECT:
      selectObject(coords);
      break;
  }

  // must be a shape selected for these to work
  if ( selectedIndex >= 0 )
  {
    switch (programMode)
    {
      case MODE_TRANSLATE_SHAPE:
      {
        vec2 trans = shapes[selectedIndex]->getTranslation();
        point3 = vec3(trans.x,trans.y,0.0);
        point1 = vec3(coords.x,coords.y,0.0);
        break;
      }
      case MODE_ROTATE_SHAPE:
      {
        if ( typeid(*shapes[selectedIndex]) == typeid(Point) )
          break;
        GLfloat theta = shapes[selectedIndex]->getTheta();
        vec2 center = shapes[selectedIndex]->getCenter();
        point3 = vec3(center.x,center.y,theta);
        point1 = vec3(coords.x,coords.y,0.0);
        break;
      }
      case MODE_SCALE_SHAPE:
      {
        if ( typeid(*shapes[selectedIndex]) == typeid(Point) )
          break;
        GLfloat theta = -shapes[selectedIndex]->getTheta()*M_PI/180.0;
        vec2 scale = shapes[selectedIndex]->getScale();
        vec2 center = shapes[selectedIndex]->getCenter();
        
        point1 = vec3(center.x,center.y,cos(theta));
        point2 = vec3(scale.x,scale.y,sin(theta));
        point3 = vec3(coords.x-center.x,coords.y-center.y,0.0);
        point3 = mat3(point1.z,-point2.z,0,
                      point2.z,point1.z,0,
                      0,0,1) * point3;
        break;
      }
    }
  }

}

void mouseMove(vec2 coords)
{
  switch (programMode)
  {
    case MODE_DRAW_RECT:
    case MODE_DRAW_LINE_SEG:
    case MODE_DRAW_POLY:
      if ( !drawingShape )
        return;
      drawingShape->mouseMove(coords, programMode);
      break;
  }

  if ( selectedIndex >= 0 )
  {
    switch (programMode)
    {
      case MODE_TRANSLATE_SHAPE:
      {
        point2 = vec3(coords.x,coords.y,0.0);
        vec3 newTrans = point3+point2-point1;
        shapes[selectedIndex]->setTranslate(vec2(newTrans.x,newTrans.y));
        selectedShape->setTranslate(vec2(newTrans.x,newTrans.y));
        break;
      }
      case MODE_ROTATE_SHAPE:
      {
        if ( typeid(*shapes[selectedIndex]) == typeid(Point) )
          break;
        point2 = vec3(coords.x,coords.y,0.0);
        GLfloat newTheta = point3.z+getAngle(
            vec2(point3.x,point3.y),
            vec2(point1.x,point1.y),
            vec2(point2.x,point2.y));
        shapes[selectedIndex]->setTheta(newTheta);
        selectedShape->setTheta(newTheta);
        break;
      }
      case MODE_SCALE_SHAPE:
      {
        if ( typeid(*shapes[selectedIndex]) == typeid(Point) )
          break;
        vec2 newScale = coords - vec2(point1.x,point1.y);
        newScale = mat2(point1.z,-point2.z,
                        point2.z,point1.z) * newScale;
        newScale = newScale - vec2(point3.x,point3.y);
        newScale.x /= point3.x;
        newScale.y /= point3.y;
        newScale = vec2(point2.x*(newScale.x+1.0),point2.y*(newScale.y+1.0));

        shapes[selectedIndex]->setScale(newScale);
        selectedShape->setScale(newScale);
        break;
      }
    }
  }
}

void mouseUp(vec2 coords)
{
  mousePressed = false;
  
  switch (programMode)
  {
    case MODE_DRAW_RECT:
      if ( !drawingShape )
        return;
      drawingShape->mouseUp(coords, programMode);
      shapes.push_back(drawingShape);
      drawingShape = ShapePtr();
      break;
  }
}

void mouseMotion(int x, int y)
{
  if ( mousePressed || ( drawingShape && (
       programMode == MODE_DRAW_LINE_SEG ||
       programMode == MODE_DRAW_POLY ) ) )
  {
    vec2 coords = mouse2Camera(x,y);
    mouseMove(coords);
    glutPostRedisplay();
  }
}

void mousePress(int button, int state, int x, int y)
{
  vec2 coords = mouse2Camera(x,y);

  if ( button == GLUT_LEFT_BUTTON && state == GLUT_DOWN )
  {
    mouseDown(coords);
    glutPostRedisplay();
  }
  else if ( button == GLUT_LEFT_BUTTON && state == GLUT_UP )
  {
    mouseUp(coords);
    glutPostRedisplay();
  }

  if ( button == GLUT_RIGHT_BUTTON && drawingShape )
  {
    if ( programMode == MODE_DRAW_LINE_SEG )
    {
      shapes.push_back(drawingShape);
      drawingShape = ShapePtr();
      glutPostRedisplay();
    }
    else if ( programMode == MODE_DRAW_POLY )
    {
      // convert the LineSeg into a Poly
      ShapePtr p =
        ShapePtr(new Poly(*reinterpret_cast<LineSegs*>(&(*drawingShape))));
      shapes.push_back(p);
      drawingShape = ShapePtr();
      glutPostRedisplay();
    }
  }
}

std::string colorName(vec4 color)
{
  if (color == COLOR_WHITE)
    return "WHITE";
  else if ( color == COLOR_BLACK )
    return "BLACK";
  else if ( color == COLOR_RED )
    return "RED";
  else if ( color == COLOR_GREEN )
    return "GREEN";
  else if ( color == COLOR_BLUE )
    return "BLUE";
  else if ( color == COLOR_CYAN )
    return "CYAN";
  else if ( color == COLOR_MAGENTA )
    return "MAGENTA";
  else if ( color == COLOR_YELLOW )
    return "YELLOW";
  else if ( color == COLOR_ORANGE )
    return "ORANGE";
  else if ( color == COLOR_PURPLE )
    return "PURPLE";
  else if ( color == COLOR_VIOLET )
    return "VIOLET";
  else if ( color == COLOR_BROWN )
    return "BROWN";
  else
  {
    ostringstream sout;
    sout << "RGB(" << color.x << ", " << color.y << ", " << color.z << ")";
    return sout.str();
  }
}

vec4 nextColor()
{
  ColorIt++;
  if ( ColorIt == Colors.end() )
    ColorIt = Colors.begin();

  return *ColorIt;
}
        
void copyShapeIntoPointer(ShapePtr shape,ShapePtr& output)
{
  if      ( typeid(*shape) == typeid(Rect) )
    output = ShapePtr(new
      Rect(*(reinterpret_cast<Rect*>(&(*shape)))));
  else if ( typeid(*shape) == typeid(Poly) )
    output = ShapePtr(new
      Poly(*(reinterpret_cast<Poly*>(&(*shape)))));
  else if ( typeid(*shape) == typeid(LineSegs) )
    output = ShapePtr(new
      LineSegs(*(reinterpret_cast<LineSegs*>(&(*shape)))));
  else if ( typeid(*shape) == typeid(Point) )
    output = ShapePtr(new
      Point(*(reinterpret_cast<Point*>(&(*shape)))));
}

void keyboardPress(unsigned char key, int x, int y)
{
  // keyboard disabled when mouse is pressed
  if ( mousePressed ) return;

  if ( !drawingShape  )
  {
    switch (key)
    {
      // change of mode cases
      case 'q': // draw rectangle
        if ( programMode == MODE_DRAW_RECT )
          return;
        clearSelection();
        cout << "DRAW RECTANGLES" << endl;
        programMode = MODE_DRAW_RECT;
        break;
      case 'w': // draw polygon
        if ( programMode == MODE_DRAW_POLY )
          return;
        clearSelection();
        cout << "DRAW POLYGONS" << endl;
        programMode = MODE_DRAW_POLY;
        break;
      case 'e': // draw circle
        if ( programMode == MODE_DRAW_CIRCLE )
          return;
        clearSelection();
        cout << "DRAW CIRCLES" << endl;
        programMode = MODE_DRAW_CIRCLE;
        break;
      case 'a': // draw line segment
        if ( programMode == MODE_DRAW_LINE_SEG )
          return;
        clearSelection();
        cout << "DRAW CONNECTED LINE SEGMENTS" << endl;
        programMode = MODE_DRAW_LINE_SEG;
        break;
      case 'd': // draw points
        if ( programMode == MODE_DRAW_POINT )
          return;
        clearSelection();
        cout << "DRAW POINTS" << endl;
        programMode = MODE_DRAW_POINT;
        break;
      case 's': // select mode
        if ( programMode == MODE_SELECT )
          return;
        cout << "SELECT MODE" << endl;
        programMode = MODE_SELECT;
        break;
      
      case '1': // toggle filled
        if ( selectedShape )
          break;
        params.filled = !params.filled;
        if ( params.filled )
          cout << "NOW DRAWING FILLED POLYGONS" << endl;
        else
          cout << "NOW DRAWING UNFILLED POLYGONS" << endl;
        break;
      case '2': // change color
        if ( selectedShape )
          break;
        params.color = nextColor();
        cout << "CHANGING COLOR TO " << colorName(*ColorIt) << endl;
        break;
      case '3': // increase thickness
      {
        if ( selectedShape )
          break;
        params.thickness += THICKNESS_STEP;
        if ( params.thickness > MAX_THICKNESS )
          params.thickness = MAX_THICKNESS;
        cout << "INCREASING THICKNESS TO " << params.thickness << endl;
        break;
      }
      case '4': // decrease thickness
      {
        if ( selectedShape )
          break;
        params.thickness -= THICKNESS_STEP;
        if ( params.thickness < MIN_THICKNESS )
          params.thickness = MIN_THICKNESS;
        cout << "DECREASING THICKNESS TO " << params.thickness << endl;
        break;
      }

      case 'v': // paste
      {
        if ( !copyShape )
          break;
        ShapePtr newShape;
        copyShapeIntoPointer(copyShape,newShape);
        shapes.push_back(newShape);
        cout << "PASTED SHAPE" << endl;
        glutPostRedisplay();
        break;
      }
    }
  }

  switch (key)
  {
    case 27:  // esc key
      if ( !mousePressed )
      {
        clearSelection();
        drawingShape = ShapePtr();
        programMode = MODE_NONE;
        glutPostRedisplay();
      }
      break;
  }

  if ( selectedIndex >= 0 )
  {
    switch (key)
    {
      case 'r': // rotate mode
        programMode = MODE_ROTATE_SHAPE;
        cout << "ROTATE MODE" << endl;
        break;
      case 't': // translate mode
        programMode = MODE_TRANSLATE_SHAPE;
        cout << "TRANSLATE MODE" << endl;
        break;
      case 'y': // scale mode
        programMode = MODE_SCALE_SHAPE;
        cout << "SCALE MODE" << endl;
        break;
      
      case '1': // toggle filled
        shapes[selectedIndex]->toggleFilled();
        if ( shapes[selectedIndex]->getFilled() )
          selectedShape->setThickness(5.0);
        else
          selectedShape->setThickness(
            shapes[selectedIndex]->getThickness()+2.0);
        params.filled = !params.filled;
        cout << "TOGGLING FILL PROPERTY" << endl;
        glutPostRedisplay();
        break;
      case '2': // change color
        shapes[selectedIndex]->setColor(nextColor());
        cout << "CHANGING COLOR TO " << colorName(*ColorIt) << endl;
        selectedShape->setColor(colorInv(*ColorIt));
        params.color = *ColorIt;
        glutPostRedisplay();
        break;
      case '3': // increase thickness
      {
        if ( shapes[selectedIndex]->getFilled() )
          break;

        GLfloat thickness = shapes[selectedIndex]->getThickness();
        thickness += THICKNESS_STEP;
        if ( thickness > MAX_THICKNESS )
          thickness = MAX_THICKNESS;
        shapes[selectedIndex]->setThickness(thickness);
        selectedShape->setThickness(thickness+2.0);
        params.thickness = thickness;

        cout << "INCREASING THICKNESS TO " << thickness << endl;
        glutPostRedisplay();
        break;
      }
      case '4': // decrease thickness
      {
        if ( shapes[selectedIndex]->getFilled() )
          break;

        GLfloat thickness = shapes[selectedIndex]->getThickness();
        thickness -= THICKNESS_STEP;
        if ( thickness < MIN_THICKNESS )
          thickness = MIN_THICKNESS;
        shapes[selectedIndex]->setThickness(thickness);
        selectedShape->setThickness(thickness+2.0);
        params.thickness = thickness;

        cout << "DECREASING THICKNESS TO " << thickness << endl;
        glutPostRedisplay();
        break;
      }

      case 'x': // cut
      {
        copyShapeIntoPointer(shapes[selectedIndex],copyShape);
        vector<ShapePtr>::iterator it = shapes.begin();
        for ( int i = 0; i < selectedIndex; ++i )
          it++;
        shapes.erase(it);
        selectedIndex = -1;
        selectedShape = ShapePtr();
        cout << "CUT SHAPE" << endl;
        glutPostRedisplay();
        break;
      }
      case 'c': // copy
        copyShapeIntoPointer(shapes[selectedIndex],copyShape);
        cout << "COPY SHAPE" << endl;
        break;
    }
  }
}


//// glut special function keys
// GLUT_KEY_F1   F1 function key
// GLUT_KEY_F2   F2 function key
// GLUT_KEY_F3   F3 function key
// GLUT_KEY_F4   F4 function key
// GLUT_KEY_F5   F5 function key
// GLUT_KEY_F6   F6 function key
// GLUT_KEY_F7   F7 function key
// GLUT_KEY_F8   F8 function key
// GLUT_KEY_F9   F9 function key
// GLUT_KEY_F10    F10 function key
// GLUT_KEY_F11    F11 function key
// GLUT_KEY_F12    F12 function key
// GLUT_KEY_LEFT   Left function key
// GLUT_KEY_RIGHT    Right function key
// GLUT_KEY_UP   Up function key
// GLUT_KEY_DOWN   Down function key
// GLUT_KEY_PAGE_UP  Page Up function key
// GLUT_KEY_PAGE_DOWN  Page Down function key
// GLUT_KEY_HOME   Home function key
// GLUT_KEY_END    End function key
// GLUT_KEY_INSERT   Insert function key
void specialPress(int key, int x, int y)
{}

int main(int argc, char *argv[])
{
  srand(time(0));
  
  glutInit(&argc,argv);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
  glutInitWindowSize(WIN_WIDTH, WIN_HEIGHT);
  glutCreateWindow("Sketchpad");

  glewInit();
  init();

  glutDisplayFunc(display);
  glutReshapeFunc(resize);
  glutKeyboardFunc(keyboardPress);
  glutSpecialFunc(specialPress);

  //glutKeyboardUpFunc(keyboardUp);
  glutMouseFunc(mousePress);
  glutPassiveMotionFunc(mouseMotion); 
  glutMotionFunc(mouseMotion);

  glutMainLoop();

  return 0;
}
