#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#if _WIN32
#   include <Windows.h>
#endif
#if __APPLE__
#   include <OpenGL/gl.h>
#   include <OpenGL/glu.h>
#   include <GLUT/glut.h>
#else
#   include <GL/gl.h>
#   include <GL/glu.h>
#   include <GL/glut.h>
#endif

#define ESC 27
#define SPACEBAR 32
#define G -9.8f // the acceleration of gravity
#define MAX_TESSELLATION 64
#define MILLI 1000.0f
#define SPEED_SEA_WAVE -0.2f
#define CANNON_SPEED 3.0f
#define RED 0
#define BLUE 1
#define ISLAND 2
#define MAX_BALL_NUM 7
#define NUM_SEG 8 // number of segment to draw defence circle
#define NUM_CIRCLES 4
#define RATE_DEFENCE_INCREASE 0.2f // rate the radius of defence circle increases
// the y position of red boat
#define Y_POS_RED A * sin(k * (global.distanceRed - global.offset))
// the rotation angle of red boat on sea wave
#define R_ANGLE_RED atan(A * k * cos(k * (global.distanceRed - global.offset)))
#define Y_POS_BLUE A * sin(k * (global.distanceBlue - global.offset))
#define R_ANGLE_BLUE atan(A * k * cos(k * (global.distanceBlue - global.offset)))
// initialize r0 of cannon ball from red boat
#define INITIAL_R0_RED (float)(global.distanceRed - 0.025 * sin(R_ANGLE_RED) + 0.05 * cos(R_ANGLE_RED) + 0.06 * cos(global.angleRed * pi / 180 + R_ANGLE_RED)), (float)(Y_POS_RED + 0.025 * cos(R_ANGLE_RED) + 0.05 * sin(R_ANGLE_RED) + 0.06 * sin(global.angleRed * pi / 180 + R_ANGLE_RED))
// initialize v0 of cannon ball from red boat
#define INITIAL_V0_RED (float)(CANNON_SPEED * cos(global.angleRed * pi / 180 + R_ANGLE_RED)), (float)(CANNON_SPEED * sin(global.angleRed * pi / 180 + R_ANGLE_RED))
#define INITIAL_R0_BLUE (float)(global.distanceBlue - 0.025 * sin(R_ANGLE_BLUE) - 0.05 * cos(R_ANGLE_BLUE) + 0.06 * cos(R_ANGLE_RED) + 0.06 * cos(global.angleBlue * pi / 180 + R_ANGLE_BLUE)), (float)(Y_POS_BLUE + 0.025 * cos(R_ANGLE_BLUE) - 0.05 * sin(R_ANGLE_BLUE) + 0.06 * sin(global.angleBlue * pi / 180 + R_ANGLE_BLUE))
#define INITIAL_V0_BLUE (float)(CANNON_SPEED * cos(global.angleBlue * pi / 180 + R_ANGLE_BLUE)), (float)(CANNON_SPEED * sin(global.angleBlue * pi / 180 + R_ANGLE_BLUE))
// initialize r0 of cannon ball from island
#define INITIAL_R0_ISLAND 0.25f * (float)cos(global.angleIsland * pi / 180.0), 0.25f + 0.25f * (float)sin(global.angleIsland * pi / 180.0)
// initialize v0 of cannon ball from island
#define INITIAL_V0_ISLAND CANNON_SPEED * (float)cos(global.angleIsland * pi / 180.0), CANNON_SPEED * (float)sin(global.angleIsland * pi / 180.0)

// define pi
const float pi = (float)(4.0 * atan(1.0));
// sine wave amplitude
float A = 0.25;
// wave length
float lamda = 2.0;
// frequence
float k = 4.0 * pi / lamda;


typedef struct {
	float angleBlue; // blue boat cannon shooting angle, initial value is 150 degrees;
	float angleIsland; // initial value is 30 degrees
	float angleRed; // initial value is 30 degrees
	int ballNumBlue;
	int ballNumIsland;
	int ballNumRed;
	float bloodBlue; // the length of blood bar of the blue boat
	float bloodIsland;
	float bloodRed;
	bool debug;
	bool disappear;
	float distanceBlue; // the distance of blue boat moving from the original position x = 0
	float distanceRed;
	bool drawNormal;
	bool drawTangent;
	bool fireIsland; // default false
	int frames;
	float frameRate;
	float frameRateInterval;
	bool go; // start animation
	float lastFrameRateT;
	float offset; // sea wave movement from original place on x axis
	float startTime; // sea wave animation start time
	int numTessellation; // number of tessellation of the water surface
	bool wireframeMode; // true - enable glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
} global_t;

