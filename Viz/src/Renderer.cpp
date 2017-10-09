//
//  Renderer.cpp
//  EnsembleVisualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

// This module
#include "Renderer.h"

// Cinder
#include <cinder/Surface.h>
#include <cinder/Rand.h>
#include <cinder/DataSource.h>
#include <cinder/Channel.h>
#include <cinder/ImageIo.h>
using namespace ci;

// C++ std
#include <map>
#include <algorithm>
using namespace std;


#define TEXTURE_ROW_SIZE 16000
// On Tim's laptop this can be 16000
// On Daniel's laptop this can be 8000


void Renderer::setState(State const & newState)
{
    mState = newState;
}

State Renderer::state() const
{
    return mState;
}

Renderer::Renderer()
    : mEnableDrawConnectionsDebug(false)
    , mShaderLoaded(false)
    , mNumParticles(30000)
    , mNumRandoms(9)
    , mRotation(0.0)
{
    // + Load blob texture {{{

    Surface blob = Surface(loadImage(app::getAssetPath("blob.png")));
    mParticleTex = gl::Texture::create(blob);

    // + }}}

    // + Make control points texture {{{

    // Determine size -
    //  One column for every pairing of instruments
    int cpTexWidth = NUM_INSTRUMENTS * NUM_INSTRUMENTS;
    //  Values going down a column
    //   (x 1) Number of control points
    //   (x 1) Start point position
    //   (x MAX_CONTROL_POINTS) Control point positions
    //   (x 1) End point position
    int cpTexHeight = 1 + 1 + MAX_CONTROL_POINTS + 1;

    // Make Cinder surface and clear all to zero
    Surface32f cpInit = Surface32f(cpTexWidth, cpTexHeight, true, SurfaceChannelOrder::RGBA);
    for (int i = 0; i < cpInit.getWidth(); i++)
    {
        for (int j = 0; j < cpInit.getHeight(); j++)
        {
            *cpInit.getDataRed(Vec2i(i, j)) = 0.0f;
            *cpInit.getDataGreen(Vec2i(i, j)) = 0.0f;
            *cpInit.getDataBlue(Vec2i(i, j)) = 0.0f;
            *cpInit.getDataAlpha(Vec2i(i, j)) = 0.0f;
        }
    }

    // Make into non-filtered texture
    mControlPointsTex = gl::Texture::create(cpInit);
    mControlPointsTex->setMagFilter(GL_NEAREST);
    mControlPointsTex->setMinFilter(GL_NEAREST);

    // In mControlPoints, for every pairing of instruments make a 2D vector
    for (int i = 0; i < NUM_INSTRUMENTS; ++i)
    {
        for (int j = 0; j < NUM_INSTRUMENTS; ++j)
        {
            mControlPoints[i][j] = vector<ci::Vec2f>();
        }
    }
    updateCalculatedControlPoints();

    // + }}}

    loadShader();

    int maxTextureSize(-42);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    // Need to send mNumRandoms numbers for each particle via texture.
    //
    // Will make a 2D texture with height maximized to near the video card texture size limit
    // (TEXTURE_ROW_SIZE), and width minimized according to how much space is needed.
    //
    // The numbers for particle 0 go in row 0, cols 0 to mNumRandoms-1
    // The numbers for particle 1 go in row 1, cols 0 to mNumRandoms-1
    // Etc.
    //
    // After TEXTURE_ROW_SIZE particles, return to row 0 and continue similarly but storing
    // values in cols mNumRandoms to 2*mNumRandoms-1.

    // randomChan holds mNumRandoms random numbers (columns) for each
    // of the mNumParticles particles (rows)
    // However, due to implementation limits, the rows are limited
    // to TEXTURE_ROW_SIZE.
    // After each TEXTURE_ROW_SIZE particles, start a new row ('colSet')

    // The -1, divide, +1 process
    // divides by TEXTURE_ROW_SIZE then rounds up to next multiple of TEXTURE_ROW_SIZE
    int randomWidth = mNumRandoms * ((mNumParticles - 1)/TEXTURE_ROW_SIZE + 1);
    int randomHeight = min(mNumParticles, TEXTURE_ROW_SIZE);
    Channel32f randomChan(randomWidth, randomHeight);
    assert(randomChan.getWidth() < maxTextureSize);
    assert(randomChan.getHeight() < maxTextureSize);
    {
        // This is a straight vector of vectors
        mRandoms = vector< vector<float> >(mNumParticles, vector<float>(mNumRandoms));

        Rand random;
        for (int particleNo = 0; particleNo < mNumParticles; ++particleNo)
        {
            int colSet = particleNo / TEXTURE_ROW_SIZE;
            for (int randomNo = 0; randomNo < mNumRandoms; ++randomNo)
            {
                float nextFloat = random.nextFloat();
                int col = colSet * mNumRandoms + randomNo;
                int row = particleNo % TEXTURE_ROW_SIZE;
                assert(row < randomChan.getHeight());
                assert(col < randomChan.getWidth());
                mRandoms.at(particleNo).at(randomNo) = nextFloat;
                *randomChan.getData(col, row) = nextFloat;
//              *randomChan.getData(randomNo, particleNo) = 0;
            }
        }
    }
    mRandomTex = gl::Texture::create(randomChan);
    mRandomTex->setMagFilter(GL_NEAREST);
    mRandomTex->setMinFilter(GL_NEAREST);
}

