//
//  ControlPointEditor.cpp
//  Visualization
//
//  Created by Tim Murray-Browne on 21/06/2013.
//
//

// This module
#include "ControlPointEditor.h"

// TMB
using tmb::Quad;

// OpenCV
#include <opencv/cv.hpp>

// Cinder
#include <cinder/Utilities.h>
#include <cinder/ImageIo.h>
//#include <cinder/Matrix.h>
using namespace ci;

// JsonCpp (bundled with Cinder)
#include <json.h>

// Boost
#include <boost/assign.hpp>

// C++ std
#include <ostream>
#include <fstream>
using namespace std;


// + Helpers pulled out of Cinder OpenCV block {{{
//#include "CinderOpenCV.h"

class ImageTargetCvMat : public ImageTarget {
  public:
	static std::shared_ptr<ImageTargetCvMat> createRef( cv::Mat *mat ) { return std::shared_ptr<ImageTargetCvMat>( new ImageTargetCvMat( mat ) ); }

	virtual bool hasAlpha() const { return mMat->channels() == 4; }	
	virtual void*	getRowPointer( int32_t row ) { return reinterpret_cast<void*>( reinterpret_cast<uint8_t*>(mMat->data) + row * mMat->step ); }
	
  protected:
	ImageTargetCvMat( cv::Mat *mat );
	
	cv::Mat		*mMat;
};

inline cv::Mat toOcv(ci::ImageSourceRef sourceRef, int type = -1)
{
	if( type == -1 )
    {
		int depth = CV_8U;
		if( sourceRef->getDataType() == ImageIo::UINT16 )
			depth = CV_16U;
		else if( sourceRef->getDataType() == ImageIo::FLOAT32 )
			depth = CV_32F;
		int channels = ImageIo::channelOrderNumChannels( sourceRef->getChannelOrder() );
		type = CV_MAKETYPE( depth, channels );
	}
	
	cv::Mat result( sourceRef->getHeight(), sourceRef->getWidth(), type );
	ImageTargetRef target = ImageTargetCvMat::createRef( &result );
	sourceRef->load( target );
	return result;
}

inline cv::Scalar toOcv( const Color &color )
{
	return CV_RGB( color.r * 255, color.g * 255, color.b * 255 );
}

inline cv::Point2f toOcv( const Vec2f &point )
{
	return cv::Point2f( point.x, point.y );
}

inline cv::Point toOcv( const Vec2i &point )
{
	return cv::Point( point.x, point.y );
}

inline cv::Rect toOcv( const ci::Area &r )
{
	return cv::Rect( r.x1, r.y1, r.getWidth(), r.getHeight() );
}

// + }}}


ControlPointEditor::ControlPointEditor()
    : mRenderer(NULL)
    , mIsInSetupMode(false)
    , mInstrumentVisibility(NUM_INSTRUMENTS, true)
    , mIsInWarpMode(false)
    , mOriginalQuad(Vec2f(-1, 1),Vec2f(1, 1),Vec2f(1, -1),Vec2f(-1, -1))
    , mEnableSecondHead(true)
    , mCurrentlyEditingSecondHead(false)
    , mRenderResolution(1080, 1080)
    , mHeadResolution(400, 400)
    , mIsSecondHeadRotated180(true)
    , mRotation(0)
{
    for (int i=0; i<NUM_INSTRUMENTS; i++)
    {
        mControlPoints[i] = map<int, std::vector<ci::Vec2f> >();
        for (int j=0; j<NUM_INSTRUMENTS; j++)
        {
            mControlPoints[i][j] = std::vector<ci::Vec2f>();
        }
    }
    mEditingInstruments[0] = NONE;
    mEditingInstruments[1] = NONE;
    for (int i=0; i<2; i++)
    {
        mWarpQuad[i] = Quad(Vec2f(-1, 1), Vec2f(1, 1), Vec2f(1, -1), Vec2f(-1, -1));

    }
}

ControlPointEditor::~ControlPointEditor()
{
    save();
}