global_t global = { 150.0, 30.0, 30.0, 0, 0, 0, 0.5, 0.5, 0.5, false, false, 0.5, -0.5, false, false, false, 0, 0.0, 0.0, false, 0.0, 0.0, 0.0, 32, false };

typedef struct { float x, y; } vec2f;

typedef struct { 
	vec2f r0, v0, r, v; 
	bool fire; bool disappear; 
	float startTime; float stopTime; 
	bool isDefence; float radius; 
	bool hitRed; bool hitBlue; bool hitIsland; bool bloodChanged; 
} state; // r - position

state ball[3][MAX_BALL_NUM];

void drawAxes(float amplifyFactor, float layer) {
	glBegin(GL_LINES);
	// draw tangent vector
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(0.0, 0.0, layer);
	glVertex3f(1.0 * amplifyFactor, 0.0, layer);
	// draw normal vector
	glColor3f(0.0, 1.0, 0.0);
	glVertex3f(0.0, 0.0, layer);
	glVertex3f(0.0, 1.0 * amplifyFactor, layer);
	glEnd();
}

// num_tessellation + 1 is num_points; num_points * 2 addes the points on the bottom of the polygons (sea flood). 
// num_points * 2 * 3 is num_vertices
GLfloat sinewave[(MAX_TESSELLATION + 1) * 6];

void drawSeaWave() {
	// sine wave amplitude
	float A = 0.25;
	// wave length
	float lamda = 2.0;
	// frequence
	float k = 4.0 * pi / lamda;
	// for loop parameter
	int i; int j = 0;
	// step for increment
	float step = (1.0 - (-1.0)) / global.numTessellation;
	// x, y variables of the Quadratic
	float x; float y;

	glColor4f(0.2, 0.6, 0.8, 0.6);
	if (global.wireframeMode)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (i = 0; i <= global.numTessellation; i++) {
		sinewave[j] = sinewave[j + 3] = x = -1.0 + i * step;
		sinewave[j + 1] = -1.0; // draw the points on the sea floor
		sinewave[j + 4] = y = A * sin(k * (x - global.offset)); // calculate y = ASin(k(x - offset))
		sinewave[j + 2] = sinewave[j + 5] = 1.0;
		j += 6;
	}

	GLubyte indices[MAX_TESSELLATION * 6];
	j = 0;
	for (i = 0; i < global.numTessellation * 2; i++) {
		indices[j] = i;
		indices[j + 1] = i + 1;
		indices[j + 2] = i + 2;
		j += 3;
	}
	
	// activate and specify pointer to vertex array
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, sinewave);

	// draw the array as triangles
	glDrawElements(GL_TRIANGLES, global.numTessellation * 6, GL_UNSIGNED_BYTE, indices);

	// deactivate vertex arrays after drawing
	glDisableClientState(GL_VERTEX_ARRAY);
}

void drawBoatRed() {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(global.distanceRed, A * sin(k * (global.distanceRed - global.offset)), 0);
	glRotatef(R_ANGLE_RED * 180 / pi, 0, 0, 1);

	drawAxes(0.1, 0.5);
	// red color
	glColor3f(1.0, 0.0, 0.0);
	// draw boat bridge
	glBegin(GL_QUADS);
	glVertex3f(-0.025, 0.025, 0.5);
	glVertex3f(0.025, 0.025, 0.5);
	glVertex3f(0.025, 0.075, 0.5);
	glVertex3f(-0.025, 0.075, 0.5);
	glEnd();

	// draw boat hull
	glBegin(GL_QUADS);
	glVertex3f(-0.1, 0.025, 0.5);
	glVertex3f(-0.05, -0.025, 0.5);
	glVertex3f(0.05, -0.025, 0.5);
	glVertex3f(0.1, 0.025, 0.5);
	glEnd();

	// draw the cannon
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(global.distanceRed - 0.025 * sin(R_ANGLE_RED) + 0.05 * cos(R_ANGLE_RED), Y_POS_RED + 0.025 * cos(R_ANGLE_RED) + 0.05 * sin(R_ANGLE_RED), 0);
	glRotatef(global.angleRed + R_ANGLE_RED * 180 / pi, 0, 0, 1); // global.angleRed is the angle of cannon 

	glBegin(GL_QUADS);
	glVertex3f(0, -0.00625, 0.5);
	glVertex3f(0.06, -0.00625, 0.5);
	glVertex3f(0.06, 0.00625, 0.5);
	glVertex3f(0, 0.00625, 0.5);
	glEnd();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, 0);
}

