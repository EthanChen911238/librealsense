#include <librealsense/rs.h>
#include <librealsense/rsutil.h>
#include "example.h"

struct rs_error * error;
void check_error()
{
	if (error)
	{
		fprintf(stderr, "error calling %s(...):\n  %s\n", rs_get_failed_function(error), rs_get_error_message(error));
		rs_free_error(error);
		exit(EXIT_FAILURE);
	}
}

double yaw, pitch, lastX, lastY;
int ml;
static void on_mouse_button(GLFWwindow * win, int button, int action, int mods)
{
	if(button == GLFW_MOUSE_BUTTON_LEFT) ml = action == GLFW_PRESS;
}

static double clamp(double val, double lo, double hi) { return val < lo ? lo : val > hi ? hi : val; }
static void on_cursor_pos(GLFWwindow * win, double x, double y)
{
	if(ml)
	{
		yaw = clamp(yaw - (x - lastX), -120, 120);
		pitch = clamp(pitch + (y - lastY), -80, 80);
	}
	lastX = x;
	lastY = y;
}

int main(int argc, char * argv[])
{
	struct rs_context * ctx;
	struct rs_camera * cam;
	struct rs_intrinsics depth_intrin, color_intrin;
	struct rs_extrinsics extrin;
	char buffer[1024];
	GLFWwindow * win;
	int x, y, d;
	const uint16_t * depth;
	float scale;

	ctx = rs_create_context(RS_API_VERSION, &error); check_error();
	if(rs_get_camera_count(ctx, NULL) < 1)
	{
		fprintf(stderr, "No camera detected. Is it plugged in?\n");
		return EXIT_FAILURE;
	}

	cam = rs_get_camera(ctx, 0, &error); check_error();
	rs_enable_stream_preset(cam, RS_STREAM_DEPTH, RS_PRESET_BEST_QUALITY, &error); check_error();
	rs_enable_stream_preset(cam, RS_STREAM_COLOR, RS_PRESET_BEST_QUALITY, &error); check_error();
	rs_start_capture(cam, &error); check_error();

	rs_get_stream_intrinsics(cam, RS_STREAM_DEPTH, &depth_intrin, &error); check_error();
	rs_get_stream_intrinsics(cam, RS_STREAM_COLOR, &color_intrin, &error); check_error();
	rs_get_stream_extrinsics(cam, RS_STREAM_DEPTH, RS_STREAM_COLOR, &extrin, &error); check_error();
	scale = rs_get_depth_scale(cam, &error); check_error();

	glfwInit();
	sprintf(buffer, "C Point Cloud Example (%s)", rs_get_camera_name(cam,0));
	win = glfwCreateWindow(640, 480, buffer, 0, 0);
	glfwSetMouseButtonCallback(win, on_mouse_button);
	glfwSetCursorPosCallback(win, on_cursor_pos);
	glfwMakeContextCurrent(win);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	while (!glfwWindowShouldClose(win))
	{
		int width, height;

		glfwPollEvents();
		rs_wait_all_streams(cam, &error); check_error();

		glfwGetFramebufferSize(win, &width, &height);
		glViewport(0, 0, width, height);
		glClearColor(0.0f, 116/255.0f, 197/255.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glBindTexture(GL_TEXTURE_2D, tex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, color_intrin.image_size[0], color_intrin.image_size[1], 0, GL_RGB, GL_UNSIGNED_BYTE, rs_get_image_pixels(cam, RS_STREAM_COLOR, 0));

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		gluPerspective(60, (float)width/height, 0.01f, 20.0f);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		gluLookAt(0,0,0, 0,0,1, 0,-1,0);
		glTranslatef(0,0,+0.5f);
		glRotated(pitch, 1, 0, 0);
		glRotated(yaw, 0, 1, 0);
		glTranslatef(0,0,-0.5f);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glPointSize((float)width/640);
		glBegin(GL_POINTS);
		depth = rs_get_image_pixels(cam, RS_STREAM_DEPTH, 0);
		float scale = rs_get_depth_scale(cam, 0);
		for(y=0; y<depth_intrin.image_size[1]; ++y)
		{
			for(x=0; x<depth_intrin.image_size[0]; ++x)
			{
				if(d = *depth++)
				{
					float depth_pixel[2] = {(float)x, (float)y}, depth_point[3], color_point[3], color_pixel[2];
					rs_deproject_pixel_to_point(depth_point, &depth_intrin, depth_pixel, d*scale);
					rs_transform_point_to_point(color_point, &extrin, depth_point);
					rs_project_point_to_pixel(color_pixel, &color_intrin, color_point);

					glTexCoord2f(color_pixel[0] / color_intrin.image_size[0], color_pixel[1] / color_intrin.image_size[1]);
					glVertex3fv(depth_point);
				}
			}
		}
		glEnd();

		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();

		glfwSwapBuffers(win);
	}

	glfwDestroyWindow(win);
	glfwTerminate();

	rs_delete_context(ctx, &error); check_error();
	return EXIT_SUCCESS;
}