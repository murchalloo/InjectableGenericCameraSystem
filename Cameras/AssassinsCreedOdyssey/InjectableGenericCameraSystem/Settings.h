#pragma once
#include "stdafx.h"
#include "Gamepad.h"
#include "Utils.h"
#include "GameConstants.h"
#include "CDataFile.h"
#include "Defaults.h"
#include <map>
#include "ActionData.h"
#include <string>

namespace IGCS
{
	struct Settings
	{
		// settings written to config file
		bool invertY;
		float fastMovementMultiplier;
		float slowMovementMultiplier;
		float movementUpMultiplier;
		float movementSpeed;
		float rotationSpeed;
		float fovChangeSpeed;
		int cameraControlDevice;		// 0==keyboard/mouse, 1 == gamepad, 2 == both, see Defaults.h
		bool allowCameraMovementWhenMenuIsUp;
		bool disableInGameDofWhenCameraIsEnabled;
		// screenshot settings
		int numberOfFramesToWaitBetweenSteps;
		float distanceBetweenLightfieldShots;
		int numberOfShotsToTake;
		int typeOfScreenshot;
		int fileType;
		float totalPanoAngleDegrees;
		float overlapPercentagePerPanoShot;
		char screenshotFolder[_MAX_PATH+1] = { 0 };
		// bokeh settings
		float focusingCorrection;
		float focusingDistance;
		int shapeSize;
		float gammaCorrection;
		float zPosCorrection;
		float previewSize;
		int numberOfRings;
		float anamorphism;
		bool offSetType;
		float offSet;
		bool onlyLastShot;
		bool testMove;
		bool upRight;

		// settings not persisted to config file.
		// add settings to edit here.
		float resolutionScale;			// 0.5-4.0
		int todHour;					// 0-23
		int todMinute;					// 0-59
		float fogStrength;				// 0-200. 0 is no fog (ugly), 200 is thick fog all around you. Can go higher if one wants.
		float fogStartCurve;			// 0-1. 1 is default. 