void drawBoatBlue() {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(global.distanceBlue, A * sin(k * (global.distanceBlue - global.offset)), 0);
	glRotatef(R_ANGLE_BLUE * 180 / pi, 0, 0, 1);

	drawAxes(0.1, 0.5);
	// blue color
	glColor3f(0.0, 0.0, 1.0);
	// draw boat bridge
	glBegin(GL_QUADS);
	glVertex3f(-0.025, 0.025, 0.5);
	glVertex3f(0.025, 0.025, 0.5);
	glVertex3f(0.025, 0.075, 0.5);
	glVertex3f(-0.025, 0.075, 0.5);
	glEnd();

	// draw boat hull
	glBegin(GL_QUADS);
	glVertex3f(-0.1, 0.025, 0.5);
	glVertex3f(-0.05, -0.025, 0.5);
	glVertex3f(0.05, -0.025, 0.5);
	glVertex3f(0.1, 0.025, 0.5);
	glEnd();

	// draw the cannon
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(global.distanceBlue - 0.025 * sin(R_ANGLE_BLUE) - 0.05 * cos(R_ANGLE_BLUE), Y_POS_BLUE + 0.025 * cos(R_ANGLE_BLUE) - 0.05 * sin(R_ANGLE_BLUE), 0);
	glRotatef(global.angleBlue + R_ANGLE_BLUE * 180 / pi, 0, 0, 1); // global.angleRed is the angle of cannon 

	glBegin(GL_QUADS);
	glVertex3f(0, -0.00625, 0.5);
	glVertex3f(0.06, -0.00625, 0.5);
	glVertex3f(0.06, 0.00625, 0.5);
	glVertex3f(0, 0.00625, 0.5);
	glEnd();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, 0);
}

void drawIsland() {
	glColor3f(1.0, 1.0, 0.0);
	glBegin(GL_QUADS);
	glVertex3f(-0.25, -1.0, 0.8);
	glVertex3f(-0.25, 0.25, 0.8);
	glVertex3f(0.25, 0.25, 0.8);
	glVertex3f(0.25, -1.0, 0.8);
	glEnd();

	// draw the cannon
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.25, 0);
	glRotatef(global.angleIsland, 0, 0, 1); // global.angleIsland is the angle of cannon 

	glBegin(GL_QUADS);
	glVertex3f(0, -0.032, 0.8);
	glVertex3f(0.25, -0.032, 0.8);
	glVertex3f(0.25, 0.032, 0.8);
	glVertex3f(0, 0.032, 0.8);
	glEnd();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0, 0.0, 0);
}

void updateBloodBar() {
	int i, j;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < MAX_BALL_NUM; i++) {
			if (ball[j][i].hitRed && !ball[j][i].bloodChanged) {
				global.bloodRed -= global.bloodRed / 10.0;
				ball[j][i].bloodChanged = true;
			}
			if (ball[j][i].hitBlue && !ball[j][i].bloodChanged) {
				global.bloodBlue -= global.bloodBlue / 10.0;
				ball[j][i].bloodChanged = true;
			}
			if (ball[j][i].hitIsland && !ball[j][i].bloodChanged) {
				global.bloodIsland -= global.bloodIsland / 100.0;
				ball[j][i].bloodChanged = true;
			}
		}
	}
}

