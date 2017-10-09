//
//  ControlPointEditor.h
//  Visualization
//
//  Created by Tim Murray-Browne on 21/06/2013.
//
//

#pragma once

#include "Common.h"
#include "Renderer.h"
#include "cinder/Matrix.h"

class ControlPointEditor
{
public:
    ControlPointEditor();
    ~ControlPointEditor();

    /// Load settings from json
    void loadSettings();
    void setup(Renderer* renderer);
    /// Will only draw when setupmode is enabled
    void draw(float elapsedTime);

    void save();
    void load();

    void setEnableSetupMode(bool enabled);

    void keyPressed(ci::app::KeyEvent event);
    /// Pos is in normalized coordinates
    /// button is 0 for LEFT, 2 for RIGHT
    void mousePressed(ci::Vec2f const& pos, int button);
    /// Pos is in normalized coordinates
    /// button is 0 for LEFT, 2 for RIGHT
    void mouseDragged(ci::Vec2f const& pos, int button);
    void mouseReleased(int button);

    ci::Matrix44d warpTransform(bool getRightHead) const { return mWarpTransform[(int) getRightHead]; }
    ci::Vec2i renderResolution() const { return mRenderResolution; }
    ci::Vec2i headResolution() const { return mHeadResolution; }
    bool isSecondHeadEnabled() const { return mEnableSecondHead; }
    int numHeads() const { return isSecondHeadEnabled()? 2 : 1; }
    bool isSecondHeadRotated180() const { return mIsSecondHeadRotated180; }
    /// How much the instruments should be rotated
    float rotation() const { return mRotation; }
    /// Hostname of the stabilizer
    std::string hostName() const { return mHostName; }

private:
    /// Call to update stuff when something changes
    void notify();
    void clearPoints();

    bool mIsInSetupMode;
    bool mIsInWarpMode;
    Renderer* mRenderer;
    std::string mJsonFilename;
    std::map<int, std::map<int, std::vector<ci::Vec2f> > > mControlPoints;

    ci::Vec2i mRenderResolution;
    ci::Vec2i mHeadResolution;

    // warp editor
    /// we can have two warp quads, one for each head
    bool mEnableSecondHead;
    bool mIsSecondHeadRotated180;
    /// the quad things get drawn to
    tmb::Quad mWarpQuad[2];
    /// the noramlized coordinate quad (-1 to +1)
    tmb::Quad mOriginalQuad;
    enum Corner_ { TL, TR, BR, BL };
    typedef int Corner;
    Corner mCurrentlyBeingDragged;
    bool mCurrentlyEditingSecondHead;
    ci::Vec2f mDragOffset;
    ci::Matrix44d mWarpTransform[2];
    /// Set mWarpTransform based on mWarpQuad
    void updateWarpTransform();
    float mRotation; ///< In radians

    std::string mStatus;
    std::string getName(int instrumentNumber) const;

    bool mWasDebugEnabledLastFrame;

    // editing mode:
    static const int NONE = -1;
    int mEditingInstruments[2];
    void editInstrument(int inst);
    bool isEditing(int inst) { return mEditingInstruments[0]==inst || mEditingInstruments[1] == inst; }
    std::vector<bool> mInstrumentVisibility;

    template <typename T>
    void push_front(std::vector<T>& dest, T const& value);

    std::string mHostName;
};