void Renderer::loadShader()
{
    mShaderLoaded = false;
    try
    {
        mShader = gl::GlslProg::create(DataSourcePath::create(app::getAssetPath("particles.vert")), DataSourcePath::create(app::getAssetPath("particles.frag")));
        mShaderLoaded = true;
        cout << "\n*\nShader compiled successfully" << endl;
    }
    catch (gl::GlslProgCompileExc e)
    {
        cout << "\n*\nError compiling shader:\n" <<e.what()<<endl;
    }
}

void Renderer::setEnableDrawConnectionsDebug(bool enabled)
{
    mEnableDrawConnectionsDebug = enabled;
}

Renderer::~Renderer()
{
}

vector<vector<float> > makeRandoms(int rows, int cols)
{
    Rand random;
    vector<vector<float> > v = vector<vector<float> >(rows, vector<float>(cols));
    for (int i=0; i<rows; i++)
    {
        for (int j=0; j<cols; j++)
        {
            v.at(i).at(j) = random.nextFloat();
        }
    }
    return v;
}


void Renderer::draw(float elapsedTime)
{
    int N = mNumParticles;
    // w is the size, z is the id
    vector<Vec4f> points;
    points.reserve(N);
    map<int,int> pCount;
    map<int,int> qCount;
    for (int particleNo = 0; particleNo < N; ++particleNo)
    {
        int randomCount = 0;
        int p = particleNo%NUM_INSTRUMENTS;
        int q = (particleNo/NUM_INSTRUMENTS)%NUM_INSTRUMENTS; // dest
        if (p==q)
            continue;
        float amount = mState.instruments.at(p).connections.at(q);
        if (0)//(amount>0.3 && ofGetFrameNum()%30==0)
        {
            printf("Points for instrument %s and %s:\n", mState.instruments[p].name.c_str(), mState.instruments[q].name.c_str());
            cout << mControlPoints[p][q];
            cout << endl << endl;
        }
        points.push_back(Vec4f(p, q, points.size(), amount));
    }

    render(elapsedTime, points);

    if (mEnableDrawConnectionsDebug)
        drawConnectionsDebug();
}

void Renderer::render(float elapsedTime, std::vector<ci::Vec4f> const& points)
{
    glClearColor(0,0,0,0);
//  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // normalized coordinates: 2x2 square centred at origin
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(mRotation, 0, 0, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mParticleTex->getId());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mRandomTex->getId());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, mControlPointsTex->getId());
    if (mShaderLoaded)
    {
        mShader->bind();
        mShader->uniform("Tex", 0);
        mShader->uniform("Rand", 1);
        mShader->uniform("RandSize", Vec2f(mRandomTex->getSize()));
        mShader->uniform("ControlPoints", 2);
        mShader->uniform("ControlPointsSize", Vec2f(mControlPointsTex->getSize()));
        mShader->uniform("numRandomsPerParticle", mNumRandoms);
        mShader->uniform("textureRowSize", TEXTURE_ROW_SIZE);
        mShader->uniform("time", elapsedTime);
    }
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(4, GL_FLOAT, 0, points.data());
        glDrawArrays(GL_POINTS, 0, points.size());
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (mShaderLoaded)
    {
        mShader->unbind();
    }
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, NULL);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, NULL);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, NULL);
//  gl::draw(mControlPointsTex, Rectf(-1, -1, 1, 1));
}