void drawBloodBar() {
	// blood bar of red boat
	glBegin(GL_QUADS);
	glColor3f(1.0, 0.0, 0.0);
	glVertex3f(-0.9, 0.9, 0.0);
	glVertex3f(-0.9, 0.85, 0.0);
	glVertex3f(-0.9 + global.bloodRed, 0.85, 0.0);
	glVertex3f(-0.9 + global.bloodRed, 0.9, 0.0);
	glEnd();
	// blood bar of blue boat
	glBegin(GL_QUADS);
	glColor3f(0.0, 0.0, 1.0);
	glVertex3f(-0.9, 0.85, 0.0);
	glVertex3f(-0.9, 0.8, 0.0);
	glVertex3f(-0.9 + global.bloodBlue, 0.8, 0.0);
	glVertex3f(-0.9 + global.bloodBlue, 0.85, 0.0);
	glEnd();
	// blood bar of island
	glBegin(GL_QUADS);
	glColor3f(1.0, 1.0, 0.0);
	glVertex3f(-0.9, 0.8, 0.0);
	glVertex3f(-0.9, 0.75, 0.0);
	glVertex3f(-0.9 + global.bloodIsland, 0.75, 0.0);
	glVertex3f(-0.9 + global.bloodIsland, 0.8, 0.0);
	glEnd();
}

void drawNormal() {
	// sine wave amplitude
	float A = 0.25;
	// wave length
	float lamda = 2.0;
	// frequence
	float k = 4.0 * pi / lamda;
	// step for increment
	float step = lamda / global.numTessellation;
	// x, y variables of the Quadratic
	float x; float y;
	// dx is the increment of x
	float dx;
	float dy;
	float s;
	// for loop parameter
	int i;

	glBegin(GL_LINES);
	glColor3f(0.0, 1.0, 0.0);
	for (i = 0; i <= global.numTessellation; i++) {
		x = sinewave[i * 6];
		y = sinewave[i * 6 + 4];
		dx = 1.0;
		dy = A * k * cos(k * (x - global.offset));
		if (global.debug)
			printf("%f, %f\n", dx, dy);
		s = sqrtf(dx * dx + dy * dy);
		dx /= s;
		dy /= s;

		glVertex3f(x, y, 0.0f);
		glVertex3f(x - dy / 10, y + dx / 10, 1.0f);
	}
	glEnd();
}

void drawTangent() {
	// sine wave amplitude
	float A = 0.25;
	// wave length
	float lamda = 2.0;
	// frequence
	float k = 4.0 * pi / lamda;
	// step for increment
	float step = lamda / global.numTessellation;
	// x, y variables of the Quadratic
	float x; float y;
	// dx is the increment of x
	float dx;
	float dy;
	float s;
	// for loop parameter
	int i;

	glBegin(GL_LINES);
	glColor3f(1.0, 1.0, 0.0);
	for (i = 0; i <= global.numTessellation; i++) {
		x = sinewave[i * 6];
		y = sinewave[i * 6 + 4];
		dx = 1.0;
		dy = A * k * cos(k * (x - global.offset));
		if (global.debug)
			printf("%f, %f\n", dx, dy);
		s = sqrtf(dx * dx + dy * dy);
		dx /= s;
		dy /= s;

		glVertex3f(x, y, 1.0);
		glVertex3f(x + dx / 8.0, y + dy / 8.0, 1.0);
	}
	glEnd();
}

void drawParabola(int type, int i) {
	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINE_STRIP);
	float x, y;
	float t = 0.0;
	float step = 0.01;
	do {
		x = ball[type][i].r0.x + ball[type][i].v0.x * t;
		y = ball[type][i].r0.y + ball[type][i].v0.y * t + 0.5 * G * t * t; // using parametric formular to calculate y of parabola
		if (global.debug)
			printf("x: %2f, y: %2f\n", x, y);
		if (global.angleIsland < 90 && x > ball[type][i].r.x || global.angleIsland > 90 && x < ball[type][i].r.x)
			glVertex3f(x, y, 0.0);
		t += step;
	} while (y > A * sin(k * (x - global.offset)));
	glEnd();
}

void drawCircle(int type, int i) {
	int m, n; // temporary value used by for loop
	float theta;
	float x, y;

	glColor3f(1.0, 1.0, 1.0);
	glBegin(GL_LINES);
	for (n = 0; n < 4; n++) {
		glVertex3f(ball[type][i].r.x + log2(NUM_CIRCLES) * ball[type][i].radius * cos(n * pi / 4.0), ball[type][i].r.y + log2(NUM_CIRCLES) * ball[type][i].radius * sin(n * pi / 4.0), 0.0);
		glVertex3f(ball[type][i].r.x + log2(NUM_CIRCLES) * ball[type][i].radius * cos((n + 4) * pi / 4.0), ball[type][i].r.y + log2(NUM_CIRCLES) * ball[type][i].radius * sin((n + 4) * pi / 4.0), 0.0);
	}
	glEnd();

	glLineWidth(2.0);
	glBegin(GL_LINE_LOOP);
	for (m = 1; m <= NUM_CIRCLES; m++) {
		for (n = 0; n <= NUM_SEG; n++) {
			theta = 2 * pi * n / NUM_SEG;
			x = ball[type][i].r.x + log2(m) * ball[type][i].radius * cos(theta); // ball[type][i].r.x is the x coordinate of circle center
			y = ball[type][i].r.y + log2(m) * ball[type][i].radius * sin(theta); // ball[type][i].r.y is the y coordinate of circle center
			if (global.debug)
				printf("x: %2f, y: %2f\n", x, y);
			glVertex3f(x, y, 0.0);
		}
	}
	glEnd();
}

