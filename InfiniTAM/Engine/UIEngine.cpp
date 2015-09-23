// Copyright 2014-2015 Isis Innovation Limited and the authors of InfiniTAM

#include "UIEngine.h"
#include <iostream>
#include <strstream>

#include <string.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#ifdef FREEGLUT
#include <GL/freeglut.h>
#else
#if (!defined USING_CMAKE) && (defined _MSC_VER)
#pragma comment(lib, "glut64")
#endif
#endif

#include "../Utils/FileUtils.h"

using namespace InfiniTAM::Engine;
UIEngine* UIEngine::instance;

static void safe_glutBitmapString(void *font, const char *str)
{
	size_t len = strlen(str);
	for (size_t x = 0; x < len; ++x) {
		glutBitmapCharacter(font, str[x]);
	}
}

void UIEngine::glutDisplayFunction()
{
	UIEngine *uiEngine = UIEngine::Instance();

	// get updated images from processing thread
	uiEngine->mainEngine->GetImage(uiEngine->outImage[0], uiEngine->outImageType[0], &uiEngine->freeviewPose, &uiEngine->freeviewIntrinsics);

	for (int w = 1; w < NUM_WIN; w++) uiEngine->mainEngine->GetImage(uiEngine->outImage[w], uiEngine->outImageType[w]);

	// do the actual drawing
	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1.0f, 1.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);

	ITMUChar4Image** showImgs = uiEngine->outImage;
	Vector4f *winReg = uiEngine->winReg;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	{
		glLoadIdentity();
		glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		{
			glEnable(GL_TEXTURE_2D);
			for (int w = 0; w < NUM_WIN; w++)	{// Draw each sub window
				if (uiEngine->outImageType[w] == ITMMainEngine::InfiniTAM_IMAGE_UNKNOWN) continue;
				glBindTexture(GL_TEXTURE_2D, uiEngine->textureId[w]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, showImgs[w]->noDims.x, showImgs[w]->noDims.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, showImgs[w]->GetData(MEMORYDEVICE_CPU));
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glBegin(GL_QUADS); {
					glTexCoord2f(0, 1); glVertex2f(winReg[w][0], winReg[w][1]); // glVertex2f(0, 0);
					glTexCoord2f(1, 1); glVertex2f(winReg[w][2], winReg[w][1]); // glVertex2f(1, 0);
					glTexCoord2f(1, 0); glVertex2f(winReg[w][2], winReg[w][3]); // glVertex2f(1, 1);
					glTexCoord2f(0, 0); glVertex2f(winReg[w][0], winReg[w][3]); // glVertex2f(0, 1);
				}
				glEnd();
			}
			glDisable(GL_TEXTURE_2D);
		}
		glPopMatrix();
	}
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glColor3f(1.0f, 0.0f, 0.0f); glRasterPos2f(0.85f, -0.962f);

	char str[200]; sprintf(str, "%04.2lf", uiEngine->processedTime);
	safe_glutBitmapString(GLUT_BITMAP_HELVETICA_18, (const char*)str);

	glRasterPos2f(-0.95f, -0.95f);
	if (uiEngine->freeviewActive)
	{
		sprintf(str, "n - next frame \t b - all frames \t e/esc - exit \t f - follow camera \t c - colours (currently %s) \t t - turn fusion %s", uiEngine->colourModes[uiEngine->currentColourMode].name, uiEngine->intergrationActive ? "off" : "on");
	}
	else
	{
		sprintf(str, "n - next frame \t b - all frames \t e/esc - exit \t f - free viewpoint \t t - turn fusion %s", uiEngine->intergrationActive ? "off" : "on");
	}
	safe_glutBitmapString(GLUT_BITMAP_HELVETICA_12, (const char*)str);

	glutSwapBuffers();
	uiEngine->needsRefresh = false;
}

