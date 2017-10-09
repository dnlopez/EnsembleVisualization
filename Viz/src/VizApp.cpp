
// This program
#include "Common.h"
#include "Renderer.h"
#include "State.h"
#include "OscReceiver.h"
#include "ControlPointEditor.h"

// Cinder
#include <cinder/app/AppNative.h>
#include <cinder/gl/gl.h>
#include <cinder/gl/Fbo.h>
#include <cinder/Utilities.h>
#include <cinder/ImageIo.h>
using namespace ci;

// Boost
#include <boost/assign.hpp>
#include <boost/date_time.hpp>

// C std
#include <ctime>
using namespace std;


const bool RECORD_FRAMES = false;
const std::string RECORD_FRAMES_PATH = "/Users/tim/viz_recording";
std::string RECORD_FRAMES_PREFIX;
int gFrameNumber = 0;


class VizApp : public ci::app::AppNative
{
public:
    VizApp();

    void prepareSettings(Settings * i_settings);
    void setup();
    void mouseDown( ci::app::MouseEvent event );
    void mouseMove(ci::app::MouseEvent event);
    void mouseDrag(ci::app::MouseEvent event);
    void mouseUp(ci::app::MouseEvent event);
    void keyDown(ci::app::KeyEvent event);
    void keyUp(ci::app::KeyEvent event);
    void update();
    void draw();


private:
    void saveScreenshot();
    void toggleFullScreen();

    int mListenPort;
    std::string mStabilizerHost;
    int mStabilizerPort;

    float mTimeOfLastUpdate;
    float mDt;
    Renderer* mRenderer;
    OscReceiver mOscReceiver;
    ControlPointEditor mEditor;

    // for setting the control points. only shown on debug
    std::string getName(int instrumentNumber);
    /// which instrument control points are visible
    vector<bool> mInstrumentVisibility;
    /// being currently edited - set to -1 for none
    int mCurrentOrig;
    int mCurrentDest;
    bool mPrintFrameRate;

    // to allow warping, everything is drawn to fbo
    ci::gl::Fbo mFbo;
    /// receives warped left head stream
    ci::gl::Fbo mLeftHead;
    ci::gl::Fbo mRightHead;
    // FBO size can be different from window size
    ci::Vec2i mRenderResolution;
    ci::Vec2i mHeadResolution;
};

VizApp::VizApp()
    : mTimeOfLastUpdate(-1.0)
    , mDt(1.0)

    , mStabilizerHost("127.0.0.1")
    , mStabilizerPort(1123)
    , mListenPort(12378)

    , mRenderer(NULL)
    , mInstrumentVisibility(NUM_INSTRUMENTS, true)
    , mCurrentOrig(-1)
    , mCurrentDest(-1)
    , mPrintFrameRate(false)
    , mRenderResolution(1500, 1500)
    , mHeadResolution(400, 400)
{
}

void VizApp::prepareSettings(Settings * i_settings)
{
    // set display to secondary display if available
    vector<DisplayRef> displays = Display::getDisplays();
    if (displays.size() > 1)
    {
        i_settings->setDisplay(displays.at(1));
    }

    //
    mEditor.loadSettings();
    mStabilizerHost = mEditor.hostName();
    mRenderResolution = mEditor.renderResolution();
    mHeadResolution = mEditor.headResolution();
    i_settings->setWindowSize(mHeadResolution.x*mEditor.numHeads(), mHeadResolution.y);
}

std::string dateString()
{
    std::ostringstream msg;
    const boost::posix_time::ptime now =    boost::posix_time::second_clock::local_time();
    boost::posix_time::time_facet *const f =    new boost::posix_time::time_facet("%Y%m%d%H%M%S");
    msg.imbue(std::locale(msg.getloc(),f));
    msg << now;
    string s = msg.str();
    return s;
}

void VizApp::setup()
{
    RECORD_FRAMES_PREFIX = dateString();

    mRenderer = new Renderer;
    mRenderer->setState(State::randomState(0));
    mOscReceiver.setup(mListenPort, mStabilizerHost, mStabilizerPort);
    mEditor.setup(mRenderer);
    mFbo = ci::gl::Fbo(mRenderResolution.x, mRenderResolution.y, true);
    mLeftHead = ci::gl::Fbo(mHeadResolution.x, mHeadResolution.y, true);
    mRightHead = ci::gl::Fbo(mHeadResolution.x, mHeadResolution.y, true);

    vector<ci::gl::Fbo*> fbos = boost::assign::list_of
        (&mFbo)(&mLeftHead)(&mRightHead);
    for (auto it=fbos.begin(); it!=fbos.end(); ++it)
    {
        (**it).bindFramebuffer();
        ci::gl::clear(Color::black());
        (**it).unbindFramebuffer();
    }
}