void drawCannonBall(int type, int i) {
	glColor3f(1.0, 1.0, 1.0);
	glPointSize(5.0);
	glBegin(GL_POINTS);
	glVertex3f(ball[type][i].r.x, ball[type][i].r.y, 0.5);
	glEnd();
}

void updateCannon(float t, int type, int i) {
	if (ball[type][i].r.y >= A * sin(k * (ball[type][i].r.x - global.offset))) {
		ball[type][i].r.x = ball[type][i].v0.x * t + ball[type][i].r0.x;
		ball[type][i].r.y = 0.5 * G * t * t + ball[type][i].v0.y * t + ball[type][i].r0.y;
	} else {
		ball[type][i].disappear = true;
		ball[type][i].fire = false;
	}
}

void updateSeaWave(float t) {
	global.offset = SPEED_SEA_WAVE * t;
}

void updateDefenceRadius(float t, int type, int i) {
	ball[type][i].radius = RATE_DEFENCE_INCREASE * t;
}

// Idle callback for animation
void update() {
	static float lastT = -1.0;
	float t, dt;

	if (!global.go)
		return;

	t = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;

	if (lastT < 0.0) {
		lastT = t;
		return;
	}

	dt = t - lastT;
	if (global.debug)
		printf("%f %f\n", t, ball[0][0].startTime);
	updateSeaWave(t);
	/**
	 * different number of j represents different type of cannon ball
	 * 0 - cannon ball from red boat
	 * 1 - cannon ball from blue boat
	 * 2 - cannon ball from island
	 */
	int i, j, m, n;
	for (j = 0; j < 3; j++) {
		for (i = 0; i < MAX_BALL_NUM; i++) {
			// check if the ball is defended
			for (m = 0; m < 2; m++) {
				for (n = 0; n < MAX_BALL_NUM; n++) {
					if (ball[m][n].isDefence && ball[m][n].radius > 0 && j != m) {
						if (pow(ball[j][i].r.x - ball[m][n].r.x, 2.0) + pow(ball[j][i].r.y - ball[m][n].r.y, 2.0) <= pow(log2(NUM_CIRCLES) * ball[m][n].radius, 2.0)) {
							ball[j][i].disappear = true;
							ball[j][i].fire = false;
						}
					}
				}
			}

			if (ball[j][i].fire) {
				// if ball[j][i] is a cannon ball and is not shooted by red boat and is landed within the range of the red boat 
				if (!ball[j][i].isDefence && j != RED
					&& fabs(ball[j][i].r.x - global.distanceRed) < 0.05 * cos(R_ANGLE_RED * 180 / pi)
					&& fabs(ball[j][i].r.y - A * sin(k * (global.distanceRed - global.offset))) < 0.05 * sin(R_ANGLE_RED * 180 / pi)) {
					ball[j][i].hitRed = true;
				}
				else if (!ball[j][i].isDefence && j != BLUE
					&& fabs(ball[j][i].r.x - global.distanceBlue) < 0.05 * cos(R_ANGLE_BLUE * 180 / pi)
					&& fabs(ball[j][i].r.y - A * sin(k * (global.distanceBlue - global.offset))) < 0.05 * sin(R_ANGLE_BLUE * 180 / pi)) {
					ball[j][i].hitBlue = true;
				}
				else if (!ball[j][i].isDefence && j != ISLAND && fabs(ball[i][j].r.x) < 0.25 && ball[i][j].r.y < 0.25) {
					ball[j][i].hitIsland = true;
				}

				if (global.startTime <= ball[j][i].startTime) {
					updateCannon((t - ball[j][i].startTime) / 5.0, j, i);

					if (ball[j][i].isDefence) {
						if (t - ball[j][i].startTime < 1.0)
							updateDefenceRadius((t - ball[j][i].startTime) / 5.0, j, i);
						else {
							ball[j][i].radius = 0.0;
							ball[j][i].disappear = true;
							ball[j][i].fire = false;
						}
					}
				} else {
					updateCannon(t / 5.0, j, i);
					if (ball[j][i].isDefence) {
						if (t < 1.0)
							updateDefenceRadius(t / 5.0, j, i);
						else {
							ball[j][i].radius = 0.0;
							ball[j][i].disappear = true;
							ball[j][i].fire = false;
						}
					}
				}
			}
		}
	}
	lastT = t;

	/* Frame rate */
	dt = t - global.lastFrameRateT;
	if (dt > global.frameRateInterval) {
		global.frameRate = global.frames / dt;
		global.lastFrameRateT = t;
		global.frames = 0;
	}

	glutPostRedisplay();
}

