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
#include "ScreenshotController.h"
#include "Utils.h"
#include "Defaults.h"
#include "OverlayConsole.h"
#include "OverlayControl.h"
#include <direct.h>
#include "CameraManipulator.h"
#include "Globals.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "stb_image_write.h"

#define ChannelBlend_Add(A, B)        ((float)(min(255, (A + B))))
#define BYTE_BOUND(value) value < 0 ? 0 : (value > 255 ? 255 : value)

double fastPow(double a, double b) {
	union {
		double d;
		int x[2];
	} u = { a };
	u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
	u.x[0] = 0;
	return u.d;
}

using namespace std;

namespace IGCS
{
	ScreenshotController::ScreenshotController() : _camera{ Camera() }
	{
	}

	ScreenshotController::~ScreenshotController()
	{}


	void ScreenshotController::configure(string rootFolder, int numberOfFramesToWaitBetweenSteps, float movementSpeed, float rotationSpeed)
	{
		if (_state != ScreenshotControllerState::Off)
		{
			// Initialize can't be called when a screenhot is in progress. ignore
			return;
		}
		_rootFolder = rootFolder;
		_numberOfFramesToWaitBetweenSteps = numberOfFramesToWaitBetweenSteps;
		_movementSpeed = movementSpeed;
		_rotationSpeed = rotationSpeed;
	}


	bool ScreenshotController::shouldTakeShot()
	{
		if (_convolutionFrameCounter > 0)
		{
			// always false
			return false;
		}
		return _state == ScreenshotControllerState::Grabbing;
	}

	bool ScreenshotController::isBokehShot()
	{
		if (_typeOfShot == ScreenshotType::BokehShot) {
			return true;
		}
		return false;
	}


	void ScreenshotController::presentCalled()
	{
		if (_convolutionFrameCounter > 0)
		{
			_convolutionFrameCounter--;
		}
	}


	void ScreenshotController::setBufferSize(int width, int height)
	{
		if (_state != ScreenshotControllerState::Off)
		{
			// ignore
			return;
		}
		_framebufferHeight = height;
		_framebufferWidth = width;
	}


	void ScreenshotController::startSingleShot()
	{
		OverlayConsole::instance().logDebug("strtSingleShot start.");
		reset();
		_typeOfShot = ScreenshotType::SingleShot;
		_state = ScreenshotControllerState::Grabbing;
		// we'll wait now till all the shots are taken. 
		waitForShots();
		OverlayControl::addNotification("Single screenshot taken. Writing to disk...");
		saveGrabbedShots();
		OverlayControl::addNotification("Single screenshot done.");
		// done
	}

	void ScreenshotController::startHorizontalPanoramaShot(Camera camera, float totalFoV, float overlapPercentagePerPanoShot, float currentFoV, bool isTestRun)
	{
		reset();
		_camera = camera;
		_totalFoV = totalFoV;
		_overlapPercentagePerPanoShot = overlapPercentagePerPanoShot;
		_currentFoV = currentFoV;
		_typeOfShot = ScreenshotType::HorizontalPanorama;
		_isTestRun = isTestRun;
		// panos are rotated from the far left to the far right of the total fov, where at the start, the center of the screen is rotated to the far left of the total fov, 
		// till the center of the screen hits the far right of the total fov. This is done because panorama stitching can often lead to corners not being used, so an overlap
		// on either side is preferable.

		// calculate the angle to step
		_anglePerStep = currentFoV * ((100.0f - overlapPercentagePerPanoShot) / 100.0f);
		// calculate the # of shots to take
		_amountOfShotsToTake = ((_totalFoV / _anglePerStep) + 1);

		// move to start
		moveCameraForPanorama(-1, true);

		// set convolution counter to its initial value
		_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
		_state = ScreenshotControllerState::Grabbing;
		// we'll wait now till all the shots are taken. 
		waitForShots();
		OverlayControl::addNotification("All Panorama shots have been taken. Writing shots to disk...");
		saveGrabbedShots();
		OverlayControl::addNotification("Panorama done.");
		// done

	}


	void ScreenshotController::startLightfieldShot(Camera camera, float distancePerStep, int amountOfShots, bool isTestRun)
	{
		OverlayConsole::instance().logDebug("startLightfield shot start. isTestRun: %d", isTestRun);
		reset();
		_isTestRun = isTestRun;
		_camera = camera;
		_distancePerStep = distancePerStep;
		_amountOfShotsToTake = amountOfShots;
		_typeOfShot = ScreenshotType::Lightfield;
		// move to start
		moveCameraForLightfield(-1, true);
		// set convolution counter to its initial value
		_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
		_state = ScreenshotControllerState::Grabbing;
		// we'll wait now till all the shots are taken. 
		waitForShots();
		OverlayControl::addNotification("All Lightfield have been shots taken. Writing shots to disk...");
		saveGrabbedShots();
		OverlayControl::addNotification("Lightfield done.");
		// done
	}

