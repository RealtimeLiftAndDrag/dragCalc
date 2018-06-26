// Core libraries
#include <iostream>
#include <cmath>

// Third party libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Local headers
#include "GLSL.h"
#include "Program.h"
#include "WindowManager.h"
#include "Shape.h"
#include "Camera.h"

#define PI 3.14159262

using namespace std;
using namespace glm;

double get_last_elapsed_time() {
	static double lasttime = glfwGetTime();
	double actualtime = glfwGetTime();
	double difference = actualtime - lasttime;
	lasttime = actualtime;
	return difference;
}

class drag_ssbo_data {
public:
	ivec4 sumF;
	ivec4 torque;
	//uvec4 counter;
};

class Application : public EventCallbacks {
public:
	WindowManager *windowManager = nullptr;
    Camera *camera = nullptr;

    std::shared_ptr<Shape> shape;
	std::shared_ptr<Program> dragShader;
    
    double gametime = 0;
    bool wireframeEnabled = false;
    bool mousePressed = false;
    bool mouseCaptured = false;
    glm::vec2 mouseMoveOrigin = glm::vec2(0);
    glm::vec3 mouseMoveInitialCameraRot;
	drag_ssbo_data drag_data;
	GLuint ssbo_drag;

    Application() {
        camera = new Camera();
    }
    
    ~Application() {
        delete camera;
    }

