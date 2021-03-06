//
//  OscReceiver.h
//  EnsembleVisualization
//
//  Created by Tim Murray-Browne on 08/06/2013.
//
//

#pragma once
#include <string>
#include "State.h"
//#include "ofxOsc.h"
#include "OscListener.h"
#include "OscSender.h"

class OscReceiver
{
public:
	OscReceiver();
	void setup(int listenPort, std::string stabilizerHost, int stabilizerPort);
	void update(float elapsedTime, float dt);
	
	bool hasNewState() const;
	State state() const;
	/// Manually set state - will be overwritten by any osc data
	/// Also will be rotated
	void setState(State const& state);

	/// OSC receiver manages the state but debug mode can be
	/// triggered manually here
	void toggleDebugMode();

	/// For debugging
	std::string status() const;

private:
	ci::osc::Listener mOsc;
	ci::osc::Sender mSender;
	State mState;
	
	int mListenPort;
	std::string mStabilizerHost;
	int mStabilizerPort;
	
	bool mHasNewState;
	float mTimeListenPortMessageWasLastSent;
	bool mHasANewStateEverHappened;
	bool mIsSetup;
};