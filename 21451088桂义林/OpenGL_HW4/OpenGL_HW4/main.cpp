// Simple Solar System Simulation
// Use light and object material
// Use Texture Mapping
// Only 3 Spheres: the sun, the earth and the moon

#include <GLTools.h>
#include <GLShaderManager.h>
#include <GLFrustum.h>
#include <GLBatch.h>
#include <GLFrame.h>
#include <GLMatrixStack.h>
#include <GLGeometryTransform.h>
#include <StopWatch.h>

#include <math.h>
#include <stdio.h>

#ifdef __APPLE__
#include <glut/glut.h>
#else
#define FREEGLUT_STATIC
#include <GL/glut.h>
#endif

GLShaderManager shaderManager;  // Shader Manager
GLMatrixStack modelViewMatrix;     // Modelview Matrix
GLMatrixStack projectionMatrix;		// Projection Matrix
GLFrustum viewFrustum;                  // View Frustum
GLGeometryTransform transformPipeline;    // Geometry Transform Pipeline
GLFrame cameraFrame;                   // Camera frame

GLTriangleBatch sunSphereBatch;
GLTriangleBatch earthSphereBatch;
GLTriangleBatch moonSphereBatch;
GLBatch floorBatch;

GLuint uiTextures[3];

static GLfloat vWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };

/**
 * @brief Load texture from TGA image file
 */
bool LoadTGATexture(const char *szFileName, GLenum minFilter, 
					GLenum magFilter, GLenum wrapMode)
{
	GLbyte *pBits;
	int nWidth, nHeight, nComponents;
	GLenum eFormat;

	// Read the texture bits
	pBits = gltReadTGABits(szFileName, &nWidth, &nHeight, 
		&nComponents, &eFormat);
	if (pBits == NULL) 
		return false;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, 
		nWidth, nHeight, 0, eFormat, GL_UNSIGNED_BYTE, pBits);

	free(pBits);

	if (minFilter == GL_LINEAR_MIPMAP_LINEAR || 
		minFilter == GL_LINEAR_MIPMAP_NEAREST ||
		minFilter == GL_NEAREST_MIPMAP_LINEAR ||
		minFilter == GL_NEAREST_MIPMAP_NEAREST)
		glGenerateMipmap(GL_TEXTURE_2D);

	return true;
}

/**
 * @brief Initialization on the rendering context. 
 */
void SetupRC()
{
	// Initialze Shader Manager
	shaderManager.InitializeStockShaders();

	glEnable(GL_DEPTH_TEST);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// This makes a torus
	gltMakeSphere(sunSphereBatch, 0.4f, 30, 30);

	// This make a sphere
	gltMakeSphere(earthSphereBatch, 0.2f, 26, 13);

	// This make a small sphere
	gltMakeSphere(moonSphereBatch, 0.1, 18, 9);

	// Make the floor
	floorBatch.Begin(GL_LINES, 324);
	for(GLfloat x = -20.0; x <= 20.0f; x+= 0.5) 
	{
		floorBatch.Vertex3f(x, -0.55f, 20.0f);
		floorBatch.Vertex3f(x, -0.55f, -20.0f);

		floorBatch.Vertex3f(20.0f, -0.55f, x);
		floorBatch.Vertex3f(-20.0f, -0.55f, x);
	}
	floorBatch.End();

	// Make 3 texture objects
	glGenTextures(3, uiTextures);

	// Load Sun
	glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
	LoadTGATexture("sunmap.tga", GL_LINEAR_MIPMAP_LINEAR, 
		GL_LINEAR, GL_CLAMP_TO_EDGE);

	// Load Earth
	glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
	LoadTGATexture("earthmap.tga", GL_LINEAR_MIPMAP_LINEAR, 
		GL_LINEAR, GL_CLAMP_TO_EDGE);

	// Load Moon
	glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
	LoadTGATexture("moonmap.tga", GL_LINEAR_MIPMAP_LINEAR,
		GL_LINEAR, GL_CLAMP_TO_EDGE);
}

/**
 * @brief Do shutdown for the rendering context
 */
void ShutdownRC(void)
{
	glDeleteTextures(3, uiTextures);
}

/**
 * @brief Callback function. Called when screen changes size or is initialized.
 * @param [in] nWidth      the width of resized window
 * @param [in] nHeight     the height of resized window
 */
void ChangeSize(int nWidth, int nHeight)
{
	glViewport(0, 0, nWidth, nHeight);

	// Create the projection matrix, and load it on the projection matrix stack
	viewFrustum.SetPerspective(35.0f, float(nWidth)/float(nHeight), 1.0f, 100.0f);
	projectionMatrix.LoadMatrix(viewFrustum.GetProjectionMatrix());

	// Set the transformation pipeline to use the two matrix stacks 
	transformPipeline.SetMatrixStacks(modelViewMatrix, projectionMatrix);
}

/**
 * @brief Called to draw scene
 */