void ControlPointEditor::setEnableSetupMode(bool enabled)
{
    mIsInSetupMode = enabled;
    notify();
}

// hack to find working directory
void ControlPointEditor::loadSettings()
{

    //std::vector<fs::path> dirs = {"assets", "../assets", "../../assets", "../../../assets", "../../../../assets", "../../../../../assets", "../../../../../../assets", "../../../../../../../assets", "../../../../../../../../assets", "../../../../../../../../../assets", "/Users/tim/Documents/Programming/Ensemble/EnsembleVisualization/Viz/assets" };
    std::vector<fs::path> dirs;
    dirs.push_back("assets");
    dirs.push_back("../assets");
    dirs.push_back("../../assets");
    dirs.push_back("../../../assets");
    dirs.push_back("../../../../assets");
    dirs.push_back("../../../../../assets");
    dirs.push_back("../../../../../../assets");
    dirs.push_back("../../../../../../../assets");
    dirs.push_back("../../../../../../../../assets");
    dirs.push_back("../../../../../../../../../assets");
    dirs.push_back("/Users/tim/Documents/Programming/Ensemble/EnsembleVisualization/Viz/assets");

    fs::path file = "";
    fs::path relativePath = "control_points.json";

    for( vector<fs::path>::const_iterator assetDirIt = dirs.begin(); assetDirIt != dirs.end(); ++assetDirIt ) {
        if( fs::exists( *assetDirIt / relativePath ) )
            file = ( *assetDirIt / relativePath );
        else
            std::cout << "Searching "<<*assetDirIt<<". " << ( *assetDirIt / relativePath ) << " does not exist. " <<endl;
    }
    if (file == "")
    {
        std::cerr << "ERROR: Failed to find control_points.json in assets folder(s)."<<std::endl;
    }

    mJsonFilename = file.string();
    load();
}

void ControlPointEditor::setup(Renderer* renderer)
{
    mRenderer = renderer;
    notify();
}

void ControlPointEditor::save()
{
    using namespace Json;
    using ci::toString;
    Value jRoot;
    Value& jPoints = jRoot["control points"];
    for (int i=0; i<NUM_INSTRUMENTS; i++)
        for (int j=0; j<NUM_INSTRUMENTS; j++)
        {
            jPoints[i][j] = Value(arrayValue);
            for (int k=0; k<mControlPoints[i][j].size(); k++)
                for (int l=0; l<mControlPoints[i][j][k].DIM; l++)
                    jPoints[i][j][k][l] = mControlPoints[i][j][k][l];
        }
    Value& jWarpQuads = jRoot["warp quads"];
    for (int i=0; i<2; i++)
    {
        Value& jWarpQuad = jWarpQuads[i];
        jWarpQuad["top left"] = toString(mWarpQuad[i].tl);
        jWarpQuad["top right"] = toString(mWarpQuad[i].tr);
        jWarpQuad["bottom right"] = toString(mWarpQuad[i].br);
        jWarpQuad["bottom left"] = toString(mWarpQuad[i].bl);
    }
    jRoot["render resolution"] = toString(mRenderResolution);
    jRoot["head resolution"] = toString(mHeadResolution);
    jRoot["enable second head"] = mEnableSecondHead;
    jRoot["rotation"] = mRotation;
    jRoot["host name"] = mHostName;
    ofstream out;
    out.open(mJsonFilename.c_str());
    if (out.good())
    {
        out << jRoot;
        out.close();
    }
    if (out.good())
    {
        cout << "Successfully written to file "<<mJsonFilename<<endl;
    }
    else
    {
        cout << "ERROR when writing to file "<<mJsonFilename<<endl;
    }
}

