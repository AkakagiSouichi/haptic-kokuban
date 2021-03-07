#include "pch.h"

#include <stdio.h>
#include <Windows.h>
#include <GL/GL.h>
#include <GL/glut.h>

#include <HD/hd.h>
#include <HDU/hduError.h>
#include <HDU/hduMatrix.h>
#include <math.h>

#include <thread>
#include <mutex>

#define WIN32
#include "network.cpp"

#pragma comment( lib, "ws2_32.lib" )

int sock = udp_open((char*)"192.168.11.42", 11110, 11111);//ソケット作成
double size;//buf書き込み用変数
double xyz[3];//buf
double gForce2[3];

std::mutex _mtx;

#define MAXPOINTS 300000     /* 記憶する点の数　　 */
#define MAXPOINTS2 300000     /* 記憶する点の数　　 */
GLdouble point2[MAXPOINTS][3]; /* 座標を記憶する配列 */
GLdouble point[MAXPOINTS][3]; /* 座標を記憶する配列 */
GLdouble stylePoint2[3];
GLdouble GodPoint2[3];

int pointnum = 0;          /* 記憶した座標の数　 */
int pointnum2 = 0;          /* 記憶した座標の数　 */
int currentButtons; //ボタン押しているなら1,押していないなら0



// --- グローバル変数 ---
hduVector3Dd gCenterOfStylusSphere, gCenterOfGodSphere, gForce;
HHD ghHD = HD_INVALID_HANDLE;
HDSchedulerHandle gSchedulerCallback = HD_INVALID_HANDLE;

void myCylinder(double radius, double height, int sides)
{
	double step = (3.14 * 2.0) / (double)sides;
	int i;

	/* 荳企擇 */
	glNormal3d(0.0, 1.0, 0.0);
	glBegin(GL_TRIANGLE_FAN);
	for (i = 0; i < sides; i++) {
		double t = step * (double)i;
		glVertex3d(radius * sin(t), height, radius * cos(t));
	}
	glEnd();

	/* 蠎暮擇 */
	glNormal3d(0.0, -1.0, 0.0);
	glBegin(GL_TRIANGLE_FAN);
	for (i = sides; --i >= 0;) {
		double t = step * (double)i;
		glVertex3d(radius * sin(t), 0.0, radius * cos(t));
	}
	glEnd();

	/* 蛛ｴ髱｢ */
	glBegin(GL_QUAD_STRIP);
	for (i = 0; i <= sides; i++) {
		double t = step * (double)i;
		double x = sin(t);
		double z = cos(t);

		glNormal3d(x, 0.0, z);
		glVertex3f(radius * x, height, radius * z);
		glVertex3f(radius * x, 0.0, radius * z);
	}
	glEnd();
}

void udp_read_write() {
	while (1) {
		_mtx.lock();
		size = udp_read(sock, (char*)xyz, sizeof(double) * 3);
		//std::cout << ":" << xyz[0] << "," << xyz[1] << "," << xyz[2] << std::endl;
		_mtx.unlock();

		_mtx.lock();
		size = udp_write(sock, (char*)gForce2, sizeof(double) * 3);
		_mtx.unlock();
	}
}