void displayOSD()
{
	char buffer[30];
	char *bufp;
	int w, h;

	glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	/* Set up orthographic coordinate system to match the
	window, i.e. (0,0)-(w,h) */
	w = glutGet(GLUT_WINDOW_WIDTH);
	h = glutGet(GLUT_WINDOW_HEIGHT);
	glOrtho(0.0, w, 0.0, h, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	/* Frame rate */
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos2i(150, 280);
	snprintf(buffer, sizeof buffer, "fr (f/s): %6.0f", global.frameRate);
	for (bufp = buffer; *bufp; bufp++)
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *bufp);

	/* Time per frame */
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos2i(150, 260);
	snprintf(buffer, sizeof buffer, "ft (ms/f): %5.0f", 1.0 / global.frameRate * 1000.0);
	for (bufp = buffer; *bufp; bufp++)
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *bufp);

	/* Number of tessllation */
	glColor3f(1.0, 1.0, 1.0);
	glRasterPos2i(150, 240);
	snprintf(buffer, sizeof buffer, "tess: %10.0d", global.numTessellation);
	for (bufp = buffer; *bufp; bufp++)
		glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *bufp);

	/* Pop modelview */
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);

	/* Pop projection */
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	/* Pop attributes */
	glPopAttrib();
}

void display() {
	GLenum error;

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPushMatrix();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	drawAxes(1.0, 0.0);
	drawBoatRed();
	drawBoatBlue();
	drawIsland();
	updateBloodBar();
	drawBloodBar();

	int i;
	for (i = 0; i < MAX_BALL_NUM; i++) {
		if (ball[ISLAND][i].fire && !ball[ISLAND][i].disappear) {
			drawCannonBall(ISLAND, i);
			drawParabola(ISLAND, i);
		}
		if (ball[RED][i].fire && !ball[RED][i].disappear) {
			if (ball[RED][i].isDefence) {
				drawCircle(RED, i);
			} else {
				drawCannonBall(RED, i);
				drawParabola(RED, i);
			}
		}
		if (ball[BLUE][i].fire && !ball[BLUE][i].disappear) {
			if (ball[BLUE][i].isDefence) {
				drawCircle(BLUE, i);
			} else {
				drawCannonBall(BLUE, i);
				drawParabola(BLUE, i);
			}
		}
	}

	if (global.drawNormal)
		drawNormal();
	if (global.drawTangent)
		drawTangent();

	drawSeaWave();
	displayOSD();

	glPopMatrix();
	glutSwapBuffers();
	global.frames++;

	/* check OpenGL error */
	while ((error = glGetError()) != GL_NO_ERROR)
		printf("%s\n", gluErrorString(error));
}