void ControlPointEditor::load()
{
    cout << "Loading "<<mJsonFilename<<"..."<<endl;
    using namespace Json;
    Value jRoot;
    Reader reader;
    ifstream in;
    in.open(mJsonFilename.c_str());
    bool success = reader.parse(in, jRoot);
    if (!success)
    {
        cout << "WARNING: Could not open "<<mJsonFilename<<":\n"
        << reader.getFormatedErrorMessages() << endl;
        return;
    }
    Value& jHostName = jRoot["host name"];
    if (jHostName.isNull())
    {
        cout << "WARNING: Could not find 'host name' element"<<endl;
        success = false;
    }
    else
    {
        mHostName = jHostName.asString();
        cout << "Loaded hostname: "<<mHostName<<endl;
    }
    Value& jRenderResolution = jRoot["render resolution"];
    if (jRenderResolution.isNull())
    {
        cout << "WARNING: Could not find 'render resolution' element"<<endl;
        success = false;
    }
    else
    {
        success = success && (jRenderResolution.asString() >> mRenderResolution);
    }
    Value& jHeadResolution = jRoot["head resolution"];
    if (jHeadResolution.isNull())
    {
        cout << "WARNING: Could not find 'head resolution' element"<<endl;
        success = false;
    }
    else
    {
        success = success && (jHeadResolution.asString() >> mHeadResolution);
    }
    Value& jEnableSecondHead = jRoot["enable second head"];
    if (!jEnableSecondHead.isBool())
    {
        cout << "WARNING: Could not find boolean 'enable second head' element"<<endl;
        success = false;
    }
    else
    {
        mEnableSecondHead = jEnableSecondHead.asBool();
    }
    Value& jRotation = jRoot["rotation"];
    if (!jRotation.isNumeric())
    {
        cout << "WARNING: Could not find real valued 'rotation' element"<<endl;
        success = false;
    }
    else
    {
        mRotation = jRotation.asDouble();
    }

    Value& jWarpQuads = jRoot["warp quads"];
    if (jWarpQuads.isNull())
    {
        cout << "WARNING: Could not find 'warp quads' element"<<endl;
        success = false;
    }
    else
    {
        bool warpSuccess = true;
        for (int i=0; i<2; i++)
        {
            Vec2f tl, tr, br, bl;
            warpSuccess =
            (jWarpQuads[i]["top left"].asString()>>tl)
            && (jWarpQuads[i]["top right"].asString()>>tr)
            && (jWarpQuads[i]["bottom right"].asString()>>br)
            && (jWarpQuads[i]["bottom left"].asString()>> bl);
            if (warpSuccess)
            {
                mWarpQuad[i] = tmb::Quad(tl, tr, br, bl);
            }
            else
            {
                cout << "WARNING: Failed to parse 'warp quad' element "<<i<<"."<<endl;
                success = false;
            }
        }
    }
    updateWarpTransform();

    Value& jControlPoints = jRoot["control points"];
    if (jControlPoints.isNull())
    {
        cout << "WARNING: Could not read "<<mJsonFilename<<" as could not find \"control points\" element"<<endl;
        success = false;
        return;
    }
    for (int i=0; i<NUM_INSTRUMENTS; i++)
    {
        Value& jPointsOrig = jControlPoints[i];
        if (jPointsOrig.isNull())
        {
            cout << "WARNING: Could not read control points from origin instrument "<<i<<endl;
            continue;
        }
        for (int j=0; j<NUM_INSTRUMENTS; j++)
        {
            if (i==j)
                continue;
            Value& jPoints =jPointsOrig[j];
//          int n = jPointsOrig.size();
            if (jPoints.isNull())
            {
                cout << "WARNING: Could not read control points from instrument "<<i<<" with destination "<<j<<" from "<<mJsonFilename<<endl;
                success = false;
                continue;
            }
            mControlPoints[i][j].clear();
            for (int k=0; k<jPoints.size(); k++)
            {
                Value jVec = jPoints[k];
                if (jVec.isNull() || jVec.size()!= ci::Vec2f::DIM)
                {
                    cout << "WARNING: Could not read control point "<<k<<" from instrument "<<i<<" to "<<j<<" from "<<mJsonFilename<<endl;
                    success = false;
                    continue;
                }
                ci::Vec2f v;
                assert(v.DIM == jVec.size());
                bool skipV = false;
                for (int l=0; l<jVec.size(); l++)
                {
                    if (!jVec[l].isConvertibleTo(realValue))
                    {
                        cout << "WARNING: Could not read component "<<l<<" from control point "<<k<<" from instrument "<<i<<" to "<<j<<" from "<<mJsonFilename<<endl;
                        skipV = true;
                        success = false;
                        break;
                    }
                    v[l] = (float) jVec[l].asDouble();
                }
                if (!skipV)
                {
                    mControlPoints[i][j].push_back(v);
                }
            }
        }
    }

    if (success)
    {
        cout << mJsonFilename << " loaded successfully."<<endl;
    }
    notify();
}