// --- 触覚の空間をグラフィックスの空間と同じにする ---
void resize(int w, int h)
{
	glMatrixMode(GL_PROJECTION); // 投影投影モードへ
	glLoadIdentity(); // 投影変換の変換行列を単位行列で初期化
	glOrtho(-100.0, 100.0, -100.0, 100.0, -100.0, 100.0); //各軸指定した範囲で囲まれる立方体の範囲を平行投影
}
// --- グラフィックスパイプライン，光源の初期化 ---
void doGraphicsState()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // カラーバッファとデプスバッファをクリア
	glEnable(GL_COLOR_MATERIAL); // 物体色の有効化
	glEnable(GL_LIGHTING); // ライティング（照光）処理を有効化
	glEnable(GL_NORMALIZE); // 法線ベクトルを正規化（単位ベクトル化）
	glShadeModel(GL_SMOOTH); // スムースシェーディングを有効化
	GLfloat lightZeroPosition[] = { 10.0f, 4.0f, 100.0f, 0.0f }; // 光源０の位置
	GLfloat lightZeroColor[] = { 0.6f, 0.6f, 0.6f, 1.0f }; // 光源０の色
	GLfloat lightOnePosition[] = { -1.0f, -2.0f, -100.0f, 0.0f }; // 光源１の位置
	GLfloat lightOneColor[] = { 0.6f, 0.6f, 0.6f, 1.0f }; // 光源１の色
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE); // より正確な陰影付けを行う
	glLightfv(GL_LIGHT0, GL_POSITION, lightZeroPosition);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
	//glLightfv(GL_LIGHT1, GL_POSITION, lightOnePosition);
	//glLightfv(GL_LIGHT1, GL_DIFFUSE, lightOneColor);
	glEnable(GL_LIGHT0); // 光源０の有効化
	//glEnable(GL_LIGHT1); // 光源１の有効化
	glEnable(GL_DEPTH_TEST);
}
// --- GLUTライブラリから定期的に呼ばれる関数 ---
void idle(void)
{
	glutPostRedisplay(); // グラフィックスの再描画
	if (!hdWaitForCompletion(gSchedulerCallback, HD_WAIT_CHECK_STATUS)) {
		printf("メインスケジューラが終了しました。\n何かキーを押すと終了します。\n");
		getchar();
		exit(-1);
	}
}
#define SSR 15.0
#define FSR 60.0
#define OBJECT_MASS 30.0
#define STIFFNESS 0.10
#define EPSILON 0.00001
// --- GodObjectの座標を更新 ---
void updateEffectorPosition(void)
{
	double m_currentDistance;
	hduVector3Dd m_centerToEffector = gCenterOfStylusSphere;
	m_currentDistance = m_centerToEffector.magnitude();

	stylePoint2[0] = xyz[0];
	stylePoint2[1] = xyz[1];
	stylePoint2[2] = xyz[2];


	/*if (m_currentDistance > SSR + FSR) {
		gCenterOfGodSphere = gCenterOfStylusSphere;
		gForce.set(0.0, 0.0, 0.0);
	}
	else {
		if (m_currentDistance > EPSILON) {
			double scale = (SSR + FSR) / m_currentDistance;
			gCenterOfGodSphere = scale * m_centerToEffector;
			gForce = STIFFNESS * (gCenterOfGodSphere - gCenterOfStylusSphere);

		}
	}*/

	//1
	if (gCenterOfStylusSphere[2] < 0) {
		point[pointnum][0] = gCenterOfStylusSphere[0];
		point[pointnum][1] = gCenterOfStylusSphere[1];
		if (point[pointnum][2] < 0) {
			point[pointnum][2] = gCenterOfStylusSphere[2];
		}
		else {
			gCenterOfGodSphere = gCenterOfStylusSphere;
			point[pointnum][2] = 0;
		}

		//printf("%f\t, %f\t, %f\t\n", point[pointnum][0], point[pointnum][1], point[pointnum][2]);
		//printf("%f\t, %f\t, %f\t\n", stylePoint2[0], stylePoint2[1], stylePoint2[2]);
		gForce[2] = STIFFNESS * (0 - gCenterOfStylusSphere[2]);
		gCenterOfGodSphere[0] = gCenterOfStylusSphere[0];
		gCenterOfGodSphere[1] = gCenterOfStylusSphere[1];
		if (pointnum < MAXPOINTS - 1) ++pointnum;
	}
	else {
		gCenterOfGodSphere = gCenterOfStylusSphere;
		gForce.set(0.0, 0.0, 0.0);
	}

	//2
	if (stylePoint2[2] < 0) {
		point2[pointnum2][0] = stylePoint2[0];
		point2[pointnum2][1] = stylePoint2[1];
		if (point2[pointnum2][2] < 0) {
			point2[pointnum2][2] = stylePoint2[2];
		}
		else {
			GodPoint2[0] = stylePoint2[0];
			GodPoint2[1] = stylePoint2[1];
			GodPoint2[2] = stylePoint2[2];
			point2[pointnum][2] = stylePoint2[2];
		}

		//printf("%f\t, %f\t, %f\t\n", point[pointnum][0], point[pointnum][1], point[pointnum][2]);
		//printf("%f\t, %f\t, %f\t\n", stylePoint2[0], stylePoint2[1], stylePoint2[2]);
		gForce2[2] = STIFFNESS * (0 - stylePoint2[2]);
		GodPoint2[0] = stylePoint2[0];
		GodPoint2[1] = stylePoint2[1];
		if (pointnum2 < MAXPOINTS2 - 1) ++pointnum2;
	}
	else {
		GodPoint2[0] = stylePoint2[0];
		GodPoint2[1] = stylePoint2[1];
		GodPoint2[2] = stylePoint2[2];
		gForce2[0] = 0;
		gForce2[1] = 0;
		gForce2[2] = 0;
	}
}

