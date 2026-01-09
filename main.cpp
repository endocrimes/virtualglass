
#include <QCoreApplication>
#include <QDir>
#include <Qt>
#include <QDateTime>
#include <QStyleHints>
#include <QSurfaceFormat>
#include <random>
#include "vgapp.h"
#include "randomutil.h"

// Global random engine for Qt 6 compatibility (replaces qsrand/qrand)
static std::mt19937 g_randomEngine;

void vg_srand(uint64_t seed)
{
	g_randomEngine.seed(seed);
}

int vg_rand()
{
	return std::uniform_int_distribution<int>(0, RAND_MAX)(g_randomEngine);
}

int main(int argc, char** argv)
{
	// Set default OpenGL format before QApplication creation (required for Qt 6)
	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setSamples(4);
	format.setProfile(QSurfaceFormat::CompatibilityProfile);
	QSurfaceFormat::setDefaultFormat(format);

	// Must come before QApp creation
	// fix Mac OS X 10.9+ font issues
	// https://bugreports.qt-project.org/browse/QTBUG-32789
	// https://bugreports.qt.io/browse/QTBUG-47206
	#ifdef Q_OS_MACOS
	QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande"); // 10.9
	QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Helvetica Neue"); // 10.9
	QFont::insertSubstitution(".SF NS Text", "Helvetica Neue"); // 10.11
	#endif

	VGApp* app;

	app = new VGApp(argc, argv);

	// drag distance used in drag-n-drop of canes, colors
	QGuiApplication::styleHints()->setStartDragDistance(3);
	// make a decent seed
	vg_srand(QDateTime::currentDateTime().toSecsSinceEpoch());

	QDir pluginsDir = QDir(app->applicationDirPath());
	#ifdef Q_OS_WIN
	if (pluginsDir.dirName().toLower() == "debug" || pluginsDir.dirName().toLower() == "release")
		pluginsDir.cdUp();
	#endif
	#ifdef Q_OS_MACOS
	if (pluginsDir.dirName() == "MacOS")
	{
		pluginsDir.cdUp();
		pluginsDir.cdUp();
		pluginsDir.cdUp();
	}
	#endif
	return app->exec();
}


