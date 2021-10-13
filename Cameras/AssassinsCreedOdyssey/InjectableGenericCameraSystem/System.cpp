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
#include "stdafx.h"
#include "System.h"
#include "Globals.h"
#include "Defaults.h"
#include "GameConstants.h"
#include "Gamepad.h"
#include "CameraManipulator.h"
#include "InterceptorHelper.h"
#include "InputHooker.h"
#include "input.h"
#include "CameraManipulator.h"
#include "GameImageHooker.h"
#include "D3D11Hooker.h"
#include "OverlayConsole.h"
#include "OverlayControl.h"
#include "MinHook.h"
#include <time.h>
#include <cmath>
#include <direct.h>
#include "ScreenshotController.h"

namespace IGCS
{
	using namespace IGCS::GameSpecific;

	System::System()
	{
	}


	System::~System()
	{
	}


	void System::start(LPBYTE hostBaseAddress, DWORD hostImageSize)
	{
		Globals::instance().systemActive(true);
		_hostImageAddress = (LPBYTE)hostBaseAddress;
		_hostImageSize = hostImageSize;
		Globals::instance().gamePad().setInvertLStickY(CONTROLLER_Y_INVERT);
		Globals::instance().gamePad().setInvertRStickY(CONTROLLER_Y_INVERT);
		initialize();		// will block till camera is found
		mainLoop();
	}


	// Core loop of the system
	void System::mainLoop()
	{
		while (Globals::instance().systemActive())
		{
			if (Globals::instance().checkBokehButton()) {
				if (!_firstFrame) {
					Sleep(round(1000.0f / ImGui::GetIO().Framerate)); // ceil/floor/round
				}
				else {
					_firstFrame = false;
				}
			}
			else {
				Sleep(FRAME_SLEEP);
			}
			updateFrame();
		}
	}


	// updates the data and camera for a frame 
	void System::updateFrame()
	{
		handleBokehRendering();
		if (!Globals::instance().checkBokehButton())
		{
			handleUserInput();
		}

		CameraManipulator::updateCameraDataInGameData(_camera);
		/*if (Globals::instance().checkBokehButton())
		{
			Sleep(round(1000.0f / ImGui::GetIO().Framerate)); // ceil/floor/round
			_camera.resetMovement();

			_camera.pitchBokeh(-_angleY); //Y
			_camera.yawBokeh(-_angleX); //X

			_camera.moveUpBokeh(-_x);
			_camera.moveRightBokeh(-_y);
		}*/
	}