void UIEngine::glutIdleFunction()
{
	UIEngine *uiEngine = UIEngine::Instance();

	switch (uiEngine->mainLoopAction)
	{
	case PROCESS_FRAME:
		uiEngine->ProcessFrame(); uiEngine->processedFrameNo++;
		uiEngine->mainLoopAction = PROCESS_PAUSED;
		uiEngine->needsRefresh = true;
		break;
	case PROCESS_VIDEO:
		uiEngine->ProcessFrame(); uiEngine->processedFrameNo++;
		uiEngine->needsRefresh = true;
		break;
		//case SAVE_TO_DISK:
		//	if (!uiEngine->actionDone)
		//	{
		//		char outFile[255];

		//		ITMUChar4Image *saveImage = uiEngine->saveImage;

		//		glReadBuffer(GL_BACK);
		//		glReadPixels(0, 0, saveImage->noDims.x, saveImage->noDims.x, GL_RGBA, GL_UNSIGNED_BYTE, (unsigned char*)saveImage->GetData(false));
		//		sprintf(outFile, "%s/out_%05d.ppm", uiEngine->outFolder, uiEngine->processedFrameNo);

		//		SaveImageToFile(saveImage, outFile, true);

		//		uiEngine->actionDone = true;
		//	}
		//	break;
	case EXIT:
#ifdef FREEGLUT
		glutLeaveMainLoop();
#else
		exit(0);
#endif
		break;
	case PROCESS_PAUSED:
	default:
		break;
	}

	if (uiEngine->needsRefresh) {
		glutPostRedisplay();
	}
}

void UIEngine::glutKeyUpFunction(unsigned char key, int x, int y)
{
	UIEngine *uiEngine = UIEngine::Instance();

	switch (key)
	{
	case 'n':
		printf("processing one frame ...\n");
		uiEngine->mainLoopAction = UIEngine::PROCESS_FRAME;
		break;
	case 'b':
		printf("processing input source ...\n");
		uiEngine->mainLoopAction = UIEngine::PROCESS_VIDEO;
		break;
	case 's':
		printf("saving surface points to disk ...");
		uiEngine->mainLoopAction = UIEngine::PROCESS_PAUSED;
		uiEngine->mainEngine->saveSurfacePoints("surfacePoints.ply");
		printf(" done\n");
		break;
	case 'e':
	case 27: // esc key
		printf("exiting ...\n");
		uiEngine->mainLoopAction = UIEngine::EXIT;
		break;
	case 'f':
		if (uiEngine->freeviewActive)
		{
			uiEngine->outImageType[0] = ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST;
			uiEngine->outImageType[1] = ITMMainEngine::InfiniTAM_IMAGE_ORIGINAL_DEPTH;

			uiEngine->freeviewActive = false;
		}
		else
		{
			uiEngine->outImageType[0] = ITMMainEngine::InfiniTAM_IMAGE_FREECAMERA_SHADED;
			uiEngine->outImageType[1] = ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST;

			uiEngine->freeviewPose.SetFrom(uiEngine->mainEngine->GetTrackingState()->pose_d);
			if (uiEngine->mainEngine->GetView() != NULL) {
				uiEngine->freeviewIntrinsics = uiEngine->mainEngine->GetView()->calib->intrinsics_d;
				uiEngine->outImage[0]->ChangeDims(uiEngine->mainEngine->GetView()->depth->noDims);
			}
			uiEngine->freeviewActive = true;
		}
		uiEngine->needsRefresh = true;
		break;
	case 'c':
		uiEngine->currentColourMode++; if ((unsigned)uiEngine->currentColourMode >= uiEngine->colourModes.size()) uiEngine->currentColourMode = 0;
		uiEngine->needsRefresh = true;
		break;
	case 't':
		uiEngine->intergrationActive = !uiEngine->intergrationActive;
		if (uiEngine->intergrationActive) uiEngine->mainEngine->turnOnIntegration();
		else uiEngine->mainEngine->turnOffIntegration();
		break;
	case 'w':
		printf("saving mesh to disk ...");
		uiEngine->SaveSceneToMesh("mesh.stl");
		printf(" done\n");
		break;
	default:
		break;
	}

	if (uiEngine->freeviewActive) {
		uiEngine->outImageType[0] = uiEngine->colourModes[uiEngine->currentColourMode].type;
	}
}