void VizApp::mouseDown( ci::app::MouseEvent event )
{
    using namespace ci;
    using namespace ci::app;
    Vec2f pos = Vec2f(float(event.getPos().x)/getWindowWidth() * 2 - 1, float(getWindowHeight() - event.getPos().y)/getWindowHeight() * 2 - 1);

    // legacy oF stuff
    static const int LEFT = 0;
    static const int RIGHT = 2;
    int button = -1;
    if (event.isLeft())
        button = LEFT;
    else if (event.isMiddle())
        button = 1;
    else if (event.isRight())
        button = RIGHT;

    mEditor.mousePressed(pos, button);
}

void VizApp::mouseMove(ci::app::MouseEvent event)
{
    mx = event.getPos().x / (float)getWindowWidth();
    my = event.getPos().y / (float)getWindowHeight();
}

void VizApp::mouseDrag(ci::app::MouseEvent event)
{
    using namespace ci;
    using namespace ci::app;
    mouseMove(event);
    Vec2f pos = Vec2f(float(event.getPos().x)/getWindowWidth() * 2 - 1, float(getWindowHeight() - event.getPos().y)/getWindowHeight() * 2 - 1);
    int button = event.isLeft()? 0
        : event.isRight()? 2
        : event.isMiddle()? 1
        : -1;
    mEditor.mouseDragged(pos, button);
}

void VizApp::mouseUp(ci::app::MouseEvent event)
{
    int button = event.isLeft()? 0
        : event.isRight()? 2
        : event.isMiddle()? 1
        : -1;
    mEditor.mouseReleased(button);
}

void VizApp::keyDown(ci::app::KeyEvent event)
{
    using namespace ci;
    using namespace ci::app;
    char key = event.getChar();
//  bool ctrl = glutGetModifiers() & GLUT_ACTIVE_CTRL;
//  bool alt = glutGetModifiers() & GLUT_ACTIVE_ALT;

    if (key=='r')
    {
        bool d = mOscReceiver.state().debugMode;
        State state = State::randomState(getElapsedSeconds());
        state.debugMode = d;
        mOscReceiver.setState(state);
    }
    else if (key=='m')
    {
        bool d = mOscReceiver.state().debugMode;
        State state = State::maximalState(getElapsedSeconds());
        state.debugMode = d;
        mOscReceiver.setState(state);
    }
    else if (key=='s')
        mEditor.save();
    else if (key=='l')
        mEditor.load();
    else if (key==' ')
        mOscReceiver.toggleDebugMode();
    else if (key=='p')
    {
        std::cout << "Renderer state:\n"<<mRenderer->state()<<endl;
        std::cout << "OscReceiver status:\n"<<mOscReceiver.status()<<endl;
    }

    else if (key=='a')
        mRenderer->loadShader();
    else if (key=='f')
    {
        if (event.isAccelDown())
        {
            toggleFullScreen();
        }
        else
        {
            mPrintFrameRate = !mPrintFrameRate;
        }
    }
    else if (key=='.')
        saveScreenshot();
    mEditor.keyPressed(event);
}

void VizApp::keyUp(ci::app::KeyEvent event)
{

}

std::string VizApp::getName(int instrumentNumber)
{
    if (instrumentNumber==-1)
        return "(none)";
    else if (instrumentNumber<NUM_INSTRUMENTS)
        return mOscReceiver.state().instruments.at(instrumentNumber).name;
    else return "error";
}

void VizApp::toggleFullScreen()
{
    if (isFullScreen())
    {
        setFullScreen(false);
    }
    else
    {
        setFullScreen(true);
    }
}

void VizApp::saveScreenshot()
{
    using namespace ci;
    Surface8u screenshot(mFbo.getTexture());
    // make filename

    time_t t = time(0);   // get time now
    struct tm * now = localtime( & t );
    char buf[128];
    strftime(buf, sizeof(buf), "%Y-%m-%d-%HH-%MM-%SS", now);
    string filename = string("Ensemble_visualizer_screenshot_")+buf+"_"+toString(getElapsedFrames())+".png";
    fs::path outPath = fs::path("~") / filename;
    writeImage(outPath, screenshot);
    std::cout << "Screenshot saved to " << outPath << endl;
}