	void System::handleUserInput()
	{
		Globals::instance().gamePad().update();
		if (_applyHammerPrevention)
		{
			_applyHammerPrevention = false;
			// sleep main thread for 200ms so key repeat delay is simulated. 
			Sleep(300);
		}

		if (Input::isActionActivated(ActionType::ToggleOverlay))
		{
			OverlayControl::toggleOverlay();
			_applyHammerPrevention = true;
		}
		if (!_cameraStructFound)
		{
			// camera not found yet, can't proceed.
			return;
		}
		if (OverlayControl::isMainMenuVisible() && !Globals::instance().settings().allowCameraMovementWhenMenuIsUp)
		{
			// stop here, so keys used in the camera system won't affect anything of the camera
			return;
		}
		if (Input::isActionActivated(ActionType::CameraEnable))
		{
			if (g_cameraEnabled)
			{
				// it's going to be disabled, make sure things are alright when we give it back to the host
				CameraManipulator::restoreOriginalValuesAfterCameraDisable();
				toggleCameraMovementLockState(false);
				InterceptorHelper::toggleDofEnableWrite(_aobBlocks, true);
				// disable screenshot action
				Globals::instance().getScreenshotController().reset();
			}
			else
			{
				// it's going to be enabled, so cache the original values before we enable it so we can restore it afterwards
				CameraManipulator::cacheOriginalValuesBeforeCameraEnable();
				_camera.resetAngles();
				InterceptorHelper::toggleDofEnableWrite(_aobBlocks, false);
			}
			g_cameraEnabled = g_cameraEnabled == 0 ? (BYTE)1 : (BYTE)0;
			displayCameraState();
			CameraManipulator::killInGameDofIfNeeded();
			_applyHammerPrevention = true;
		}
		if (Input::isActionActivated(ActionType::FovReset) && Globals::instance().keyboardMouseControlCamera())
		{
			CameraManipulator::resetFoV();
		}
		if (Input::isActionActivated(ActionType::FovDecrease) && Globals::instance().keyboardMouseControlCamera())
		{
			CameraManipulator::changeFoV(-Globals::instance().settings().fovChangeSpeed);
		}
		if (Input::isActionActivated(ActionType::FovIncrease) && Globals::instance().keyboardMouseControlCamera())
		{
			CameraManipulator::changeFoV(Globals::instance().settings().fovChangeSpeed);
		}
		if (Input::isActionActivated(ActionType::Timestop))
		{
			toggleTimestopState();
			_applyHammerPrevention = true;
		}

		if (Input::isActionActivated(ActionType::HudToggle))
		{
			toggleHudRenderState();
			_applyHammerPrevention = true;
		}

		if (!g_cameraEnabled)
		{
			// camera is disabled. We simply disable all input to the camera movement, by returning now.
			return;
		}
		if (Input::isActionActivated(ActionType::TakeScreenshot))
		{
			takeSingleScreenshot();
			_applyHammerPrevention = true;
			return;
		}
		if (Input::isActionActivated(ActionType::TestMultiShotSetup))
		{
			takeMultiShot(true);
			_applyHammerPrevention = true;
			return;
		}
		if (Input::isActionActivated(ActionType::TakeMultiShot))
		{
			takeMultiShot(false);
			_applyHammerPrevention = true;
			return;
		}
		if (Input::isActionActivated(ActionType::TakeBokehShot))
		{
			takeBokehScreenshot();
			_applyHammerPrevention = true;
			return;
		}
		if (Input::isActionActivated(ActionType::BlockInput))
		{
			toggleInputBlockState(!Globals::instance().inputBlocked());
			_applyHammerPrevention = true;
		}
		_camera.resetMovement();
		Settings& settings = Globals::instance().settings();
		if (Input::isActionActivated(ActionType::CameraLock))
		{
			toggleCameraMovementLockState(!_cameraMovementLocked);
			_applyHammerPrevention = true;
		}
		if (_cameraMovementLocked)
		{
			// no movement allowed, simply return
			return;
		}

		bool altPressed = Utils::altPressed();
		bool rcontrolPressed = Utils::keyDown(VK_RCONTROL);
		float multiplier = altPressed ? settings.fastMovementMultiplier : rcontrolPressed ? settings.slowMovementMultiplier : 1.0f;
		handleKeyboardCameraMovement(multiplier);
		handleMouseCameraMovement(multiplier);
		handleGamePadMovement(multiplier);

	}