void ControlPointEditor::draw(float elapsedTime)
{

    State state = mRenderer->state();
    if (mWasDebugEnabledLastFrame != state.debugMode)
    {
        notify();
        mWasDebugEnabledLastFrame = state.debugMode;
    }
    if (!state.debugMode)
        return;
    auto instruments = state.instruments;

    ci::ColorAf editCol(0,1,0,0.8);
    ci::ColorAf visibleCol(1,1,1,0.4);
    ci::ColorAf editingControlPoint(.4,1.,.4,0.8);
    ci::ColorAf notEditingControlPoint(.5,.5,1,0.2);

    // normalized coordinates: 2x2 square centred at origin
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gl::rotate(mRotation);

    glBindTexture(GL_TEXTURE_2D, 0);
    ci::gl::enableAdditiveBlending();
    //  glColor4f(1,1,1,0.8);
    assert(instruments.size() == mInstrumentVisibility.size());
    for (int i=0; i<instruments.size(); ++i)
    {
        if (!mInstrumentVisibility.at(i))
            continue;
        Instrument inst = instruments.at(i);
        if (i==mEditingInstruments[0] || i==mEditingInstruments[1])
            gl::color(editCol);
        else
            gl::color(visibleCol);
        Vec2f pos = inst.pos;
//      pos.rotate(mRotation);
        gl::drawSolidEllipse(inst.pos, 0.2, 0.2, 30);
    }

    gl::lineWidth(1.f);
    glColor4f(0,0,0,1);
    for (int i=0; i<instruments.size(); ++i)
    {
        Instrument const& inst = instruments.at(i);
        gl::pushModelView();
        Vec2f pos = inst.pos;
//      pos.rotate(mRotation);
        gl::translate(inst.pos);
        gl::scale(0.004, -0.004);
        tmb::drawString(ci::toString(i)+" "+inst.name, Vec2f(), true);
        gl::popModelView();
//      ofDrawBitmapString(ofToString(i)+" "+inst.name, inst.pos - ci::Vec2f(0.04, 0));
    }
    //  glColor4f(1,0,0,0.5);
    for (int i=0; i<instruments.size(); ++i)
    {
        for (int j=0; j<i; ++j)
        {
            if (!mInstrumentVisibility.at(i) || !mInstrumentVisibility.at(j))
                continue;
            auto const& points = mControlPoints[i][j];
            if (isEditing(i) && isEditing(j))
                gl::color(editingControlPoint);
            else if (mInstrumentVisibility.at(i) && mInstrumentVisibility.at(j))
                gl::color(notEditingControlPoint);
            else
                continue;
            //for (ci::Vec2f const& point: points)
            for (std::vector<ci::Vec2f>::const_iterator pointItr = points.begin();
                 pointItr != points.end();
                 ++pointItr)
            {
                const ci::Vec2f & point = *pointItr;

                gl::drawSolidEllipse(point, 0.1, 0.1, 25);
            }
        }
    }

    if (mIsInWarpMode)
    {
        gl::color(.85, .85, 1., 0.875);
        glPushAttrib(GL_LINE_WIDTH);
        glLineWidth(3);
        // draw lines to help keystone
        for (int i=0; i<10; i++)
        {
            float x = i/9.f;
            gl::drawLine(Vec2f(x*2-1., -1), Vec2f(x*2.-1., +1));
        }
        for (int i=0; i<10; i++)
        {
            float y = i/9.f;
            gl::drawLine(Vec2f(-1, y*2-1.), Vec2f(+1, y*2-1));
        }
        glPopAttrib();
        // draw corners
        gl::color(1,0,0,0.9);
        float radius = 0.2;
        gl::drawSolidCircle(mOriginalQuad.tl, radius, 20);
        gl::drawSolidCircle(mOriginalQuad.tr, radius, 20);
        gl::drawSolidCircle(mOriginalQuad.br, radius, 20);
        gl::drawSolidCircle(mOriginalQuad.bl, radius, 20);
    }

    // status text
//  gl::setMatricesWindow(app::getWindowWidth(), app::getWindowHeight());
    gl::setMatricesWindow(gl::getViewport().getWidth(), gl::getViewport().getHeight());
//  ofSetupScreen();
    gl::color(255, 163, 183);
    gl::enableAlphaBlending();
    tmb::drawString(mStatus, ci::Vec2f(10, 20), false);

}