// --- 力覚スケジューラ ---
HDCallbackCode HDCALLBACK ContactCB(void* data)
{
	HHD hHD = hdGetCurrentDevice();
	hdBeginFrame(hHD);
	hdGetDoublev(HD_CURRENT_POSITION, gCenterOfStylusSphere);
	updateEffectorPosition();
	hdSetDoublev(HD_CURRENT_FORCE, gForce);
	hdGetIntegerv(HD_CURRENT_BUTTONS, &currentButtons);
	hdEndFrame(hHD);
	HDErrorInfo error;
	if (HD_DEVICE_ERROR(error = hdGetError())) {
		hduPrintError(stderr, &error, "力覚スケジューラ内でエラーが発生しました。");
		if (hduIsSchedulerError(&error)) {
			return HD_CALLBACK_DONE;
		}
	}
	return HD_CALLBACK_CONTINUE;
}

// グラフィックス（球体）の表示
void display()
{
	int i;


	glPointSize(5);

	doGraphicsState();
	glDisable(GL_CULL_FACE); // カリングの無効化

	glPushMatrix();
	/* 記録したデータで線を描く */
	if (pointnum > 1) {
		glColor4d(1.0, 1.0, 1.0, 1.0);
		glBegin(GL_POINTS);
		for (i = 0; i < pointnum; ++i) {
			glVertex3d(point[i][0], point[i][1], point[i][2]);
		}
		glEnd();
	}
	glPopMatrix();

	glPushMatrix();
	/* 記録したデータで線を描く */
	if (pointnum2 > 1) {
		glColor4d(1.0, 0.0, 0.0, 1.0);
		glBegin(GL_POINTS);
		for (i = 0; i < pointnum2; ++i) {
			glVertex3d(point2[i][0], point2[i][1], point2[i][2]);
		}
		glEnd();
	}
	glPopMatrix();

	glPushMatrix();
	glTranslated(0.0, 0.0, 0);
	glColor4d(0.0, 0.8, 0.0, 1.0);
	glBegin(GL_POLYGON);
	glVertex2d(-100, -100);
	glVertex2d(100, -100);
	glVertex2d(100, 100);
	glVertex2d(-100, 100);
	glEnd();
	glPopMatrix();

	glPushMatrix();
	glTranslated(gCenterOfGodSphere[0], gCenterOfGodSphere[1], gCenterOfGodSphere[2]);
	glColor4d(1, 1, 1, 1.0);
	//glutSolidSphere(SSR, 20, 20); // 物体表面にとどまる球(GodObject)
	myCylinder(2.5, 20, 5);
	glPopMatrix();

	/* The device-controlled sphere.
	glPushMatrix();
	glTranslated(gCenterOfStylusSphere[0], gCenterOfStylusSphere[1], gCenterOfStylusSphere[2]);
	glColor4d(1.0, 1.0, 0.0, 1.0);
	glutWireSphere(SSR, 20, 20); // 物体内部に侵入する球
	glPopMatrix();*/

	glPushMatrix();
	glTranslated(GodPoint2[0], GodPoint2[1], GodPoint2[2]);
	glColor4d(1, 0, 0, 1.0);
	//glutSolidSphere(SSR, 20, 20); // 物体表面にとどまる球(GodObject)
	myCylinder(2.5, 20, 5);
	glPopMatrix();

	glutSwapBuffers();
}
// --- 終了処理 ---
void exitHandler()
{
	hdStopScheduler();
	if (ghHD != HD_INVALID_HANDLE) {
		hdDisableDevice(ghHD);
		ghHD = HD_INVALID_HANDLE;
	}
	printf("終了。\n");
}
// --- キーボード入力時のコールバック関数 ----
void keyboard(unsigned char key, int x, int y)
{
	if (key == 'q') exit(0); // 'q'キーが押されたらプログラムを終了
}
// --- メイン関数 ---
int main(int argc, char* argv[])
{

	HDErrorInfo error;
	printf("起動します\n");
	std::thread th1(udp_read_write);
	atexit(exitHandler);
	ghHD = hdInitDevice(HD_DEFAULT_DEVICE);
	hdEnable(HD_FORCE_OUTPUT);
	hdEnable(HD_MAX_FORCE_CLAMPING);
	hdStartScheduler();
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
	glutInitWindowSize(500, 500);
	glutCreateWindow("hellohaptics");
	glutDisplayFunc(display);
	glutReshapeFunc(resize);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboard);
	gSchedulerCallback = hdScheduleAsynchronous(ContactCB, NULL, HD_DEFAULT_SCHEDULER_PRIORITY);
	glutMainLoop(); // GLUTのメインループ
	return 0;
}