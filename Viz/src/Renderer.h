//
//  Renderer.h
//  EnsembleVisualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

#pragma once


// This program
#include "State.h"
#include "Common.h"

// Cinder
#include <cinder/gl/Texture.h>
#include <cinder/Perlin.h>
#include <cinder/gl/GlslProg.h>


class Renderer
{
public:
    Renderer();
    virtual ~Renderer();

    void setState(State const & newState);
    State state() const;

    void draw(float elapsedTime);

    /// control points for splines
    void setControlPoints(ControlPointMap const& points);
    ControlPointMap controlPoints() const;

    void setEnableDrawConnectionsDebug(bool enabled);
    bool isDrawConnectionsDebugEnabled() const { return mEnableDrawConnectionsDebug; }

    void loadShader();

    void setRotation(float radians) { mRotation = radians; }

private:
    void render(float elapsedTime, std::vector<ci::Vec4f> const& points);
    void drawQuad(ci::Vec2f const& pos, ci::Vec2f const& size);
    void drawConnectionsDebug();
//  ci::Vec2f interp(ci::Vec2f const& orig, ci::Vec2f const& dest, float t);
//  // optimization of above
//  ci::Vec2f interp(int inst0, int inst1, float t);
    bool mEnableDrawConnectionsDebug;
    State mState;
    ci::gl::TextureRef mParticleTex;
    ci::gl::TextureRef mRandomTex;
    /// point n for inst i to inst j is (i*NUM_INSTRUMENTS+j, n+1)
    /// (i*NUM_INSTRUMENTS+j, 0) gives number of control points
    /// red and green are x,y of control point
    /// blue and alpha are x,y of tangent at that point
    ci::gl::TextureRef mControlPointsTex;
    std::vector<std::vector<float> > mRandoms;
    int mNumParticles;
    int mNumRandoms; // per particle
//  int mRandomsRows;
//  int mRandomsCols;
    float mRotation;

    ci::gl::GlslProgRef mShader;
    bool mShaderLoaded;

    /// just control points
    // Instrument 1 number -> Instrument 2 number -> Control points
    std::map< int, std::map< int, std::vector<ci::Vec2f> > > mControlPoints;
    /// including start and end points too
    // Instrument 1 number -> Instrument 2 number -> Instrument 1 position, Control points, Instrument 2 position
    std::map< int, std::map< int, std::vector<ci::Vec2f> > > mCalculatedControlPoints;
    void updateCalculatedControlPoints();

    float mx, my;

    template <typename T>
    static T hermiteSpline(T const& point0, T const& tangent0, T const& point1, T const& tangent1, float t);

    template <typename T>
    static T hermiteSpline(std::vector<T> const& points, float t);

    ci::Vec2f interpHermite(int inst0, int inst1, float t) const;

    ci::Perlin mPerlin;
};

//template<typename T, typename L>
//T bezierInterp( const T &a, const T &b, const T &c, const T &d, L t)
//{
//    L t1 = static_cast<L>(1.0) - t;
//    return a*(t1*t1*t1) + b*(3*t*t1*t1) + c*(3*t*t*t1) + d*(t*t*t);
//}