void ControlPointEditor::keyPressed(ci::app::KeyEvent event)
{
    char key = event.getChar();
    bool ctrlPressed = event.isControlDown();
    bool altPressed = event.isAltDown();
//  bool shiftPressed = event.isShiftDown();

    if (mRenderer->state().debugMode)
    {
        if (key=='d')
        {
            editInstrument(NONE);
            editInstrument(NONE);
        }
        if (key=='-' || ('0' <= key && key < '8'))
        {
            int inst = key=='-'? NONE : key - '0';
            if (!ctrlPressed && !altPressed && mIsInSetupMode)
            {
                editInstrument(inst);
            }
            else if (ctrlPressed && !altPressed)
            {
                // change all when - is pressed
                if (inst == -1)
                    mInstrumentVisibility.assign(mInstrumentVisibility.size(), !mInstrumentVisibility.at(0));
                else
                    mInstrumentVisibility.at(inst) = !mInstrumentVisibility.at(inst);
            }
            else if (mIsInWarpMode)
            {
                if (key=='0')
                    mCurrentlyEditingSecondHead = false;
                else if (key=='1')
                    mCurrentlyEditingSecondHead = true;
            }
        }
        else if (mIsInSetupMode && event.getCode()==cinder::app::KeyEvent::KEY_BACKSPACE && ctrlPressed)
        {
            clearPoints();
        }
        else if (mIsInWarpMode && event.getCode()==cinder::app::KeyEvent::KEY_BACKSPACE && ctrlPressed)
        {
            mWarpQuad[0] = mOriginalQuad;
            mWarpQuad[1] = mOriginalQuad;
            updateWarpTransform();
        }
        else if (key=='e')
        {
            mIsInSetupMode = !mIsInSetupMode;
            if (mIsInSetupMode)
            {
                mIsInWarpMode = false;
            }
            else
            {
                editInstrument(NONE);
                editInstrument(NONE);
            }

        }
        else if (key=='c')
        {
            mRenderer->setEnableDrawConnectionsDebug(!mRenderer->isDrawConnectionsDebugEnabled());
        }
        else if (key=='w')
        {
            mIsInWarpMode = !mIsInWarpMode;
            if (mIsInWarpMode && mIsInSetupMode)
            {
                mIsInSetupMode = false;
                editInstrument(NONE);
                editInstrument(NONE);
            }
        }
    }
    notify();
}

void ControlPointEditor::clearPoints()
{
    //for (auto& orig: mControlPoints)
    for (std::map<int, std::map<int, std::vector<ci::Vec2f> > >::iterator origItr = mControlPoints.begin();
         origItr != mControlPoints.end();
         ++origItr)
    {
        std::map<int, std::vector<ci::Vec2f> > & orig = origItr->second;

        //for (auto& dest: orig.second)
        for (std::map<int, std::vector<ci::Vec2f> >::iterator destItr = orig.begin();
             destItr != orig.end();
             ++destItr)
        {
            std::map<int, std::vector<ci::Vec2f> >::value_type dest = *destItr;

            dest.second.clear();
        }
    }
    cout << "All control points cleared."<<endl;
}