/* ASCII key press responces */
void keyboard(unsigned char key, int x, int y) {
	float fireTimeRed = -1.0;
	float defenceTimeRed = -1.0;
	float fireTimeBlue = -1.0;
	float defenceTimeBlue = -1.0;
	float fireTimeIsland = -1.0;
	switch (key) {
	case 'n':
		global.drawNormal = !global.drawNormal;
		break;
	case 't':
		global.drawTangent = !global.drawTangent;
		break;
	case 'q':
		if (global.angleRed < 180.0)
			global.angleRed += 1.0;
		break;
	case 'Q':
		if (global.angleRed > 0)
			global.angleRed -= 1.0;
		break;
	case 'a':
		if (global.distanceRed > -1.0)
			global.distanceRed -= 0.01;
		break;
	case 'd':
		if (global.distanceRed < 1.0)
			global.distanceRed += 0.01;
		break;
	case 'o':
		if (global.angleBlue < 180.0)
			global.angleBlue += 1.0;
		break;
	case 'O':
		if (global.angleBlue > 0.0)
			global.angleBlue -= 1.0;
		break;
	case 'j':
		if (global.distanceBlue > -1.0)
			global.distanceBlue -= 0.01;
		break;
	case 'l':
		if (global.distanceBlue < 1.0)
			global.distanceBlue += 0.01;
		break;
	case 'e': // red boat fire
		if (glutGet(GLUT_ELAPSED_TIME) / MILLI - fireTimeRed > 1.0) {
			
			// initialize the r0 and v0 of this cannon ball
			if (!ball[RED][global.ballNumRed].fire) {
				ball[RED][global.ballNumRed].r0 = ball[RED][global.ballNumRed].r = { INITIAL_R0_RED };
				ball[RED][global.ballNumRed].v0 = ball[RED][global.ballNumRed].v = { INITIAL_V0_RED };
				ball[RED][global.ballNumRed].startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;
				ball[RED][global.ballNumRed].fire = true;
				ball[RED][global.ballNumRed].disappear = false;
				ball[RED][global.ballNumRed].isDefence = false;
				ball[RED][global.ballNumRed].radius = 0.0;
				ball[RED][global.ballNumRed].hitRed = false;
				ball[RED][global.ballNumRed].hitBlue = false;
				ball[RED][global.ballNumRed].hitIsland = false;
				ball[RED][global.ballNumRed].bloodChanged = false;
				if (global.ballNumRed < MAX_BALL_NUM - 1)
					global.ballNumRed++;
				else
					global.ballNumRed = 0;
			}
			fireTimeRed = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		}
		break;
	case 'i': // blue boat fire
		if (glutGet(GLUT_ELAPSED_TIME) / MILLI - fireTimeBlue> 1.0) {
			
			// initialize the r0 and v0 of this cannon ball from blue boat
			if (!ball[BLUE][global.ballNumBlue].fire) {
				ball[BLUE][global.ballNumBlue].r0 = ball[BLUE][global.ballNumBlue].r = { INITIAL_R0_BLUE };
				ball[BLUE][global.ballNumBlue].v0 = ball[BLUE][global.ballNumBlue].v = { INITIAL_V0_BLUE };
				ball[BLUE][global.ballNumBlue].startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;
				ball[BLUE][global.ballNumBlue].fire = true;
				ball[BLUE][global.ballNumBlue].disappear = false;
				ball[BLUE][global.ballNumBlue].isDefence = false;
				ball[BLUE][global.ballNumBlue].radius = 0.0;
				ball[BLUE][global.ballNumBlue].hitRed = false;
				ball[BLUE][global.ballNumBlue].hitBlue = false;
				ball[BLUE][global.ballNumBlue].hitIsland = false;
				ball[BLUE][global.ballNumBlue].bloodChanged = false;
				if (global.ballNumBlue < MAX_BALL_NUM - 1)
					global.ballNumBlue++;
				else
					global.ballNumBlue = 0;
			}
			fireTimeBlue = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		}
		break;
	case 'r': // red boat defence
		if (glutGet(GLUT_ELAPSED_TIME) / MILLI - defenceTimeRed > 1.0) {
			
			// initialize the r0 and v0 of this cannon ball
			if (!ball[RED][global.ballNumRed].fire) {
				ball[RED][global.ballNumRed].r0 = ball[RED][global.ballNumRed].r = { INITIAL_R0_RED };
				ball[RED][global.ballNumRed].v0 = ball[RED][global.ballNumRed].v = { INITIAL_V0_RED };
				ball[RED][global.ballNumRed].startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;
				ball[RED][global.ballNumRed].fire = true;
				ball[RED][global.ballNumRed].disappear = false;
				ball[RED][global.ballNumRed].isDefence = true;
				ball[RED][global.ballNumRed].radius = 0.0;
				ball[RED][global.ballNumRed].hitRed = false;
				ball[RED][global.ballNumRed].hitBlue = false;
				ball[RED][global.ballNumRed].hitIsland = false;
				ball[RED][global.ballNumRed].bloodChanged = false;
				if (global.ballNumRed < MAX_BALL_NUM - 1)
					global.ballNumRed++;
				else
					global.ballNumRed = 0;
			}
			defenceTimeRed = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		}
		break;
	case 'u': // blue boat defence
		if (glutGet(GLUT_ELAPSED_TIME) / MILLI - defenceTimeBlue > 1.0) {
		
			// initialize the r0 and v0 of this cannon ball from blue boat
			if (!ball[BLUE][global.ballNumBlue].fire) {
				ball[BLUE][global.ballNumBlue].r0 = ball[BLUE][global.ballNumBlue].r = { INITIAL_R0_BLUE };
				ball[BLUE][global.ballNumBlue].v0 = ball[BLUE][global.ballNumBlue].v = { INITIAL_V0_BLUE };
				ball[BLUE][global.ballNumBlue].startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;
				ball[BLUE][global.ballNumBlue].fire = true;
				ball[BLUE][global.ballNumBlue].disappear = false;
				ball[BLUE][global.ballNumBlue].isDefence = true;
				ball[BLUE][global.ballNumBlue].radius = 0.0;
				ball[BLUE][global.ballNumBlue].hitRed = false;
				ball[BLUE][global.ballNumBlue].hitBlue = false;
				ball[BLUE][global.ballNumBlue].hitIsland = false;
				ball[BLUE][global.ballNumBlue].bloodChanged = false;
				if (global.ballNumBlue < MAX_BALL_NUM - 1)
					global.ballNumBlue++;
				else
					global.ballNumBlue = 0;
			}
			defenceTimeBlue = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		}
		break;
	case 'g':
		global.startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		global.go = !global.go;
		break;
	case '=':
		if (global.numTessellation < MAX_TESSELLATION)
			global.numTessellation *= 2;
		break;
	case '-':
		if (global.numTessellation > 4)
			global.numTessellation /= 2;
		break;
	case 'f':
		if (global.angleIsland < 180)
			global.angleIsland += 1.0;
		break;
	case 'h':
		if (global.angleIsland > 0)
			global.angleIsland -= 1.0;
		break;
	case SPACEBAR: // island fire
		if (glutGet(GLUT_ELAPSED_TIME) / MILLI - fireTimeIsland > 1.0) {

			// initialize the r0 and v0 of this cannon ball from island
			if (!ball[ISLAND][global.ballNumIsland].fire) {
				ball[ISLAND][global.ballNumIsland].r0 = ball[ISLAND][global.ballNumIsland].r = { INITIAL_R0_ISLAND };
				ball[ISLAND][global.ballNumIsland].v0 = ball[ISLAND][global.ballNumIsland].v = { INITIAL_V0_ISLAND };
				ball[ISLAND][global.ballNumIsland].startTime = glutGet(GLUT_ELAPSED_TIME) / MILLI - global.startTime;
				ball[ISLAND][global.ballNumIsland].fire = true;
				ball[ISLAND][global.ballNumIsland].disappear = false;
				ball[ISLAND][global.ballNumIsland].isDefence = false;
				ball[ISLAND][global.ballNumIsland].radius = 0.0;
				ball[ISLAND][global.ballNumIsland].hitRed = false;
				ball[ISLAND][global.ballNumIsland].hitBlue = false;
				ball[ISLAND][global.ballNumIsland].hitIsland = false;
				ball[ISLAND][global.ballNumIsland].bloodChanged = false;
				if (global.ballNumIsland < MAX_BALL_NUM - 1)
					global.ballNumIsland++;
				else
					global.ballNumIsland = 0;
			}

			fireTimeIsland = glutGet(GLUT_ELAPSED_TIME) / MILLI;
		}
		
		break;
	case ESC:
		exit(EXIT_SUCCESS);
		break;
	default:
		break;
	}
	glutPostRedisplay();
}

/* deal with the special keys */
void specialKey(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_F1:
		global.wireframeMode = !global.wireframeMode;
		break;
	default:
		break;
	}
	glutPostRedisplay();
}

void init() {
	glEnable(GL_BLEND); //Enable blending.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //Set blending function.
}

void myReshape(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Assignment 1");
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(specialKey);
	glutReshapeFunc(myReshape);
	glutDisplayFunc(display);
	glutIdleFunc(update);

	init();

	glutMainLoop();

	return EXIT_SUCCESS;
}