		void loadFromFile(map<ActionType, ActionData*> keyBindingPerActionType)
		{
			CDataFile iniFile;
			if (!iniFile.Load(IGCS_SETTINGS_INI_FILENAME))
			{
				// doesn't exist
				return;
			}
			invertY = iniFile.GetBool("invertY", "CameraSettings");
			allowCameraMovementWhenMenuIsUp = iniFile.GetBool("allowCameraMovementWhenMenuIsUp", "CameraSettings");
			disableInGameDofWhenCameraIsEnabled = iniFile.GetBool("disableInGameDofWhenCameraIsEnabled", "CameraSettings");
			fastMovementMultiplier = Utils::clamp(iniFile.GetFloat("fastMovementMultiplier", "CameraSettings"), 0.0f, FASTER_MULTIPLIER);
			slowMovementMultiplier = Utils::clamp(iniFile.GetFloat("slowMovementMultiplier", "CameraSettings"), 0.0f, SLOWER_MULTIPLIER);
			movementUpMultiplier = Utils::clamp(iniFile.GetFloat("movementUpMultiplier", "CameraSettings"), 0.0f, DEFAULT_UP_MOVEMENT_MULTIPLIER);
			movementSpeed = Utils::clamp(iniFile.GetFloat("movementSpeed", "CameraSettings"), 0.0f, DEFAULT_MOVEMENT_SPEED);
			rotationSpeed = Utils::clamp(iniFile.GetFloat("rotationSpeed", "CameraSettings"), 0.0f, DEFAULT_ROTATION_SPEED);
			fovChangeSpeed = Utils::clamp(iniFile.GetFloat("fovChangeSpeed", "CameraSettings"), 0.0f, DEFAULT_FOV_SPEED);
			cameraControlDevice = Utils::clamp(iniFile.GetInt("cameraControlDevice", "CameraSettings"), 0, DEVICE_ID_ALL, DEVICE_ID_ALL);
			// screenshot settings
			numberOfFramesToWaitBetweenSteps = Utils::clamp(iniFile.GetInt("numberOfFramesToWaitBetweenSteps", "ScreenshotSettings"), 1, 100);
			distanceBetweenLightfieldShots = Utils::clamp(iniFile.GetFloat("distanceBetweenLightfieldShots", "ScreenshotSettings"), 0.0f, 100.0f);
			numberOfShotsToTake = Utils::clamp(iniFile.GetInt("numberOfShotsToTake", "ScreenshotSettings"), 0, 45);
			typeOfScreenshot = Utils::clamp(iniFile.GetInt("typeOfScreenshot", "ScreenshotSettings"), 0, ((int)ScreenshotType::Amount)-1);
			totalPanoAngleDegrees = Utils::clamp(iniFile.GetFloat("totalPanoAngleDegrees", "ScreenshotSettings"), 30.0f, 360.0f, 110.0f);
			overlapPercentagePerPanoShot = Utils::clamp(iniFile.GetFloat("overlapPercentagePerPanoShot", "ScreenshotSettings"), 0.1f, 99.0f, 80.0f);
			std::string folder = iniFile.GetValue("screenshotFolder", "ScreenshotSettings");
			folder.copy(screenshotFolder, folder.length());
			screenshotFolder[folder.length()] = '\0';
			// bokeh settings
			focusingCorrection = Utils::clamp(iniFile.GetFloat("focusingCorrection", "BokehSettings"), 1.0f, 10.0f);
			focusingDistance = Utils::clamp(iniFile.GetFloat("focusingDistance", "BokehSettings"), 0.001f, 1.0f);
			shapeSize = Utils::clamp(iniFile.GetInt("shapeSize", "BokehSettings"), 1, 100);
			gammaCorrection = Utils::clamp(iniFile.GetFloat("gammaCorrection", "BokehSettings"), 1.0f, 3.0f);
			zPosCorrection = Utils::clamp(iniFile.GetFloat("zPosCorrection", "BokehSettings"), 0.0f, 1.0f);
			previewSize = Utils::clamp(iniFile.GetFloat("previewSize", "BokehSettings"), 1.0f, 3.0f);
			numberOfRings = Utils::clamp(iniFile.GetInt("numberOfRings", "BokehSettings"), 1, 100);
			anamorphism = Utils::clamp(iniFile.GetFloat("anamorphismSize", "BokehSettings"), -1.0f, 1.0f);
			offSetType = iniFile.GetBool("offSetType", "BokehSettings");
			offSet = Utils::clamp(iniFile.GetFloat("offSet", "BokehSettings"), 0.0f, 10.0f);
			onlyLastShot = iniFile.GetBool("offSetType", "BokehSettings");

			// load keybindings. They might not be there, or incomplete. 
			for (std::pair<ActionType, ActionData*> kvp : keyBindingPerActionType)
			{
				int valueFromIniFile = iniFile.GetInt(kvp.second->getName(), "KeyBindings");
				if (valueFromIniFile == INT_MIN)
				{
					// not found
					continue;
				}
				kvp.second->setValuesFromIniFileValue(valueFromIniFile);
			}
		}