void ControlPointEditor::mouseDragged(ci::Vec2f const& pos, int button)
{
    cout << "pos "<<pos<<endl;
    if (mIsInWarpMode)
    {
        Vec2f p = pos;
        if (mEnableSecondHead)
        {
            if (mCurrentlyEditingSecondHead)
                // [0,1] to [-1, 1]
                p.x = p.x * 2 - 1;
            else
                // [-1, 0] to [-1, 1]
                p.x = p.x * 2 + 1;
        }
        p -= mDragOffset;
        int headIndex = mCurrentlyEditingSecondHead? 1 : 0;
        switch (mCurrentlyBeingDragged)
        {
            case TL:
                mWarpQuad[headIndex].tl = p;
                break;
            case TR:
                mWarpQuad[headIndex].tr = p;
                break;
            case BR:
                mWarpQuad[headIndex].br = p;
                break;
            case BL:
                mWarpQuad[headIndex].bl = p;
                break;
            default:
                break;
        }
    }
    updateWarpTransform();
}


void ControlPointEditor::updateWarpTransform()
{
    cout << "left warp quad "<<mWarpQuad[0]<<endl;
    cout << "right warp quad "<<mWarpQuad[1]<<endl;

    using namespace cv;
    // quad warping drawing on http://forum.openframeworks.cc/index.php/topic,509.msg2429.html#msg2429
    for (int i=0; i<2; i++)
    {
        vector<Point2f> src = boost::assign::list_of
            (toOcv(mOriginalQuad.tl))
            (toOcv(mOriginalQuad.tr))
            (toOcv(mOriginalQuad.br))
            (toOcv(mOriginalQuad.bl));
        vector<Point2f> dst = boost::assign::list_of
            (toOcv(mWarpQuad[i].tl))
            (toOcv(mWarpQuad[i].tr))
            (toOcv(mWarpQuad[i].br))
            (toOcv(mWarpQuad[i].bl));
        Mat transform(findHomography(src, dst));
        assert(transform.type()==CV_64F);
        vector<double> glTransform(16, 0.f);
        // [From oF theo:]
        //we need to copy these values
        //from the 3x3 2D openCV matrix which is row ordered
        //
        // ie:   [0][1][2] x
        //       [3][4][5] y
        //       [6][7][8] w

        //to openGL's 4x4 3D column ordered matrix
        //        x  y  z  w
        // ie:   [0][3][ ][6]
        //       [1][4][ ][7]
        //       [ ][ ][ ][ ]
        //       [2][5][ ][9]
        //
        mWarpTransform[i].m[0] = transform.at<double>(0);
        mWarpTransform[i].m[4] = transform.at<double>(1);
        mWarpTransform[i].m[12] = transform.at<double>(2);

        mWarpTransform[i].m[1] = transform.at<double>(3);
        mWarpTransform[i].m[5] = transform.at<double>(4);
        mWarpTransform[i].m[13] = transform.at<double>(5);

        mWarpTransform[i].m[3] = transform.at<double>(6);
        mWarpTransform[i].m[7] = transform.at<double>(7);
        mWarpTransform[i].m[15] = transform.at<double>(8);
    }
}

void ControlPointEditor::mouseReleased(int button)
{
    const int LEFT = 0;
    if (button==LEFT)
    {
        mCurrentlyBeingDragged = NONE;
        mDragOffset = Vec2f(0,0);
    }
}

