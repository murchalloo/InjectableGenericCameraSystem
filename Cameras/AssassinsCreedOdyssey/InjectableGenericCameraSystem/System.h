////////////////////////////////////////////////////////////////////////////////////////////////////////
// Part of Injectable Generic Camera System
// Copyright(c) 2019, Frans Bouma
// All rights reserved.
// https://github.com/FransBouma/InjectableGenericCameraSystem
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
//  * Redistributions of source code must retain the above copyright notice, this
//	  list of conditions and the following disclaimer.
//
//  * Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and / or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "stdafx.h"
#include "Camera.h"
#include "Gamepad.h"
#include <map>
#include "AOBBlock.h"

namespace IGCS
{
	class System
	{
	public:
		System();
		~System();
		void start(LPBYTE hostBaseAddress, DWORD hostImageSize);

	private:
		void mainLoop();
		void initialize();
		void updateFrame();
		void handleUserInput();
		void handleBokehRendering();
		void displayCameraState();
		void toggleCameraMovementLockState(bool newValue);
		void handleKeyboardCameraMovement(float multiplier);
		void handleMouseCameraMovement(float multiplier);
		void handleGamePadMovement(float multiplierBase);
		void waitForCameraStructAddresses();
		void toggleInputBlockState(bool newValue);
		void toggleTimestopState();
		void toggleHudRenderState();
		void takeMultiShot(bool isTestRun);
		void takeSingleScreenshot();
		void takeBokehScreenshot();
		
		
		Camera _camera;
		LPBYTE _hostImageAddress;
		DWORD _hostImageSize;
		bool _timeStopped = false;
		bool _cameraMovementLocked = false;
		bool _cameraStructFound = false;
		bool _hudToggled = false;
		map<string, AOBBlock*> _aobBlocks;
		bool _applyHammerPrevention = false;	// set to true by a keyboard action and which triggers a sleep before keyboard handling is performed.

		/// //////////////////////////////////////
		double _x = 0.0;
		double _y = 0.0;
		double _z = 0.0;
		bool _firstFrame = false;
		bool _bokehRendering = false;

		// bokeh settings
		double _radius = 0.001f;
		float _ringIndex = 1.0f;
		float _pointNumber = 1.0f;
		float _actualOffSet = 0.0f;
		float _roundness = 1.0f;
		double _amount;
		int _pointsFirstRing = 8;
		int _pointsOnRing = 0;
		double _currentRingRadiusCoords = 0;
		double _ringRadiusDeltaCoords = 0;
		double _anamorphX = 1.0f;
		double _anamorphY = 1.0f;
		double _angle;
		double _anglePerPoint;
		float _shotCounter = 0.0f;
		double _angleX = 0.0;
		double _angleY = 0.0;
		double _dot = 0.0f;
		double _focus = 0.0f;
		
	};
}