void Renderer::setControlPoints(ControlPointMap const & points)
{
    mControlPoints = points;
    updateCalculatedControlPoints();
}

ControlPointMap Renderer::controlPoints() const
{
    return mControlPoints;
}


struct VecComparer
{
    bool operator()(Vec2f const* a, Vec2f const* b)
    {
        return mDistSq[a] > mDistSq[b];
    }

    static map<Vec2f const*, float> mDistSq;
};

map<Vec2f const*, float> VecComparer::mDistSq;

bool compareVec2fLex(Vec2f const& a, Vec2f const& b)
{
    return
      a.x < b.x? true
    : a.x > b.x? false
    : a.y < b.y? true
    : a.y > b.y? false
               : false;
}

int fact(int t)
{
    int x = 1;
    for (int i=1; i<=t; ++i)
    {
        x *= i;
    }
    return x;
}

float calculateDistSqOfPath(vector<Vec2f> const& points)
{
    float distSq = 0.f;
    for (int l=1; l<points.size(); ++l)
    {
        distSq += points[l-1].distanceSquared(points[l]);
    }
    return distSq;
}

//
//ofVec2f Renderer::interp(int inst0, int inst1, float t)
//{
//  std::vector<ofVec2f> const& bezierPoints = mControlPoints[inst0][inst1];
//  // we need 4 points for a bezier segment
//  const int numSegments = bezierPoints.size() - 3;
//  int segment = min(numSegments-1, int(t*numSegments));
//  float p = t*numSegments - segment;
//  assert(0<=p && p<=1);
//  assert(segment < bezierPoints.size()-3);
//  return bezierInterp(bezierPoints[segment], bezierPoints[segment+1], bezierPoints[segment+2], bezierPoints[segment+3], p);
//
//}
//
//ofVec2f Renderer::interp(ofVec2f const& orig, ofVec2f const& dest, float t)
//{
//  priority_queue<ofVec2f const*, vector<ofVec2f const*>, VecComparer> queue;
//  for (ofVec2f const& v: mPoints)
//  {
//      float d = orig.distanceSquared(v);
//      VecComparer::mDistSq[&v] = d;
//      queue.push(&v);
//  }
//
//  const int NUM_CONTROL_POINTS = min<int>(4, mPoints.size());
//  // plus start and end
//  std::vector<ofVec2f const*> bezierPoints;
//  bezierPoints.reserve(NUM_CONTROL_POINTS+2);
//  bezierPoints.push_back(&orig);
//  for (int i=0; i<NUM_CONTROL_POINTS; ++i)
//  {
//      bezierPoints.push_back(queue.top());
//      queue.pop();
//  }
//  bezierPoints.push_back(&dest);
//  if (bezierPoints.size() < 4) // in case we have no control points
//  {
//      bezierPoints.push_back(&orig);
//      bezierPoints.push_back(&dest);
//  }
//  // we need 4 points for a bezier segment
//  const int numSegments = bezierPoints.size() - 3;
//  int segment = min(numSegments-1, int(t*numSegments));
//  float p = t*numSegments - segment;
//  assert(0<=p && p<=1);
//  assert(segment < bezierPoints.size()-3);
//  return bezierInterp(*bezierPoints[segment], *bezierPoints[segment+1], *bezierPoints[segment+2], *bezierPoints[segment+3], p);
//
//
//}