void UIEngine::glutMouseButtonFunction(int button, int state, int x, int y)
{
	UIEngine *uiEngine = UIEngine::Instance();

	if (state == GLUT_DOWN)
	{
		switch (button)
		{
		case GLUT_LEFT_BUTTON: uiEngine->mouseState = 1; break;
		case GLUT_MIDDLE_BUTTON: uiEngine->mouseState = 3; break;
		case GLUT_RIGHT_BUTTON: uiEngine->mouseState = 2; break;
		default: break;
		}
		uiEngine->mouseLastClick.x = x;
		uiEngine->mouseLastClick.y = y;
	}
	else if (state == GLUT_UP) uiEngine->mouseState = 0;
}

static inline Matrix3f createRotation(const Vector3f & _axis, float angle)
{
	Vector3f axis = normalize(_axis);
	float si = sinf(angle);
	float co = cosf(angle);

	Matrix3f ret;
	ret.setIdentity();

	ret *= co;
	for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) ret.at(c, r) += (1.0f - co) * axis[c] * axis[r];

	Matrix3f skewmat;
	skewmat.setZeros();
	skewmat.at(1, 0) = -axis.z;
	skewmat.at(0, 1) = axis.z;
	skewmat.at(2, 0) = axis.y;
	skewmat.at(0, 2) = -axis.y;
	skewmat.at(2, 1) = axis.x;
	skewmat.at(1, 2) = -axis.x;
	skewmat *= si;
	ret += skewmat;

	return ret;
}

void UIEngine::glutMouseMoveFunction(int x, int y)
{
	UIEngine *uiEngine = UIEngine::Instance();

	if (!uiEngine->freeviewActive) return;

	Vector2i movement;
	movement.x = x - uiEngine->mouseLastClick.x;
	movement.y = y - uiEngine->mouseLastClick.y;
	uiEngine->mouseLastClick.x = x;
	uiEngine->mouseLastClick.y = y;

	if ((movement.x == 0) && (movement.y == 0)) return;

	static const float scale_rotation = 0.005f;
	static const float scale_translation = 0.0025f;

	switch (uiEngine->mouseState)
	{
	case 1:
	{
		// left button: rotation
		Vector3f axis((float)-movement.y, (float)-movement.x, 0.0f);
		float angle = scale_rotation * sqrt((float)(movement.x * movement.x + movement.y*movement.y));
		Matrix3f rot = createRotation(axis, angle);
		uiEngine->freeviewPose.SetRT(rot * uiEngine->freeviewPose.GetR(), rot * uiEngine->freeviewPose.GetT());
		uiEngine->freeviewPose.Coerce();
		uiEngine->needsRefresh = true;
		break;
	}
	case 2:
	{
		// right button: translation in x and y direction
		uiEngine->freeviewPose.SetT(uiEngine->freeviewPose.GetT() + scale_translation * Vector3f((float)movement.x, (float)movement.y, 0.0f));
		uiEngine->needsRefresh = true;
		break;
	}
	case 3:
	{
		// middle button: translation along z axis
		uiEngine->freeviewPose.SetT(uiEngine->freeviewPose.GetT() + scale_translation * Vector3f(0.0f, 0.0f, (float)movement.y));
		uiEngine->needsRefresh = true;
		break;
	}
	default: break;
	}
}

void UIEngine::glutMouseWheelFunction(int button, int dir, int x, int y)
{
	UIEngine *uiEngine = UIEngine::Instance();

	static const float scale_translation = 0.05f;

	uiEngine->freeviewPose.SetT(uiEngine->freeviewPose.GetT() + scale_translation * Vector3f(0.0f, 0.0f, (dir > 0) ? -1.0f : 1.0f));
	uiEngine->needsRefresh = true;
}