	void System::handleBokehRendering()
	{
		//////////////////////////////////////////////////////////////////////////////////////////////// //checking camera movement
		if (Globals::instance().settings().testMove) {
			if (Globals::instance().checkBokehButton())
			{
				if (Globals::instance().settings().upRight) {
					_camera.moveRightBokeh(1.0f * _radius);
				}
				else {
					_camera.moveUpBokeh(1.0f * _radius);
				}
				Globals::instance().offsetValuesX((1.0f * _radius) * (1.f - Globals::instance().settings().focusingDistance) / (_radius / Globals::instance().settings().focusingCorrection) / GameSpecific::CameraManipulator::getCurrentFoV());
				Globals::instance().offsetValuesY((-1.0f * _radius) * (1.f - Globals::instance().settings().focusingDistance) / (_radius / Globals::instance().settings().focusingCorrection) / GameSpecific::CameraManipulator::getCurrentFoV());
				OverlayConsole::instance().logLine(" 1 unit offset X: %f offset Y: %f", Globals::instance().offsetValuesX(), Globals::instance().offsetValuesY());
				//
				Globals::instance().checkBokehButton(false);
			}
			else {
				return;
			}
		}
		////////////////////////////////////////////////////////////////////////////////////////////////
		else {
			
			if (_applyHammerPrevention)
			{
				_applyHammerPrevention = false;
				// sleep main thread for 200ms so key repeat delay is simulated. 
				Sleep(300);
			}

			if (Input::isActionActivated(ActionType::TakeBokehShot))
			{
				takeBokehScreenshot();
				_applyHammerPrevention = true;
				return;
			}

			if (!Globals::instance().checkBokehButton())
			{
				//disable rendering while processing
				_ringIndex = 1.0f;
				_pointNumber = 1.0f;
				if (_bokehRendering) {
					_camera.yawBokeh(-_angleX); //X
					_camera.pitchBokeh(-_angleY); //Y
					GameSpecific::CameraManipulator::restoreOriginalValuesAfterMultiShot();
				}
				_bokehRendering = false;
				return;
			}
			else {

				_camera.resetMovement();

				////////////////////////////////////////////////////////////////////////////////////////////////

				if (!_bokehRendering) //defaulting all necessary values
				{
					_bokehRendering = true;
					_x = 0.0;
					_y = 0.0;
					_z = 0.0;
					_ringIndex = 1.0f;
					_pointNumber = 1.0f;
					_pointsOnRing = _pointsFirstRing;

					//exact final samples count for blending

					////////////////////////////////////////////////////////////////////////////////////////////////

					int samplesCounter = 0;
					for (float ringIndex = 0; ringIndex < Globals::instance().settings().numberOfRings; ringIndex++)
					{
						for (float pointNumber = 0; pointNumber < _pointsOnRing; pointNumber++)
						{
							samplesCounter++;
						}
						_pointsOnRing += _pointsFirstRing;
					}
					Globals::instance().shotsToTake(samplesCounter);

					////////////////////////////////////////////////////////////////////////////////////////////////

					if (!Globals::instance().settings().onlyLastShot) {
						Globals::instance().getScreenshotController().startBokehShot(false); //first sample capturing
					}

					//anamorphic value

					////////////////////////////////////////////////////////////////////////////////////////////////

					if (Globals::instance().settings().anamorphism > 0.0f) {
						_anamorphX = 1.0f - abs(Globals::instance().settings().anamorphism);
						_anamorphY = 1.0f;
					}
					else if (Globals::instance().settings().anamorphism < 0.0f) {
						_anamorphY = 1.0f - abs(Globals::instance().settings().anamorphism);
						_anamorphX = 1.0f;
					}
					else {
						_anamorphX = 1.0f;
						_anamorphY = 1.0f;
					}
					////////////////////////////////////////////////////////////////////////////////////////////////

					_ringRadiusDeltaCoords = (_radius * (double)(Globals::instance().settings().shapeSize)) / (double)Globals::instance().settings().numberOfRings;
					_currentRingRadiusCoords = _ringRadiusDeltaCoords;
					_pointsOnRing = _pointsFirstRing;
					_anglePerPoint = (double)DirectX::XM_2PI / (double)_pointsOnRing;
					_angle = _anglePerPoint;
					_shotCounter = 0.0f;
					_angleX = 0.0f;
					_angleY = 0.0f;
					_focus = (double)Globals::instance().settings().focusingDistance;

					if (Globals::instance().settings().offSetType) {
						_actualOffSet = Globals::instance().settings().offSet * _ringIndex;
					}
					else {
						_actualOffSet = Globals::instance().settings().offSet;
					}
				}

				//reseting camera position and rotation to 0 before move to next point

				////////////////////////////////////////////////////////////////////////////////////////////////

				/*_camera.moveForwardBokeh(-_z);*/ //z astigmatism immitation test

				if (_angleY != 0.0) _camera.pitchBokeh(-_angleY); //Y
				if (_angleX != 0.0) _camera.yawBokeh(-_angleX); //X

				if (_x != 0.0) _camera.moveUpBokeh(-_x);
				if (_y != 0.0) _camera.moveRightBokeh(-_y);

				////////////////////////////////////////////////////////////////////////////////////////////////

				//calculating new camera position

				////////////////////////////////////////////////////////////////////////////////////////////////

				_x = (double)(_currentRingRadiusCoords * _anamorphX * sin(_angle + _actualOffSet));
				_y = (double)(_currentRingRadiusCoords * _anamorphY * cos(_angle + _actualOffSet));

				if (_y != 0.0) _camera.moveRightBokeh(_y);
				if (_x != 0.0) _camera.moveUpBokeh(_x);

				////////////////////////////////////////////////////////////////////////////////////////////////

				// camera rotation method (close distance broken)

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				_angleX = (atan2(_focus, 0.0) - atan2(_focus, -_y)); 
				_angleY = (atan2(_focus, 0.0) - atan2(_focus, _x));

				if (_angleX != 0.0) _camera.yawBokeh(_angleX); //X
				if (_angleY != 0.0) _camera.pitchBokeh(_angleY); //Y

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				// image offset method (close distance broken too)

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				//Globals::instance().offsetValuesX(-_y * (1.f - Globals::instance().settings().focusingDistance) / (_radius / Globals::instance().settings().focusingCorrection) / GameSpecific::CameraManipulator::getCurrentFoV());
				//Globals::instance().offsetValuesY(_x * (1.f - Globals::instance().settings().focusingDistance) / (_radius / Globals::instance().settings().focusingCorrection) / GameSpecific::CameraManipulator::getCurrentFoV());
				//Globals::instance().offsetValuesX(-_y * (Globals::instance().settings().focusingDistance) / GameSpecific::CameraManipulator::getCurrentFoV());
				//Globals::instance().offsetValuesY(_x * (Globals::instance().settings().focusingDistance) / GameSpecific::CameraManipulator::getCurrentFoV());
				//Globals::instance().offsetValuesX(-_y * (double)(Globals::instance().settings().focusingDistance) * 9.0);
				//Globals::instance().offsetValuesY(_x * (double)(Globals::instance().settings().focusingDistance) * 9.0);

				//_shotCounter++;
				//Globals::instance().shotCounter(_shotCounter);

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				//z astigmatism immitation test (broken cuz of broken rotation)

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				//_z = (double)(Globals::instance().settings().zPosCorrection * (((double)(max(abs(_x), abs(_y)))/(_focus*100.0f))*_ringIndex)); 
				//_camera.moveForwardBokeh(_z);

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				if (!Globals::instance().settings().onlyLastShot) {
					Globals::instance().getScreenshotController().startBokehShot(false);
				}

				///////////////////////////////////

				_angle += _anglePerPoint;
				_pointNumber++;

				if (_pointNumber > _pointsOnRing)
				{
					_pointNumber = 1.0f;
					_ringIndex++;
					_pointsOnRing += _pointsFirstRing;
					_anglePerPoint = (double)(DirectX::XM_2PI) / _pointsOnRing;
					_currentRingRadiusCoords += _ringRadiusDeltaCoords;
				}

				if (_ringIndex > Globals::instance().settings().numberOfRings)
				{
					if (Globals::instance().settings().onlyLastShot) {
						Globals::instance().getScreenshotController().startBokehShot(false);
					}
					Globals::instance().getScreenshotController().startBokehShot(true);
					Globals::instance().checkBokehButton(false);
					//OverlayConsole::instance().logLine("shots overlay: %f actual shots: %f", Globals::instance().shotsToTake(), _shotCounter);
					GameSpecific::CameraManipulator::restoreOriginalValuesAfterMultiShot();
					return;
				}
			}
		}
	}