		void saveToFile(map<ActionType, ActionData*> keyBindingPerActionType)
		{
			CDataFile iniFile;
			iniFile.SetBool("invertY", invertY, "", "CameraSettings");
			iniFile.SetBool("allowCameraMovementWhenMenuIsUp", allowCameraMovementWhenMenuIsUp, "", "CameraSettings");
			iniFile.SetBool("disableInGameDofWhenCameraIsEnabled", disableInGameDofWhenCameraIsEnabled, "", "CameraSettings");
			iniFile.SetFloat("fastMovementMultiplier", fastMovementMultiplier, "", "CameraSettings");
			iniFile.SetFloat("slowMovementMultiplier", slowMovementMultiplier, "", "CameraSettings");
			iniFile.SetFloat("movementUpMultiplier", movementUpMultiplier, "", "CameraSettings");
			iniFile.SetFloat("movementSpeed", movementSpeed, "", "CameraSettings");
			iniFile.SetFloat("rotationSpeed", rotationSpeed, "", "CameraSettings");
			iniFile.SetFloat("fovChangeSpeed", fovChangeSpeed, "", "CameraSettings");
			iniFile.SetInt("cameraControlDevice", cameraControlDevice, "", "CameraSettings");
			// screenshot settings
			iniFile.SetInt("numberOfFramesToWaitBetweenSteps", numberOfFramesToWaitBetweenSteps, "", "ScreenshotSettings");
			iniFile.SetFloat("distanceBetweenLightfieldShots", distanceBetweenLightfieldShots, "", "ScreenshotSettings");
			iniFile.SetInt("numberOfShotsToTake", numberOfShotsToTake, "", "ScreenshotSettings");
			iniFile.SetInt("typeOfScreenshot", typeOfScreenshot, "", "ScreenshotSettings");
			iniFile.SetFloat("totalPanoAngleDegrees", totalPanoAngleDegrees, "", "ScreenshotSettings");
			iniFile.SetFloat("overlapPercentagePerPanoShot", overlapPercentagePerPanoShot, "", "ScreenshotSettings");
			iniFile.SetValue("screenshotFolder", screenshotFolder, "", "ScreenshotSettings");
			// bokeh settings
			iniFile.SetFloat("focusingCorrection", focusingCorrection, "", "BokehSettings");
			iniFile.SetFloat("focusingDistance", focusingDistance, "", "BokehSettings");
			iniFile.SetFloat("shapeSize", shapeSize, "", "BokehSettings");
			iniFile.SetFloat("gammaCorrection", gammaCorrection, "", "BokehSettings");
			iniFile.SetFloat("zPosCorrection", zPosCorrection, "", "BokehSettings");
			iniFile.SetFloat("previewSize", previewSize, "", "BokehSettings");
			iniFile.SetInt("numberOfRings", numberOfRings, "", "BokehSettings");
			iniFile.SetFloat("anamorphism", anamorphism, "", "BokehSettings");
			iniFile.SetBool("offSetType", offSetType, "", "BokehSettings");
			iniFile.SetFloat("offSet", offSet, "", "BokehSettings");
			iniFile.SetBool("onlyLastShot", onlyLastShot, "", "BokehSettings");

			// save keybindings
			if (!keyBindingPerActionType.empty())
			{
				for (std::pair<ActionType, ActionData*> kvp : keyBindingPerActionType)
				{
					// we're going to write 4 bytes, the keycode is 1 byte, and for each bool we'll shift in a 1 or 0, depending on whether it's true (1) or false (0).
					int value = kvp.second->getIniFileValue();
					iniFile.SetInt(kvp.second->getName(), value, "", "KeyBindings");
				}
				iniFile.SetSectionComment("KeyBindings", "Values are 4-byte ints: A|B|C|D, where A is byte for VT keycode, B, C and D are bools if resp. Alt, Ctrl or Shift is required");
			}

			iniFile.SetFileName(IGCS_SETTINGS_INI_FILENAME);
			iniFile.Save();
		}


		void init(bool persistedOnly)
		{
			invertY = CONTROLLER_Y_INVERT;
			fastMovementMultiplier = FASTER_MULTIPLIER;
			slowMovementMultiplier = SLOWER_MULTIPLIER;
			movementUpMultiplier = DEFAULT_UP_MOVEMENT_MULTIPLIER;
			movementSpeed = DEFAULT_MOVEMENT_SPEED;
			rotationSpeed = DEFAULT_ROTATION_SPEED;
			fovChangeSpeed = DEFAULT_FOV_SPEED;
			cameraControlDevice = DEVICE_ID_ALL;
			allowCameraMovementWhenMenuIsUp = false;
			disableInGameDofWhenCameraIsEnabled = false;
			numberOfFramesToWaitBetweenSteps = 1;
			// Screenshot settings
			distanceBetweenLightfieldShots = 1.0f;
			numberOfShotsToTake= 45;
			typeOfScreenshot = (int)ScreenshotType::Lightfield;
			fileType = (int)ScreenshotFiletype::Bmp;
			totalPanoAngleDegrees = 110.0f;
			overlapPercentagePerPanoShot = 80.0f;
			strcpy(screenshotFolder, "c:\\");
			// bokeh settings
			focusingCorrection = 1.0f;
			focusingDistance = 1.0f;
			shapeSize = 1;
			gammaCorrection = 2.2f;
			zPosCorrection = 0.0f;
			previewSize = 1.5f;
			numberOfRings = 12;
			anamorphism = 0.0f;
			offSetType = false;
			offSet = 0.0;
			onlyLastShot = true;
			testMove = false;
			upRight = false;

			if (!persistedOnly)
			{
				resolutionScale = 1.0f;
				todHour = 12;
				todMinute = 0;
				fogStrength = 1.0f;
				fogStartCurve = 1.0f;
			}
		}
	};
}