void Renderer::drawQuad(Vec2f const& pos, Vec2f const& size)
{
    glPushMatrix();
    gl::translate(pos);
    glScalef(size.x, size.y, 1);



    static GLfloat vertices[] = {
        -1., -1.,
        -1.,  1.,
         1.,  1.,
         1., -1.
    };
    static GLfloat uvs[] = {
        0., 0.,
        0., 1.,
        1., 1.,
        1., 0.
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

void Renderer::drawConnectionsDebug()
{
    auto& instruments = mState.instruments;
    for (int j=0; j<instruments.size(); ++j)
    {
        Instrument inst = instruments[j];
        for (int i=0; i<instruments.size(); ++i)
        {
            float f = 15*inst.connections.at(i);
            glColor4f(1,1,1,min(1.f,f)*0.5);
            gl::lineWidth(f);
            gl::drawLine(inst.pos, instruments.at(i).pos);
        }
    }
    gl::lineWidth(1.f);
}

template <typename T>
T Renderer::hermiteSpline(T const& point0, T const& tangent0, T const& point1, T const& tangent1, float s)
{
    float h1 =  2*s*s*s - 3*s*s + 1;          // calculate basis function 1
    float h2 = -2*s*s*s + 3*s*s;              // calculate basis function 2
    float h3 =   s*s*s  - 2*s*s + s;         // calculate basis function 3
    float h4 =   s*s*s  -  s*s;              // calculate basis function 4
    T p = h1*point0 +                    // multiply and sum all funtions
    h2*point1 +                    // together to build the interpolated
    h3*tangent0 +                    // point along the curve.
    h4*tangent1;
    return p;
}

template <typename T>
T Renderer::hermiteSpline(vector<T> const& points, float t)
{
    assert(points.size()>=2);
    const int numSegments = points.size() - 1;
    int segment = min(numSegments-1, int(t*numSegments));
    float p = t*numSegments - segment;
    assert(0<=p && p<=1);
    assert(segment < points.size()-1);
    // last segment is a special case
    // ...
    T tangent0 = points[segment+1]-points[segment];
    T tangent1 =
        segment+2>=points.size()? T()
                                : points[segment+2]-points[segment];
    return hermiteSpline(points[segment], tangent0, points[segment+1], tangent1, p);

}

Vec2f Renderer::interpHermite(int inst0, int inst1, float t) const
{
    vector<Vec2f> const & points = mCalculatedControlPoints.at(inst0).at(inst1);
    return hermiteSpline(points, t);
}

void Renderer::updateCalculatedControlPoints()
{
    // put onto texture for shader
    Surface32f surface(mControlPointsTex->getWidth(), mControlPointsTex->getHeight(), true, SurfaceChannelOrder::RGBA);

    // For every instrument pair
    for (int i=0; i<NUM_INSTRUMENTS; ++i)
    {
        for (int j=0; j<NUM_INSTRUMENTS; ++j)
        {
            // Get reference to points
            auto & points = mControlPoints[i][j];

            //
            assert(points.size() < MAX_CONTROL_POINTS);
            while (points.size() >= MAX_CONTROL_POINTS)
            {
                points.pop_back();
            }

            //
            mCalculatedControlPoints[i][j] = vector<Vec2f>(1 + points.size() + 1);
            auto& ps = mCalculatedControlPoints[i][j];
            ps[0] = mState.instruments.at(i).pos;
            ps[ps.size()-1] = mState.instruments.at(j).pos;
            copy(points.begin(), points.end(), ps.begin()+1);

            // for surface
            int x = i*NUM_INSTRUMENTS + j;
            assert(x < surface.getWidth());
            *surface.getDataRed(Vec2i(x, 0)) = ps.size();
            for (int k=0; k<ps.size(); k++)
            {
                assert(k < surface.getHeight());
                *surface.getDataRed(Vec2i(x, k+1)) = ps.at(k).x;
                *surface.getDataGreen(Vec2i(x, k+1)) = ps.at(k).y;
                // optimization - cache the tangents here as well
                Vec2f tangent = Vec2f(0,0);
                if (k+1 < ps.size())
                {
                    tangent = ps.at(k+1) - ps.at(k);
                }
                *surface.getDataBlue(Vec2i(x, k+1)) = tangent.x;
                *surface.getDataAlpha(Vec2i(x, k+1)) = tangent.y;
            }
        }
    }

    // copy surface to texture
    mControlPointsTex->update(surface);
}