	void System::handleGamePadMovement(float multiplierBase)
	{
		if (!Globals::instance().controllerControlsCamera())
		{
			return;
		}

		auto gamePad = Globals::instance().gamePad();

		if (gamePad.isConnected())
		{
			Settings& settings = Globals::instance().settings();
			float  multiplier = gamePad.isButtonPressed(IGCS_BUTTON_FASTER) ? settings.fastMovementMultiplier
				: gamePad.isButtonPressed(IGCS_BUTTON_SLOWER) ? settings.slowMovementMultiplier : multiplierBase;
			vec2 rightStickPosition = gamePad.getRStickPosition();
			_camera.pitch(rightStickPosition.y * multiplier);
			_camera.yaw(rightStickPosition.x * multiplier);

			vec2 leftStickPosition = gamePad.getLStickPosition();
			_camera.moveUp((gamePad.getLTrigger() - gamePad.getRTrigger()) * multiplier);
			_camera.moveForward(leftStickPosition.y * multiplier);
			_camera.moveRight(leftStickPosition.x * multiplier);

			if (gamePad.isButtonPressed(IGCS_BUTTON_TILT_LEFT))
			{
				_camera.roll(multiplier);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_TILT_RIGHT))
			{
				_camera.roll(-multiplier);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_RESET_FOV))
			{
				CameraManipulator::resetFoV();
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_FOV_DECREASE))
			{
				CameraManipulator::changeFoV(-Globals::instance().settings().fovChangeSpeed);
			}
			if (gamePad.isButtonPressed(IGCS_BUTTON_FOV_INCREASE))
			{
				CameraManipulator::changeFoV(Globals::instance().settings().fovChangeSpeed);
			}
		}
	}


	void System::handleMouseCameraMovement(float multiplier)
	{
		if (!Globals::instance().keyboardMouseControlCamera())
		{
			return;
		}
		long mouseDeltaX = Input::getMouseDeltaX();
		long mouseDeltaY = Input::getMouseDeltaY();
		if (abs(mouseDeltaY) > 1)
		{
			_camera.pitch(-(static_cast<float>(mouseDeltaY) * MOUSE_SPEED_CORRECTION * multiplier));
		}
		if (abs(mouseDeltaX) > 1)
		{
			_camera.yaw(static_cast<float>(mouseDeltaX) * MOUSE_SPEED_CORRECTION * multiplier);
		}
		Input::resetMouseDeltas();
	}


	void System::handleKeyboardCameraMovement(float multiplier)
	{
		if (!Globals::instance().keyboardMouseControlCamera())
		{
			return;
		}
		if (Input::isActionActivated(ActionType::MoveForward, true))
		{
			_camera.moveForward(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveBackward, true))
		{
			_camera.moveForward(-multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveRight, true))
		{
			_camera.moveRight(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveLeft, true))
		{
			_camera.moveRight(-multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveUp, true))
		{
			_camera.moveUp(multiplier);
		}
		if (Input::isActionActivated(ActionType::MoveDown, true))
		{
			_camera.moveUp(-multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateDown, true))
		{
			_camera.pitch(-multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateUp, true))
		{
			_camera.pitch(multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateRight, true))
		{
			_camera.yaw(multiplier);
		}
		if (Input::isActionActivated(ActionType::RotateLeft, true))
		{
			_camera.yaw(-multiplier);
		}
		if (Input::isActionActivated(ActionType::TiltLeft, true))
		{
			_camera.roll(multiplier);
		}
		if (Input::isActionActivated(ActionType::TiltRight, true))
		{
			_camera.roll(-multiplier);
		}
	}


	// Initializes system. Will block till camera struct is found.
	void System::initialize()
	{
		MH_Initialize();
		OverlayControl::init();
		// first grab the window handle
		Globals::instance().mainWindowHandle(Utils::findMainWindow(GetCurrentProcessId()));
		// then initialize imgui and the rest.
		OverlayControl::initImGui();
		InputHooker::setInputHooks();
		Input::registerRawInput();
		D3D11Hooker::initializeHook();

		GameSpecific::InterceptorHelper::initializeAOBBlocks(_hostImageAddress, _hostImageSize, _aobBlocks);
		GameSpecific::InterceptorHelper::setCameraStructInterceptorHook(_aobBlocks);
		waitForCameraStructAddresses();		// blocks till camera is found.
		GameSpecific::InterceptorHelper::setPostCameraStructHooks(_aobBlocks);

		// camera struct found, init our own camera object now and hook into game code which uses camera.
		_cameraStructFound = true;
		_camera.setPitch(INITIAL_PITCH_RADIANS);
		_camera.setRoll(INITIAL_ROLL_RADIANS);
		_camera.setYaw(INITIAL_YAW_RADIANS);

		// apply initial settings
		CameraManipulator::applySettingsToGameState();
	}


	// Waits for the interceptor to pick up the camera struct address. Should only return if address is found 
	void System::waitForCameraStructAddresses()
	{
		OverlayConsole::instance().logLine("Waiting for camera struct interception...");
		while (!GameSpecific::CameraManipulator::isCameraFound())
		{
			handleUserInput();
			Sleep(100);
		}
		OverlayControl::addNotification("Camera found.");
		GameSpecific::CameraManipulator::displayCameraStructAddress();
	}


	void System::toggleCameraMovementLockState(bool newValue)
	{
		if (_cameraMovementLocked == newValue)
		{
			// already in this state. Ignore
			return;
		}
		_cameraMovementLocked = newValue;
		OverlayControl::addNotification(_cameraMovementLocked ? "Camera movement is locked" : "Camera movement is unlocked");
	}


	void System::toggleInputBlockState(bool newValue)
	{
		if (Globals::instance().inputBlocked() == newValue)
		{
			// already in this state. Ignore
			return;
		}
		Globals::instance().inputBlocked(newValue);
		OverlayControl::addNotification(newValue ? "Input to game blocked" : "Input to game enabled");
	}


	void System::displayCameraState()
	{
		OverlayControl::addNotification(g_cameraEnabled ? "Camera enabled" : "Camera disabled");
	}


	void System::toggleTimestopState()
	{
		_timeStopped = !_timeStopped;
		OverlayControl::addNotification(_timeStopped ? "Game paused" : "Game unpaused");
		CameraManipulator::setTimeStopValue(_timeStopped);
	}


	void System::toggleHudRenderState()
	{
		_hudToggled = !_hudToggled;
		InterceptorHelper::toggleHudRenderState(_aobBlocks, _hudToggled);
	}


	void System::takeMultiShot(bool isTestRun)
	{
		// first cache the camera state
		GameSpecific::CameraManipulator::cacheOriginalValuesBeforeMultiShot();

		Settings& settings = Globals::instance().settings();
		// calls won't return till the process has been completed. 
		switch (static_cast<ScreenshotType>(settings.typeOfScreenshot))
		{
		case ScreenshotType::HorizontalPanorama:
		{
			// The total fov of the pano is always given in degrees. So we have to calculate that back to radians for usage with our camera.
			float totalPanoAngleInDegrees = Utils::clamp(settings.totalPanoAngleDegrees, 30.0f, 360.0f, 110.0f);
			float totalPanoAngleInRadians = (totalPanoAngleInDegrees / 180.0f) * DirectX::XM_PI;
			float currentFoVInRadians = Utils::clamp(CameraManipulator::getCurrentFoV(), 0.01f, 3.1f, 1.34f);		// clamp it to max 180degrees. 
			// if total fov is < than current fov, why bother with a pano?
			if (currentFoVInRadians > 0.0f && currentFoVInRadians < totalPanoAngleInRadians)
			{
				// take the shots
				Globals::instance().getScreenshotController().startHorizontalPanoramaShot(_camera, totalPanoAngleInRadians,
					Utils::clamp(settings.overlapPercentagePerPanoShot, 0.1f, 99.0f, 70.0f),
					currentFoVInRadians, isTestRun);
			}
			else
			{
				OverlayControl::addNotification("The total panorama angle is smaller than the current field of view, so just take a single screenshot instead.");
			}
		}
		break;
		case ScreenshotType::Lightfield:
			Globals::instance().getScreenshotController().startLightfieldShot(_camera, settings.distanceBetweenLightfieldShots, settings.numberOfShotsToTake, isTestRun);
			break;
		}
		// restore camera state
		GameSpecific::CameraManipulator::restoreOriginalValuesAfterMultiShot();
	}


	void System::takeSingleScreenshot()
	{
		// calls won't return till the process has been completed. 
		Globals::instance().getScreenshotController().startSingleShot();
	}


	void System::takeBokehScreenshot()
	{
		// calls won't return till the process has been completed. 
		if (!Globals::instance().checkBokehButton()) {
			GameSpecific::CameraManipulator::cacheOriginalValuesBeforeMultiShot();
			_firstFrame = true;
			Globals::instance().checkBokehButton(true);
		}
		else {
			Globals::instance().checkBokehButton(false);
			GameSpecific::CameraManipulator::restoreOriginalValuesAfterMultiShot();
		}
	}
}