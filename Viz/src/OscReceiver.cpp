//
//  OscReceiver.cpp
//  EnsembleVisualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

#include <sstream>
#include "OscReceiver.h"
using namespace ci;
using namespace ci::osc;
using namespace std;


OscReceiver::OscReceiver()
: mHasNewState(false)
, mTimeListenPortMessageWasLastSent(-42)
, mListenPort(0)
, mHasANewStateEverHappened(false)
, mIsSetup(false)
{

}

void OscReceiver::setup(int port, std::string stabilizerHost, int stabilizerPort)
{
    mOsc.setup(port);
    mSender.setup(stabilizerHost, stabilizerPort);
    mListenPort = port;
    mStabilizerHost = stabilizerHost;
    mStabilizerPort = stabilizerPort;
    mIsSetup = true;
}

void OscReceiver::update(float i_timeSinceAppLaunch, float i_timeSinceLastUpdate)
{
    assert(mIsSetup);
    assert(mState.instruments.size() == NUM_INSTRUMENTS);

    // For each received OSC message
    Message m;
    while (mOsc.hasWaitingMessages())
    {
        m.clear();
        mOsc.getNextMessage(&m);

        //
        string address = m.getAddress();
        if (address == "/viz/narrative"
            && m.getArgType(0) == TYPE_FLOAT)
        {
            mState.narrative = m.getArgAsFloat(0);
        }
        else if (address == "/viz/note"
                 && m.getArgType(0) == TYPE_INT32
                 && m.getArgType(1) == TYPE_FLOAT)
        {
            int instrumentNo = m.getArgAsInt32(0);
            if (instrumentNo < 0 || instrumentNo >= NUM_INSTRUMENTS)
            {
                std::cout << "ERROR: Note has instrument "<<instrumentNo<<" which is out of bounds." << endl;
                continue;
            }
            Note note(i_timeSinceAppLaunch, m.getArgAsFloat(1));
            mState.instruments[instrumentNo].notes.push_back(note);
        }
        else if (address == "/viz/connections"
                 && m.getArgType(0) == TYPE_INT32)
        {
            const int num_insts = m.getArgAsInt32(0);
            if (num_insts > NUM_INSTRUMENTS)
            {
                std::cout << "ERROR: connections message sent with "<<num_insts<<" instruments but we are only setup to work with "<<NUM_INSTRUMENTS<<endl;
                continue;
            }
            else if (num_insts*num_insts + 1 != m.getNumArgs())
            {
                printf("ERROR: connections message declares %d instruments but has %d arguments\n", num_insts, m.getNumArgs());
                continue;
            }
//          std::cout << "state connections updated"<<endl;
//          std::cout << "new connections: ";
            for (int i=0; i<num_insts; i++)
            {
                for (int j=0; j<num_insts; j++)
                {
//                  // the + 1 is because the first argument is num_insts
                    float v = m.getArgAsFloat(i * num_insts + j + 1);
//                  std::cout << i << "->"<<j<<" "<<v<<", ";
                    mState.instruments.at(i).connections.at(j) = v;
                }
            }
//          std::cout << endl;
        }
        else if (address=="/viz/debug" && m.getArgType(0)==TYPE_INT32)
        {
            mState.debugMode = m.getArgAsInt32(0) != 0;
            // get names if they're there
            int num_names = std::min(NUM_INSTRUMENTS, m.getNumArgs()-1);
            for (int i=0; i<num_names; ++i)
                if (m.getArgType(i+1)==TYPE_STRING)
                    mState.instruments.at(i).name = m.getArgAsString(i+1);
        }
        mHasNewState = true;
        mHasANewStateEverHappened = true;
    }
    mState.update(i_timeSinceAppLaunch, i_timeSinceLastUpdate);

    if (i_timeSinceAppLaunch - mTimeListenPortMessageWasLastSent > 5)
    {
        Message m;
        m.setAddress("/viz/listen_port");
        m.addIntArg(mListenPort);
        mSender.sendMessage(m);
        mTimeListenPortMessageWasLastSent = i_timeSinceAppLaunch;
    }
}

bool OscReceiver::hasNewState() const
{
    return mHasNewState;
}

State OscReceiver::state() const
{
    return mState;
}

void OscReceiver::setState(State const& state)
{
    mState = state;
    mHasNewState = true;
}


/// For debugging
std::string OscReceiver::status() const
{
    std::stringstream ss;
    ss <<"OscReceiver (listen: "<<mListenPort<<" stabilizer: "<<mStabilizerHost<<':'<<mStabilizerPort<<") ";
    ss << (mHasANewStateEverHappened
        ? "OSC Data has been received"
        : "Yet to receive OSC data");
    return ss.str();
}


void OscReceiver::toggleDebugMode()
{
    mState.debugMode = !mState.debugMode;
    // send to stabilizer to prevent it overriding our value
    Message m;
    m.setAddress("/viz/debug");
    m.addIntArg(int(mState.debugMode));
    mSender.sendMessage(m);
    mHasNewState = true;
}