	void ScreenshotController::startBokehShot(bool lastShotTaken)
	{
		if (!lastShotTaken) {
			//OverlayConsole::instance().logDebug("startBokehShot start.");
			reset();
			_typeOfShot = ScreenshotType::BokehShot;
			_state = ScreenshotControllerState::Grabbing;
			// we'll wait now till all the shots are taken. 
			waitForShots();
		}
		else {
			OverlayControl::addNotification("Bokeh screenshot taken. Writing to disk...");
			saveBokehGrabbedShots();
			OverlayControl::addNotification("Bokeh screenshot done.");
		}

		// done
	}

	void ScreenshotController::storeBokehGrabbedShot(std::vector<uint8_t> grabbedShot)
	{
		if (grabbedShot.size() <= 0)
		{
			// failed
			return;
		}
		if (!Globals::instance().settings().onlyLastShot) {
			float blendingRate = 1.0f / (float)(Globals::instance().shotsToTake());
			if (_floatRangeFrame.size() <= 0) {
				_finalFrame = grabbedShot;
				size_t size = grabbedShot.size();
				for (int i = 0; i < size; i += 1) {
					_floatRangeFrame.push_back((float)(fastPow(grabbedShot.data()[i] / 255.f * blendingRate, (Globals::instance().settings().gammaCorrection)))); //first sample converted to 0 to 1 float with gamma correction
				}
			}
			else {
				size_t size = grabbedShot.size();

				// camera rotation method (close distance broken)
				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				for (int i = 0; i < size; i += 4) {
					_floatRangeFrame.data()[i] = ChannelBlend_Add(_floatRangeFrame.data()[i], ((float)(fastPow(grabbedShot.data()[i] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate)));
					_floatRangeFrame.data()[i + 1] = ChannelBlend_Add(_floatRangeFrame.data()[i + 1], ((float)(fastPow(grabbedShot.data()[i + 1] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate)));
					_floatRangeFrame.data()[i + 2] = ChannelBlend_Add(_floatRangeFrame.data()[i + 2], ((float)(fastPow(grabbedShot.data()[i + 2] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate)));
					_floatRangeFrame.data()[i + 3] = 1.f;
				}

				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

				//image offset method (close distance broken too)
				///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				
				/*int sy = 0;
				int sx = 0;
				for (sy = 0; sy < _framebufferHeight; ++sy) {
					if (sy + ((int)floor(Globals::instance().offsetValuesY())) < 0) { continue; }
					if (sy + ((int)floor(Globals::instance().offsetValuesY())) >= _framebufferHeight) { break; }
					for (sx = 0; sx < _framebufferWidth; ++sx) {
						if (sx + ((int)floor(Globals::instance().offsetValuesX())) < 0) { continue; }
						if (sx + ((int)floor(Globals::instance().offsetValuesX())) >= _framebufferWidth) { break; }
						int sourcePx = (sx + sy * _framebufferWidth) * 4;
						int dstPx = (sx + (int)floor(Globals::instance().offsetValuesX()) + (sy + (int)floor(Globals::instance().offsetValuesY())) * _framebufferWidth) * 4;
						float dstPxF = (sx + Globals::instance().offsetValuesX() + (sy + Globals::instance().offsetValuesY()) * _framebufferWidth) * 4;
						_floatRangeFrame.data()[sourcePx] = _floatRangeFrame.data()[sourcePx] + ((float)(fastPow(grabbedShot.data()[dstPx] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate));
						_floatRangeFrame.data()[sourcePx + 1] = _floatRangeFrame.data()[sourcePx + 1] + ((float)(fastPow(grabbedShot.data()[dstPx + 1] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate));
						_floatRangeFrame.data()[sourcePx + 2] = _floatRangeFrame.data()[sourcePx + 2] + ((float)(fastPow(grabbedShot.data()[dstPx + 2] / 255.f, (Globals::instance().settings().gammaCorrection)) * blendingRate));
						_floatRangeFrame.data()[sourcePx + 3] = 1;//_floatRangeFrame.data()[sourcePx + 3] + ((float)((grabbedShot.data()[dstPx + 3] / 255.f * blendingRate)));
					}
				}*/
			}
		}
		else { //Only Last Shot capture
			if (_floatRangeFrame.size() <= 0) {
				_finalFrame = grabbedShot;
				size_t size = grabbedShot.size();
				for (int i = 0; i < size; i += 1) {
					_floatRangeFrame.push_back((float)(grabbedShot.data()[i] / 255.f));
				}
			}
		}

		// we're done. Move to the next state, which is saving shots.
		unique_lock<mutex> lock(_waitCompletionMutex);
		_state = ScreenshotControllerState::SavingShots;
		// tell the waiting thread to wake up so the system can proceed as normal.
		_waitCompletionHandle.notify_all();

	}


	void ScreenshotController::storeGrabbedShot(std::vector<uint8_t> grabbedShot)
	{
		if (grabbedShot.size() <= 0)
		{
			// failed
			return;
		}
		_grabbedFrames.push_back(grabbedShot);
		_shotCounter++;
		if (_shotCounter > _amountOfShotsToTake)
		{
			// we're done. Move to the next state, which is saving shots. 
			unique_lock<mutex> lock(_waitCompletionMutex);
			_state = ScreenshotControllerState::SavingShots;
			// tell the waiting thread to wake up so the system can proceed as normal.
			_waitCompletionHandle.notify_all();
		}
		else
		{
			modifyCamera();
			_convolutionFrameCounter = _numberOfFramesToWaitBetweenSteps;
		}
	}


	void ScreenshotController::saveBokehGrabbedShots()
	{
		/*if (_grabbedFrames.size() <= 0)
		{
			return;
		}*/
		if (!_isTestRun)
		{
			if (Globals::instance().settings().onlyLastShot) {
				for (int i = 0; i < _floatRangeFrame.size(); i += 4) {
					_finalFrame.data()[i] = (unsigned char/*uint8_t*/)BYTE_BOUND(_floatRangeFrame.data()[i] * 255.f);
					_finalFrame.data()[i + 1] = (unsigned char/*uint8_t*/)BYTE_BOUND(_floatRangeFrame.data()[i + 1] * 255.f);
					_finalFrame.data()[i + 2] = (unsigned char/*uint8_t*/)BYTE_BOUND(_floatRangeFrame.data()[i + 2] * 255.f);
					_finalFrame.data()[i + 3] = (unsigned char/*uint8_t*/)BYTE_BOUND(255.f);
				}
			} else {
			float gamma = 1.f / Globals::instance().settings().gammaCorrection;
			for (int i = 0; i < _floatRangeFrame.size(); i += 4) {
				_finalFrame.data()[i] = (unsigned char/*uint8_t*/)BYTE_BOUND(fastPow(_floatRangeFrame.data()[i], (gamma)) * 255.f);
				_finalFrame.data()[i + 1] = (unsigned char/*uint8_t*/)BYTE_BOUND(fastPow(_floatRangeFrame.data()[i + 1], (gamma)) * 255.f);
				_finalFrame.data()[i + 2] = (unsigned char/*uint8_t*/)BYTE_BOUND(fastPow(_floatRangeFrame.data()[i + 2], (gamma)) * 255.f);
				_finalFrame.data()[i + 3] = (unsigned char/*uint8_t*/)BYTE_BOUND(255.f);
			}
			}
			string destinationFolder = createScreenshotFolder();
			int frameNumber = 0;
			saveShotToFile(destinationFolder, _finalFrame, frameNumber);
		}
		// done
		_floatRangeFrame.clear();
		_finalFrame.clear();
		_grabbedFrames.clear();
		_state = ScreenshotControllerState::Off;
	}


	void ScreenshotController::saveGrabbedShots()
	{
		if (_grabbedFrames.size() <= 0)
		{
			return;
		}
		if (!_isTestRun)
		{
			string destinationFolder = createScreenshotFolder();
			int frameNumber = 0;
			for (std::vector<uint8_t> frame : _grabbedFrames)
			{
				saveShotToFile(destinationFolder, frame, frameNumber);
				frameNumber++;
			}
		}
		// done
		_grabbedFrames.clear();
		_state = ScreenshotControllerState::Off;
	}


	void ScreenshotController::saveShotToFile(std::string destinationFolder, std::vector<uint8_t> data, int frameNumber)
	{
		bool saveSuccessful = false;
		string filename = "";
		switch (Globals::instance().settings().fileType) //_filetype (choose between filetypes)
		{
			/*case ScreenshotFiletype::Bmp:
				filename = Utils::formatString("%s\\%d.bmp", destinationFolder.c_str(), frameNumber);
				saveSuccessful = stbi_write_bmp(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data()) != 0;
				break;
			case ScreenshotFiletype::Jpeg:
				filename = Utils::formatString("%s\\%d.jpg", destinationFolder.c_str(), frameNumber);
				saveSuccessful = stbi_write_jpg(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data(), IGCS_JPG_SCREENSHOT_QUALITY) != 0;
				break;
			case ScreenshotFiletype::Png:
				filename = Utils::formatString("%s\\%d.png", destinationFolder.c_str(), frameNumber);
				saveSuccessful = stbi_write_png(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data(), 4 * _framebufferWidth) != 0;
				break;*/

		case (int)ScreenshotFiletype::Bmp:
			filename = Utils::formatString("%s\\%d.bmp", destinationFolder.c_str(), frameNumber);
			saveSuccessful = stbi_write_bmp(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data()) != 0;
			break;
		case (int)ScreenshotFiletype::Jpeg:
			filename = Utils::formatString("%s\\%d.jpg", destinationFolder.c_str(), frameNumber);
			saveSuccessful = stbi_write_jpg(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data(), IGCS_JPG_SCREENSHOT_QUALITY) != 0;
			break;
		case (int)ScreenshotFiletype::Png:
			filename = Utils::formatString("%s\\%d.png", destinationFolder.c_str(), frameNumber);
			saveSuccessful = stbi_write_png(filename.c_str(), _framebufferWidth, _framebufferHeight, 4, data.data(), 4 * _framebufferWidth) != 0;
			break;
		}
		if (saveSuccessful)
		{
			OverlayConsole::instance().logDebug("Successfully wrote screenshot of dimensions %dx%d to... %s", _framebufferWidth, _framebufferHeight, filename.c_str());
		}
		else
		{
			OverlayConsole::instance().logDebug("Failed to write screenshot of dimensions %dx%d to... %s", _framebufferWidth, _framebufferHeight, filename.c_str());
		}
	}


	string ScreenshotController::createScreenshotFolder()
	{
		time_t t = time(nullptr);
		tm tm;
		localtime_s(&tm, &t);
		string optionalBackslash = (_rootFolder.ends_with('\\')) ? "" : "\\";
		string folderName = Utils::formatString("%s%s%.4d-%.2d-%.2d-%.2d-%.2d-%.2d", _rootFolder.c_str(), optionalBackslash.c_str(), (tm.tm_year + 1900), (tm.tm_mon + 1), tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
		mkdir(folderName.c_str());
		return folderName;
	}


	void ScreenshotController::waitForShots()
	{
		unique_lock<mutex> lock(_waitCompletionMutex);
		while (_state != ScreenshotControllerState::SavingShots)
		{
			_waitCompletionHandle.wait(lock);
		}
		// state has changed, we're notified so we're all goed to save the shots. 
	}


	void ScreenshotController::modifyCamera()
	{
		// based on the type of the shot, we'll either rotate or move.
		switch (_typeOfShot)
		{
		case ScreenshotType::HorizontalPanorama:
			moveCameraForPanorama(1, false);
			break;
		case ScreenshotType::Lightfield:
			moveCameraForLightfield(1, false);
			break;
		case ScreenshotType::SingleShot:
			// nothing
			break;
		case ScreenshotType::BokehShot:
			// nothing
			break;
		}
	}


	void ScreenshotController::moveCameraForLightfield(int direction, bool end)
	{
		_camera.resetMovement();
		float dist = direction * _distancePerStep;
		if (end)
		{
			dist *= 0.5f * _amountOfShotsToTake;
		}
		float stepSize = dist / _movementSpeed; // scale to be independent of camera movement speed
		_camera.moveRight(stepSize);
		GameSpecific::CameraManipulator::updateCameraDataInGameData(_camera);
	}


	void ScreenshotController::moveCameraForPanorama(int direction, bool end)
	{
		_camera.resetMovement();
		float dist = direction * _anglePerStep;
		if (end)
		{
			dist *= 0.5f * _amountOfShotsToTake;
		}
		float stepSize = dist / _rotationSpeed; // scale to be independent of camera rotation speed
		_camera.yaw(stepSize);
		GameSpecific::CameraManipulator::updateCameraDataInGameData(_camera);
	}


	void ScreenshotController::reset()
	{
		// don't reset framebuffer width/height, numberOfFramesToWaitBetweenSteps, movementSpeed, 
		// rotationSpeed, rootFolder as those are set through configure!
		_typeOfShot = ScreenshotType::Lightfield;
		_state = ScreenshotControllerState::Off;
		_totalFoV = 0.0f;
		_currentFoV = 0.0f;
		_distancePerStep = 0.0f;
		_anglePerStep = 0.0f;
		_amountOfShotsToTake = 0;
		_amountOfColumns = 0;
		_amountOfRows = 0;
		_convolutionFrameCounter = 0;
		_shotCounter = 0;
		_overlapPercentagePerPanoShot = 30.0f;
		_isTestRun = false;

		_grabbedFrames.clear();
		_floatRangeFrame.clear();
	}
}