void ControlPointEditor::mousePressed(ci::Vec2f const& pos, int button)
{
    static const int LEFT = 0;
    static const int RIGHT = 2;
    if (!isEditing(NONE))
    {
        // always consider points from lowest value instrument to
        // highest value instrument
        int inst0 = min(mEditingInstruments[0], mEditingInstruments[1]);
        int inst1 = max(mEditingInstruments[0], mEditingInstruments[1]);

        auto& points0 = mControlPoints.at(inst0).at(inst1);
        auto& points1 = mControlPoints.at(inst1).at(inst0);
        if (button==LEFT)
        {
            if (points0.size() < MAX_CONTROL_POINTS)
            {
                points0.push_back(pos);
                // higher valued instrument we insert the control points
                // in reverse order
                points1.insert(points1.begin(), pos);
            }
            else
            {
                cout << "Cannot add control point as the maximum number ("<<MAX_CONTROL_POINTS<<") has been reached."<<endl;
            }
        }
        else if (button==RIGHT)
        {
            if (!points0.empty())
                points0.pop_back();
            if (!points1.empty())
                points1.erase(points1.begin());
        }
    }
    else if (mIsInWarpMode && button==LEFT)
    {
        Vec2f p = pos;
        cout << "orig pos " << pos << endl;
        if (mEnableSecondHead)
        {
            if (mCurrentlyEditingSecondHead)
                // [0,1] to [-1, 1]
                p.x = p.x * 2 - 1;
            else
                // [-1, 0] to [-1, 1]
                p.x = p.x * 2 + 1;
        }
        cout << "p " << p << endl;
        int headIndex = mCurrentlyEditingSecondHead? 1 : 0;
        const float radius = 0.2;
        if (p.distance(mWarpQuad[headIndex].tl) < radius)
        {
            mCurrentlyBeingDragged = TL;
            mDragOffset = p - mWarpQuad[headIndex].tl;
        }
        else if (p.distance(mWarpQuad[headIndex].tr) < radius)
        {
            mCurrentlyBeingDragged = TR;
            mDragOffset = p - mWarpQuad[headIndex].tr;
        }
        else if (p.distance(mWarpQuad[headIndex].bl) < radius)
        {
            mCurrentlyBeingDragged = BL;
            mDragOffset = p - mWarpQuad[headIndex].bl;
        }
        else if (p.distance(mWarpQuad[headIndex].br) < radius)
        {
            mCurrentlyBeingDragged = BR;
            mDragOffset = p - mWarpQuad[headIndex].br;
        }
        else
        {
            mCurrentlyBeingDragged = NONE;
            mDragOffset = Vec2f(0,0);
        }
    }
    notify();
}

void ControlPointEditor::notify()
{
    if (mRenderer != NULL)
    {
        mRenderer->setControlPoints(mControlPoints);
        mRenderer->setRotation(mRotation);
    }
    mStatus = "S to save, L to load, C to draw connections, P to print state, R to randomize state,\nM for maximal state, space to toggle debug interface, E to toggle control point editor\nW to toggle warp editing mode";
    if (mIsInSetupMode)
        mStatus += "\nEditing: "+toString(mEditingInstruments[0])+" "
        + getName(mEditingInstruments[0])+" and "+toString(mEditingInstruments[1])+" "
        + getName(mEditingInstruments[1])+"\n<num> to select instruments, control <num> to change visibility"
        + "\n(use '-' for no/all instrument(s)), d to deselect, Ctrl Backspace to clear all control points\nStart points at low-numbered instrument";
    if (mIsInWarpMode)
    {
        mStatus += "\nWarp editor: Currently editing ";
        mStatus += mCurrentlyEditingSecondHead? "RIGHT" : "LEFT";
        mStatus += " head.\nCtrl Backspace to reset the warp quad Drag the corners to warp the screen.\n0 to edit left head, 1 to edit right head.";
    }
}

string ControlPointEditor::getName(int instrumentNumber) const
{
    if (instrumentNumber<0)
        return "(none)";
    else if (instrumentNumber<NUM_INSTRUMENTS)
        return mRenderer->state().instruments.at(instrumentNumber).name;
    else
        return "error";
}

void ControlPointEditor::editInstrument(int inst)
{
    if (inst!=NONE && isEditing(inst))
        return;
    if (mEditingInstruments[1] != NONE || inst==NONE)
        mEditingInstruments[0] = mEditingInstruments[1];
    mEditingInstruments[1] = inst;
}
