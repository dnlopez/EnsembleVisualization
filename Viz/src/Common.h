//
//  Common.h
//  Visualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

#pragma once

// Cinder
#include <cinder/app/AppNative.h>
#include <cinder/gl/gl.h>
#include <cinder/Font.h>
#include <cinder/Vector.h>

// C++ std
#include <string>
#include <vector>
#include <iostream>
#include <ios>
#include <map>
#include <sstream>


typedef std::map< int, std::map< int, std::vector<ci::Vec2f> > > ControlPointMap;

template <typename T>
T sq(T const & x)
// Square a number
{
    return x*x;
}

const static double PI = 3.14159265359;

// maximum number of control points per particle
// This value is used directly in the shader as well
const static int MAX_CONTROL_POINTS = 50;

//inline
//ofVec2f toNorm(ofVec2f const& v)
//{
//  return ofVec2f(v.x/ofGetWidth()*2.f-1.f, 1.f-v.y/ofGetHeight()*2.f);
//}

inline
ci::Vec2f toNorm(ci::Vec2f const & v)
// Convert 2D point in window pixel coordinates
// to GL NDC-style coordinates (-1 to +1, Y increasing upwards)
{
    return ci::Vec2f(      v.x / ci::app::getWindowWidth() * 2.0f - 1.0f,
                     1.f - v.y / ci::app::getWindowHeight() * 2.0f);
}

inline
ci::Vec2f toNorm(int x, int y)
{
    return toNorm(ci::Vec2f(x,y));
}


template <typename T>
std::ostream & operator<<(std::ostream & o_outstream, std::vector<T> i_vector)
// Stream output operator for printing std::vector to text
{
    o_outstream << "vector: [\n";
    for (T const & element: i_vector)
    {
        o_outstream << element << '\n';
    }
    return o_outstream << ']';
}

/// mouse x normalized
extern float mx;
/// mouse y normalized
extern float my;

namespace tmb
{
    void drawString(std::string const& text, ci::Vec2f const& position, bool isCentered = true, ci::ColorA const& color=ci::ColorA(1, .6, .45, 1.));

    struct Quad
    {
        ci::Vec2f tl, tr, br, bl;

        Quad()
            : tl(-1, 1)
            , tr(1, 1)
            , br(1, -1)
            , bl(-1, -1)
        {}

        Quad(ci::Vec2f const & tl_, ci::Vec2f const & tr_, ci::Vec2f const & br_, ci::Vec2f const & bl_)
            : tl(tl_)
            , tr(tr_)
            , br(br_)
            , bl(bl_)
        {}
    };

    std::ostream & operator<<(std::ostream & i_outstream, Quad i_quad);
}

namespace cinder
{
    template<typename T>
    std::istream & operator>>(std::istream & i_stream, Vec2<T> & o_vector)
    // Stream input operator for reading text to Vec2
    {
        // Read character, expecting it to be a '['
        // Set fail state if no such character found
        char c = '\0';
        if (!(i_stream >> c) || c != '[')
        {
            i_stream.setstate(std::ios_base::failbit);
        }

        //
        Vec2f v;
        if (i_stream)
            i_stream >> v.x;
        if (i_stream && (!(i_stream >> c) || c != ','))
            i_stream.setstate(std::ios_base::failbit);
        if (i_stream)
            i_stream >> v.y;
        if (i_stream && (!(i_stream >> c) || c != ']'))
            i_stream.setstate(std::ios_base::failbit);
        if (i_stream)
            o_vector = v;

        // Return stream, for chaining
        return i_stream;
    }

    template<typename T>
    bool operator>>(std::string const & i_string, Vec2<T> & o_vector)
    {
        std::istringstream ss(i_string);
        ss >> o_vector;
        return ss.good();
    }
}