void RenderScene()
{
	// Color values
	static GLfloat vSunColor[] = { 1.0f, 0.3f, 0.0f, 1.0f };
	static GLfloat vEarthColor[] = { 0.0f, 0.0f, 1.0f, 1.0f };
	static GLfloat vMoonColor[] = { 0.84f, 0.93f, 0.95f, 1.0f };
	static GLfloat vFloorColor[] = { 0.0f, 0.64f, 0.0f, 1.0f};

	// Time Based animation
	static CStopWatch rotTimer;
	float yRot = rotTimer.GetElapsedSeconds() * 60.0f;

	// Clear the color and depth buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Save the current modelview matrix (the identity matrix)
	modelViewMatrix.PushMatrix();	

	M3DMatrix44f mCamera;
	cameraFrame.GetCameraMatrix(mCamera);
	modelViewMatrix.PushMatrix(mCamera);

	// Transform the light position into eye coordinates
	M3DVector4f vLightPos = { 0.0f, 10.0f, 5.0f, 1.0f };
	M3DVector4f vLightEyePos;
	m3dTransformVector4(vLightEyePos, vLightPos, mCamera);

	// Draw the floor
	shaderManager.UseStockShader(GLT_SHADER_FLAT,
		transformPipeline.GetModelViewProjectionMatrix(),
		vFloorColor);	
	floorBatch.Draw();    

	// Draw the spinning Sun
	modelViewMatrix.Translate(0.0f, 0.0f, -3.5f);

	// Save the Translation
	modelViewMatrix.PushMatrix();

	// Apply a rotation and draw the Sun
	modelViewMatrix.Rotate(yRot, 0.0f, 1.0f, 0.0f);
	glBindTexture(GL_TEXTURE_2D, uiTextures[0]);
	shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF, 
		transformPipeline.GetModelViewMatrix(), 
		transformPipeline.GetProjectionMatrix(), vLightEyePos, vSunColor, 0);
	sunSphereBatch.Draw();
	modelViewMatrix.PopMatrix(); // "Erase" the Rotation from before

	// Apply another rotation, followed by a translation, then draw the Earth
	modelViewMatrix.Rotate(yRot * -2.0f, 0.0f, 1.0f, 0.0f);
	modelViewMatrix.Translate(0.8f, 0.0f, 0.0f);
	glBindTexture(GL_TEXTURE_2D, uiTextures[1]);
	shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF, 
		transformPipeline.GetModelViewMatrix(), 
		transformPipeline.GetProjectionMatrix(), vLightEyePos, vWhite, 0);
	earthSphereBatch.Draw();

	// Apply another rotation, followed by a translation, then draw the Moon
	modelViewMatrix.Rotate(yRot * -4.0f, 0.0f, 1.0f, 0.0f);
	modelViewMatrix.Translate(0.4f, 0.0f, 0.0f);
	glBindTexture(GL_TEXTURE_2D, uiTextures[2]);
	shaderManager.UseStockShader(GLT_SHADER_TEXTURE_POINT_LIGHT_DIFF, 
		transformPipeline.GetModelViewMatrix(), 
		transformPipeline.GetProjectionMatrix(), vLightEyePos, vMoonColor, 0);
	moonSphereBatch.Draw();

	// Restore the previous modleview matrix (the identity matrix)
	modelViewMatrix.PopMatrix();
	modelViewMatrix.PopMatrix();    
	modelViewMatrix.PopMatrix();   

	// Do the buffer Swap
	glutSwapBuffers();

	// Tell GLUT to do it again
	glutPostRedisplay();
}

/**
 * @brief Respond to arrow keys by moving the camera frame of reference
 * @param [in] key    Specify which key is pressed
 * @param [in] x        x coordinates of the touch point with the mouse
 * @param [in] y        y coordinates of the touch point with the mouse
 */
void SpecialKeys(int key, int x, int y)
{
	float linear = 0.1f;
	float angular = float(m3dDegToRad(5.0f));

	if(key == GLUT_KEY_UP)
		cameraFrame.MoveForward(linear);

	if(key == GLUT_KEY_DOWN)
		cameraFrame.MoveForward(-linear);

	if(key == GLUT_KEY_LEFT)
		cameraFrame.RotateWorld(angular, 0.0f, 1.0f, 0.0f);

	if(key == GLUT_KEY_RIGHT)
		cameraFrame.RotateWorld(-angular, 0.0f, 1.0f, 0.0f);		
}

int main(int argc, char* argv[])
{
	gltSetWorkingDirectory(argv[0]);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(800, 600);

	glutCreateWindow("Simple Solar System with light and material");

	glutSpecialFunc(SpecialKeys);
	glutReshapeFunc(ChangeSize);
	glutDisplayFunc(RenderScene);

	GLenum err = glewInit();
	if (GLEW_OK != err) 
	{
		fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
		return 1;
	}

	SetupRC();
	glutMainLoop();   
	ShutdownRC();

	return 0;
}