void UIEngine::Initialise(int & argc, char** argv, ImageSourceEngine *imageSource, IMUSourceEngine *imuSource, ITMMainEngine *mainEngine,
	const char *outFolder, ITMLibSettings::DeviceType deviceType)
{
	this->freeviewActive = false;
	this->intergrationActive = true;
	this->currentColourMode = 0;
	this->colourModes.push_back(UIColourMode("shaded greyscale", ITMMainEngine::InfiniTAM_IMAGE_FREECAMERA_SHADED));
	if (ITMVoxel::hasColorInformation) this->colourModes.push_back(UIColourMode("integrated colours", ITMMainEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_VOLUME));
	this->colourModes.push_back(UIColourMode("surface normals", ITMMainEngine::InfiniTAM_IMAGE_FREECAMERA_COLOUR_FROM_NORMAL));

	this->imageSource = imageSource;
	this->imuSource = imuSource;
	this->mainEngine = mainEngine;
	{
		size_t len = strlen(outFolder);
		this->outFolder = new char[len + 1];
		strcpy(this->outFolder, outFolder);
	}

	//Vector2i winSize;
	//int textHeight = 30; // Height of text area
	//winSize.x = 2 * MAX(imageSource->getRGBImageSize().x, imageSource->getDepthImageSize().x);
	//winSize.y = MAX(imageSource->getRGBImageSize().y, imageSource->getDepthImageSize().y) + textHeight;
	//float h1 = textHeight / (float)winSize.y, h2 = (1.f + h1) / 2;
	//winReg[0] = Vector4f(0, h1, 0.5, 1); // Main render
	//winReg[1] = Vector4f(0.5, h2, 0.75, 1); // Side sub window 0
	//winReg[2] = Vector4f(0.75, h2, 1, 1); // Side sub window 1
	//winReg[3] = Vector4f(0.5, h1, 0.75, h2); // Side sub window 2
	//winReg[4] = Vector4f(0.75, h1, 1, h2); // Side sub window 3

	int textHeight = 30; // Height of text area
	//winSize.x = (int)(1.5f * (float)MAX(imageSource->getImageSize().x, imageSource->getDepthImageSize().x));
	//winSize.y = MAX(imageSource->getRGBImageSize().y, imageSource->getDepthImageSize().y) + textHeight;
	winSize.x = (int)(1.5f * (float)(imageSource->getDepthImageSize().x));
	winSize.y = imageSource->getDepthImageSize().y + textHeight;
	float h1 = textHeight / (float)winSize.y, h2 = (1.f + h1) / 2;
	winReg[0] = Vector4f(0.0f, h1, 0.665f, 1.0f);   // Main render
	winReg[1] = Vector4f(0.665f, h2, 1.0f, 1.0f);   // Side sub window 0
	winReg[2] = Vector4f(0.665f, h1, 1.0f, h2);     // Side sub window 2

	this->isRecording = true;
	this->currentFrameNo = 0;

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(winSize.x, winSize.y);
	glutCreateWindow("InfiniTAM");
	glGenTextures(NUM_WIN, textureId);

	glutDisplayFunc(UIEngine::glutDisplayFunction);
	glutKeyboardUpFunc(UIEngine::glutKeyUpFunction);
	glutMouseFunc(UIEngine::glutMouseButtonFunction);
	glutMotionFunc(UIEngine::glutMouseMoveFunction);
	glutIdleFunc(UIEngine::glutIdleFunction);

#ifdef FREEGLUT
	glutMouseWheelFunc(UIEngine::glutMouseWheelFunction);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, 1);
#endif

	bool allocateGPU = false;
	if (deviceType == ITMLibSettings::DEVICE_CUDA) allocateGPU = true;

	for (int w = 0; w < NUM_WIN; w++)
		outImage[w] = new ITMUChar4Image(imageSource->getDepthImageSize(), true, allocateGPU);

	inputRGBImage = new ITMUChar4Image(imageSource->getRGBImageSize(), true, allocateGPU);
	inputRawDepthImage = new ITMShortImage(imageSource->getDepthImageSize(), true, allocateGPU);
	inputIMUMeasurement = new ITMIMUMeasurement();

	saveImage = new ITMUChar4Image(imageSource->getDepthImageSize(), true, false);

	outImageType[0] = ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST;
	outImageType[1] = ITMMainEngine::InfiniTAM_IMAGE_ORIGINAL_DEPTH;
	outImageType[2] = ITMMainEngine::InfiniTAM_IMAGE_ORIGINAL_RGB;
	if (inputRGBImage->noDims == Vector2i(0,0)) outImageType[2] = ITMMainEngine::InfiniTAM_IMAGE_UNKNOWN;
	//outImageType[3] = ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST;
	//outImageType[4] = ITMMainEngine::InfiniTAM_IMAGE_SCENERAYCAST;

	mainLoopAction = PROCESS_PAUSED;
	mouseState = 0;
	needsRefresh = false;
	processedFrameNo = 0;
	processedTime = 0.0f;