	void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
		// Movement
        if (key == GLFW_KEY_W && action != GLFW_REPEAT) camera->vel.z = (action == GLFW_PRESS) * -0.2f;
        if (key == GLFW_KEY_S && action != GLFW_REPEAT) camera->vel.z = (action == GLFW_PRESS) * 0.2f;
        if (key == GLFW_KEY_A && action != GLFW_REPEAT) camera->vel.x = (action == GLFW_PRESS) * -0.2f;
        if (key == GLFW_KEY_D && action != GLFW_REPEAT) camera->vel.x = (action == GLFW_PRESS) * 0.2f;
        // Rotation
        if (key == GLFW_KEY_I && action != GLFW_REPEAT) camera->rotVel.x = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_K && action != GLFW_REPEAT) camera->rotVel.x = (action == GLFW_PRESS) * -0.02f;
        if (key == GLFW_KEY_J && action != GLFW_REPEAT) camera->rotVel.y = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_L && action != GLFW_REPEAT) camera->rotVel.y = (action == GLFW_PRESS) * -0.02f;
        if (key == GLFW_KEY_U && action != GLFW_REPEAT) camera->rotVel.z = (action == GLFW_PRESS) * 0.02f;
        if (key == GLFW_KEY_O && action != GLFW_REPEAT) camera->rotVel.z = (action == GLFW_PRESS) * -0.02f;
        // Polygon mode (wireframe vs solid)
        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            wireframeEnabled = !wireframeEnabled;
            glPolygonMode(GL_FRONT_AND_BACK, wireframeEnabled ? GL_LINE : GL_FILL);
        }
        // Hide cursor (allows unlimited scrolling)
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            mouseCaptured = !mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            resetMouseMoveInitialValues(window);
        }
	}

	void mouseCallback(GLFWwindow *window, int button, int action, int mods) {
        mousePressed = (action != GLFW_RELEASE);
        if (action == GLFW_PRESS) {
            resetMouseMoveInitialValues(window);
        }
    }
    
    void mouseMoveCallback(GLFWwindow *window, double xpos, double ypos) {
        if (mousePressed || mouseCaptured) {
            float yAngle = (xpos - mouseMoveOrigin.x) / windowManager->getWidth() * 3.14159f;
            float xAngle = (ypos - mouseMoveOrigin.y) / windowManager->getHeight() * 3.14159f;
            camera->setRotation(mouseMoveInitialCameraRot + glm::vec3(-xAngle, -yAngle, 0));
        }
    }

	void resizeCallback(GLFWwindow *window, int in_width, int in_height) { }
    
    // Reset mouse move initial position and rotation
    void resetMouseMoveInitialValues(GLFWwindow *window) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        mouseMoveOrigin = glm::vec2(mouseX, mouseY);
        mouseMoveInitialCameraRot = camera->rot;
    }

	void initGeom(const std::string& resourceDirectory, string airfoilID = "0012") {
        shape = make_shared<Shape>();
        shape->loadMesh(resourceDirectory + "/" + airfoilID + ".obj");
        shape->resize();
        shape->init();
		drag_data.sumF = ivec4(0);
		drag_data.torque = ivec4(0);
		//drag_data.counter = uvec4(0);

		glGenBuffers(1, &ssbo_drag);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_drag);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(drag_data), &drag_data, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_drag);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // unbind

	}
	
	void init(const std::string& resourceDirectory) {
		GLSL::checkVersion();

		// Enable z-buffer test.
		glEnable(GL_DEPTH_TEST);
        
		// Initialize the GLSL programs
        dragShader = std::make_shared<Program>();
        dragShader->setShaderNames(resourceDirectory + "/drag.vert", resourceDirectory + "/drag.frag");
        dragShader->init();

		dragShader->bind();
		GLuint block_index = 0;
		block_index = glGetProgramResourceIndex(dragShader->getPID(), GL_SHADER_STORAGE_BLOCK, "shader_data");
		GLuint ssbo_binding_point_index = 2;
		glShaderStorageBlockBinding(dragShader->getPID(), block_index, ssbo_binding_point_index);
		dragShader->unbind();
	}
    
    glm::mat4 getPerspectiveMatrix() {
        float fov = 3.14159f / 4.0f;
        float aspect = windowManager->getAspect();
        return glm::perspective(fov, aspect, 0.01f, 10000.0f);
    }

	void render() {
		static uint frame = 0;
		double frametime = get_last_elapsed_time();
		gametime += frametime;

		// Clear framebuffer.
		glClearColor(0.3f, 0.7f, 0.8f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Create the matrix stacks.
		mat4 V, M, P;
		mat4 T, R, S;
        P = getPerspectiveMatrix();
        V = camera->getViewMatrix();
        M = glm::mat4(1);
        
		GLuint block_index = 0;
		block_index = glGetProgramResourceIndex(dragShader->getPID(), GL_SHADER_STORAGE_BLOCK, "shader_data");
		GLuint ssbo_binding_point_index = 0;
		glShaderStorageBlockBinding(dragShader->getPID(), block_index, ssbo_binding_point_index);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_drag);

        /**************/
        /* DRAW SHAPE */
        /**************/
		T = translate(glm::mat4(1), glm::vec3(0, 0, -2));
		R = rotate(mat4(1), float(PI / 2.0f), vec3(0, 1, 0));
		vec3 center = vec3(0, 0, -2);
		M = T * R;
        dragShader->bind();
        dragShader->setMVP(&M[0][0], &V[0][0], &P[0][0]);
		dragShader->setVector3("center", &center[0]);
        shape->draw(dragShader, false);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

		//step 1 getting data back from GPU to the stars object
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_drag);
		GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		int siz = sizeof(drag_data);
		memcpy(&drag_data, p, siz);
		glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

		cout << "frame: " << frame << endl;
		printf("torque for frame: (%i,%i,%i,%i)\n", 
			drag_data.torque.x,
			drag_data.torque.y,
			drag_data.torque.z,
			drag_data.torque.w
		);
		printf("summed Force for frame: (%i,%i,%i,%i)\n",
			drag_data.sumF.x,
			drag_data.sumF.y,
			drag_data.sumF.z,
			drag_data.sumF.w
		);
		//printf("Counter for frame: (%u,%u,%u,%u)\n",
		//	drag_data.counter.x,
		//	drag_data.counter.y,
		//	drag_data.counter.z,
		//	drag_data.counter.w
		//);
		cout << endl << endl;
		frame++;

		drag_data.torque = vec4(0);
		drag_data.sumF = vec4(0);
		//drag_data.counter = uvec4(0);
        dragShader->unbind();

		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(drag_data), &drag_data, GL_DYNAMIC_COPY);

	}
};

int main(int argc, char **argv) {
	std::string resourceDir = "../resources";
	string airfoilID;
	if (argc >= 2) {
		airfoilID = argv[1];
	}

	Application *application = new Application();

    // Initialize window.
	WindowManager * windowManager = new WindowManager();
	windowManager->init(800, 600);
	windowManager->setEventCallbacks(application);
	application->windowManager = windowManager;

	// Initialize scene.
	application->init(resourceDir);
	if(airfoilID.empty())
		application->initGeom(resourceDir);
	else
		application->initGeom(resourceDir, airfoilID);
    
	// Loop until the user closes the window.
	while (!glfwWindowShouldClose(windowManager->getHandle())) {
        // Update camera position.
        application->camera->update();
		// Render scene.
		application->render();

		// Swap front and back buffers.
		glfwSwapBuffers(windowManager->getHandle());
		// Poll for and process events.
		glfwPollEvents();
	}

	// Quit program.
	windowManager->shutdown();
	return 0;
}