void VizApp::update()
{
    using namespace ci;
    using namespace ci::app;

    // Get current time (since application was launched), work out delta since last update,
    // and save new current time
    float currentTime = getElapsedSeconds();
    //  If recording then fake the current time to have a fixed delta
    if (RECORD_FRAMES)
        currentTime = mTimeOfLastUpdate += 0.04;  // [shouldn't it be + and not += ?]
    float timeSinceLastUpdate = currentTime - mTimeOfLastUpdate;
    mTimeOfLastUpdate = currentTime;

    //
    mOscReceiver.update(mTimeOfLastUpdate, timeSinceLastUpdate);
    if (mOscReceiver.hasNewState())
    {
        mRenderer->setState(mOscReceiver.state());
    }

    // debugging
    if (0)
    {
        if (getElapsedFrames()%30 == 0)
        {
            cout << mOscReceiver.status() << endl;
            cout << "State at time "<<mTimeOfLastUpdate<<"\n"
                 << mOscReceiver.state() << endl;
        }
        cout << 1.f/timeSinceLastUpdate << endl;
    }

    if (mPrintFrameRate)
    {
        cout << "timeSinceLastUpdate: " <<timeSinceLastUpdate << "\nfps: " << 1.f/timeSinceLastUpdate << endl;
    }
}

void VizApp::draw()
{
    using namespace ci;
    using namespace tmb;

    // + Draw visualization to offscreen mFbo {{{

    // Return if mFbo has not yet been created, otherwise binding it crashes.
    // This is an issue on Windows where it seems that draw() may get called before setup().
    if (!mFbo)
        return;
    mFbo.bindFramebuffer();

    gl::setViewport(mFbo.getBounds());
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Clear background to black
        //if (getElapsedFrames()%5==0)
        //    gl::clear( Color( 0, 0, 0 ) );

        gl::enableAlphaBlending();
        gl::color(ColorA(0, 0, 0, 0.7));
        gl::drawSolidRect(Rectf(-1, -1, 1, 1));

        // Render particles
        mRenderer->draw(mTimeOfLastUpdate);

        // Render control point editor
        mEditor.draw(mTimeOfLastUpdate);
    }

    mFbo.unbindFramebuffer();

    // + }}}

    if (RECORD_FRAMES)
    {
        ci::writeImage(fs::path(RECORD_FRAMES_PATH) / fs::path(RECORD_FRAMES_PREFIX + "_" + toString(gFrameNumber++)+".png"), mFbo.getTexture());
    }

    gl::clear(Color::black());
    gl::color(Color::white());

    // Warp
    int numHeads = mEditor.isSecondHeadEnabled()? 2 : 1;
    vector<gl::Fbo *> headFbos = boost::assign::list_of(&mLeftHead)(&mRightHead);
    for (int headNo = 0; headNo < numHeads; ++headNo)
    {
        headFbos.at(headNo)->bindFramebuffer();
        gl::clear(ColorA::black());
        gl::setViewport(headFbos.at(headNo)->getBounds());
        gl::disableAlphaBlending();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMultMatrixd(mEditor.warpTransform(headNo));
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
//      glTranslatef(headNo/float(numHeads), 0, 0);
        tmb::Quad quad = Quad();
        mFbo.bindTexture();
        glBegin(GL_QUADS);
        {
            gl::texCoord(0, 1);
            gl::vertex(quad.tl);
            gl::texCoord(1, 1);
            gl::vertex(quad.tr);
            gl::texCoord(1, 0);
            gl::vertex(quad.br);
            gl::texCoord(0, 0);
            gl::vertex(quad.bl);
        }
        glEnd();
        mFbo.unbindTexture();
        headFbos.at(headNo)->unbindFramebuffer();
    }

    // Draw
    gl::setViewport(getWindowBounds());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    for (int headNo = 0; headNo < numHeads; ++headNo)
    {
        float x0 = headNo/float(numHeads) * 2 - 1;
        float x1 = (headNo+1)/float(numHeads) * 2 - 1;
        Vec2f tl(x0, 1), tr(x1, 1), br(x1, -1), bl(x0, -1);
        headFbos.at(headNo)->bindTexture();
        glBegin(GL_QUADS);
        {
            gl::texCoord(0, 1);
            gl::vertex(tl);
            gl::texCoord(1, 1);
            gl::vertex(tr);
            gl::texCoord(1, 0);
            gl::vertex(br);
            gl::texCoord(0, 0);
            gl::vertex(bl);
        }
        glEnd();
        headFbos.at(headNo)->unbindTexture();
    }
    //glFinish();
}

CINDER_APP_NATIVE( VizApp, ci::app::RendererGl )

// On Windows, CINDER_APP_NATIVE defines WinMain() which is normally what's
// needed, but if one wants to get a console window by using the linker option
// /SUBSYSTEM:CONSOLE, the linker also expects to find the traditional main()
// instead, defined here.
// Warning: the following code throws away any command-line arguments.
#if defined(CINDER_MSW)
int main(int argc, char * const argv[])
{
    cinder::app::AppBasic::prepareLaunch();
    cinder::app::AppBasic *app = new VizApp;
    cinder::app::RendererRef ren(new ci::app::RendererGl);
    cinder::app::AppBasic::executeLaunch(app, ren, "VizApp");
    cinder::app::AppBasic::cleanupLaunch();
    return 0;
}
#endif