#ifndef COMPILE_WITHOUT_CUDA
	ITMSafeCall(cudaThreadSynchronize());
#endif

	sdkCreateTimer(&timer_instant);
	sdkCreateTimer(&timer_average);

	sdkResetTimer(&timer_average);

	printf("initialised.\n");
}

void UIEngine::SaveScreenshot(const char *filename) const
{
	ITMUChar4Image screenshot(getWindowSize(), true, false);
	GetScreenshot(&screenshot);
	SaveImageToFile(&screenshot, filename, true);
}

void UIEngine::SaveFusionScreenshot(const char *filename) const
{
	UIEngine *uiEngine = UIEngine::Instance();
	Vector4f *winReg = uiEngine->winReg;
	Vector2i window;
	window.x = (int)((winReg[0][2] - winReg[0][0]) * winSize.x);
	window.y = (int)((winReg[0][3] - winReg[0][1]) * winSize.y);

	ITMUChar4Image screenshot(window, true, false);
	GetFusionScreenshot(&screenshot);
	SaveImageToFile(&screenshot, filename, true);
}

void UIEngine::SaveSceneToMesh(const char *filename) const
{
	mainEngine->SaveSceneToMesh(filename);
}

void UIEngine::GetScreenshot(ITMUChar4Image *dest) const
{
	glReadPixels(0, 0, dest->noDims.x, dest->noDims.y, GL_RGBA, GL_UNSIGNED_BYTE, dest->GetData(MEMORYDEVICE_CPU));
}

void UIEngine::GetFusionScreenshot(ITMUChar4Image *dest) const
{
	int textHeight = 30;
	glReadPixels(0, textHeight, dest->noDims.x, dest->noDims.y + textHeight, GL_RGBA, GL_UNSIGNED_BYTE, dest->GetData(MEMORYDEVICE_CPU));
}


void UIEngine::ProcessFrame()
{
	if (!imageSource->hasMoreImages()) return;
	imageSource->getImages(inputRGBImage, inputRawDepthImage);

	if (imuSource != NULL) {
		if (!imuSource->hasMoreMeasurements()) return;
		else imuSource->getMeasurement(inputIMUMeasurement);
	}

	sdkResetTimer(&timer_instant);
	sdkStartTimer(&timer_instant); sdkStartTimer(&timer_average);

	//actual processing on the mailEngine
	if (imuSource != NULL) mainEngine->ProcessFrame(inputRGBImage, inputRawDepthImage, inputIMUMeasurement);
	else mainEngine->ProcessFrame(inputRGBImage, inputRawDepthImage);

#ifndef COMPILE_WITHOUT_CUDA
	ITMSafeCall(cudaThreadSynchronize());
#endif
	sdkStopTimer(&timer_instant); sdkStopTimer(&timer_average);

	//processedTime = sdkGetTimerValue(&timer_instant);
	processedTime = sdkGetAverageTimerValue(&timer_average);

	if (isRecording){
		std::strstream ss1;
		std::string snapshotname1;
		ss1 << "snapshots/full_" << currentFrameNo << ".ppm";
		ss1 >> snapshotname1;
		SaveScreenshot(snapshotname1.c_str());

		std::strstream ss2;
		std::string snapshotname2;
		ss2 << "snapshots/fusion_" << currentFrameNo << ".ppm";
		ss2 >> snapshotname2;
		SaveFusionScreenshot(snapshotname2.c_str());
	}
	
	currentFrameNo++;
}

void UIEngine::Run() { glutMainLoop(); }
void UIEngine::Shutdown()
{
	sdkDeleteTimer(&timer_instant);
	sdkDeleteTimer(&timer_average);

	for (int w = 0; w < NUM_WIN; w++)
		delete outImage[w];

	delete inputRGBImage;
	delete inputRawDepthImage;
	delete inputIMUMeasurement;

	delete[] outFolder;
	delete saveImage;
	delete instance;
}
