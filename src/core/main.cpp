/*
 * main.cpp - just main.cpp which is starting up app...
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2012-2013 Paul Giblock    <p/at/pgiblock.net>
 *
 * This file is part of LMMS - http://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "lmmsconfig.h"
#include "lmmsversion.h"
#include "versioninfo.h"

#include "denormals.h"

#include <QFileInfo>
#include <QLocale>
#include <QDate>
#include <QTimer>
#include <QTranslator>
#include <QApplication>
#include <QMessageBox>
#include <QTextStream>

#ifdef LMMS_BUILD_WIN32
#include <windows.h>
#endif

#ifdef LMMS_HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef LMMS_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef LMMS_HAVE_PROCESS_H
#include <process.h>
#endif

#ifdef LMMS_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "MemoryManager.h"
#include "ConfigManager.h"
#include "NotePlayHandle.h"
#include "Engine.h"
#include "GuiApplication.h"
#include "ImportFilter.h"
#include "MainWindow.h"
#include "ProjectRenderer.h"
#include "DataFile.h"
#include "Song.h"

static inline QString baseName( const QString & file )
{
	return QFileInfo( file ).absolutePath() + "/" +
			QFileInfo( file ).completeBaseName();
}




static std::string getCurrentYear()
{
	return QString::number( QDate::currentDate().year() ).toStdString();
}




inline void loadTranslation( const QString & tname,
	const QString & dir = ConfigManager::inst()->localeDir() )
{
	QTranslator * t = new QTranslator( QCoreApplication::instance() );
	QString name = tname + ".qm";

	t->load( name, dir );

	QCoreApplication::instance()->installTranslator( t );
}




void printVersion( char *executableName )
{
	printf( "LMMS %s\n(%s %s, Qt %s, %s)\n\n"
		"Copyright (c) 2004-%s LMMS developers.\n\n"
		"This program is free software; you can redistribute it and/or\n"
		"modify it under the terms of the GNU General Public\n"
		"License as published by the Free Software Foundation; either\n"
		"version 2 of the License, or (at your option) any later version.\n\n"
		"Try \"%s --help\" for more information.\n\n", LMMS_VERSION,
		PLATFORM, MACHINE, QT_VERSION_STR, GCC_VERSION,
		getCurrentYear().c_str(), executableName );
}




void printHelp()
{
	printf( "LMMS %s\n"
		"Copyright (c) 2004-%s LMMS developers.\n\n"
		"Usage: lmms [ -r <project file> ] [ options ]\n"
		"            [ -u <in> <out> ]\n"
		"            [ -d <in> ]\n"
		"            [ -h ]\n"
		"            [ <file to load> ]\n\n"
		"-r, --render <project file>	Render given project file\n"
		"-o, --output <file>		Render into <file>\n"
		"-f, --format <format>		Specify format of render-output where\n"
		"				Format is either 'wav' or 'ogg'.\n"
		"-s, --samplerate <samplerate>	Specify output samplerate in Hz\n"
		"				Range: 44100 (default) to 192000\n"
		"-b, --bitrate <bitrate>		Specify output bitrate in KBit/s\n"
		"				Default: 160.\n"
		"-i, --interpolation <method>	Specify interpolation method\n"
		"				Possible values:\n"
		"				   - linear\n"
		"				   - sincfastest (default)\n"
		"				   - sincmedium\n"
		"				   - sincbest\n"
		"-x, --oversampling <value>	Specify oversampling\n"
		"				Possible values: 1, 2, 4, 8\n"
		"				Default: 2\n"
		"-a, --float			32bit float bit depth\n"
		"-l, --loop			Render as a loop\n"
		"-u, --upgrade <in> [out]	Upgrade file <in> and save as <out>\n"
		"       Standard out is used if no output file is specifed\n"
		"-d, --dump <in>			Dump XML of compressed file <in>\n"
		"-v, --version			Show version information and exit.\n"
		"    --allowroot			Bypass root user startup check (use with caution).\n"
		"-h, --help			Show this usage information and exit.\n\n",
		LMMS_VERSION, getCurrentYear().c_str() );
}




int main( int argc, char * * argv )
{
	// initialize memory managers
	MemoryManager::init();
	NotePlayHandleManager::init();

	// intialize RNG
	srand( getpid() + time( 0 ) );

	disable_denormals();

	bool coreOnly = false;
	bool fullscreen = true;
	bool exitAfterImport = false;
	bool allowRoot = false;
	bool renderLoop = false;
	QString fileToLoad, fileToImport, renderOut, profilerOutputFile;

	// first of two command-line parsing stages
	for( int i = 1; i < argc; ++i )
	{
		QString arg = argv[i];

		if( arg == "--help"    || arg == "-h" ||
		    arg == "--version" || arg == "-v" ||
		    arg == "--render"  || arg == "-r" )
		{
			coreOnly = true;
		}
		else if( arg == "--allowroot" )
		{
			allowRoot = true;
		}
		else if( arg == "-geometry" )
		{
			// option -geometry is filtered by Qt later,
			// so we need to check its presence now to
			// determine, if the application should run in
			// fullscreen mode (default, no -geometry given).
			fullscreen = false;
		}
	}

#ifndef LMMS_BUILD_WIN32
	if ( ( getuid() == 0 || geteuid() == 0 ) && !allowRoot )
	{
		printf( "LMMS cannot be run as root.\nUse \"--allowroot\" to override.\n\n" );
		return EXIT_FAILURE;
	}	
#endif

	QCoreApplication * app = coreOnly ?
			new QCoreApplication( argc, argv ) :
					new QApplication( argc, argv ) ;

	Mixer::qualitySettings qs( Mixer::qualitySettings::Mode_HighQuality );
	ProjectRenderer::OutputSettings os( 44100, false, 160,
						ProjectRenderer::Depth_16Bit );
	ProjectRenderer::ExportFileFormats eff = ProjectRenderer::WaveFile;

	// second of two command-line parsing stages
	for( int i = 1; i < argc; ++i )
	{
		QString arg = argv[i];

		if( arg == "--version" || arg == "-v" )
		{
			printVersion( argv[0] );
			return EXIT_SUCCESS;
		}
		else if( arg == "--help" || arg  == "-h" )
		{
			printHelp();
			return EXIT_SUCCESS;
		}
		else if( arg == "--upgrade" || arg == "-u" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo input file specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			DataFile dataFile( QString::fromLocal8Bit( argv[i] ) );

			if( argc > i+1 ) // output file specified
			{
				dataFile.writeFile( QString::fromLocal8Bit( argv[i+1] ) );
			}
			else // no output file specified; use stdout
			{
				QTextStream ts( stdout );
				dataFile.write( ts );
				fflush( stdout );
			}

			return EXIT_SUCCESS;
		}
		else if( arg == "--allowroot" )
		{
			// Ignore, processed earlier
#ifdef LMMS_BUILD_WIN32
			if( allowRoot )
			{
				printf( "\nOption \"--allowroot\" will be ignored on this platform.\n\n" );
			}
#endif
			
		}
		else if( arg == "--dump" || arg == "-d" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo input file specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			QFile f( QString::fromLocal8Bit( argv[i] ) );
			f.open( QIODevice::ReadOnly );
			QString d = qUncompress( f.readAll() );
			printf( "%s\n", d.toUtf8().constData() );

			return EXIT_SUCCESS;
		}
		else if( arg == "--render" || arg == "-r" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo input file specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			fileToLoad = QString::fromLocal8Bit( argv[i] );
			renderOut = baseName( fileToLoad ) + ".";
		}
		else if( arg == "--loop" || arg == "-l" )
		{
			renderLoop = true;
		}
		else if( arg == "--output" || arg == "-o" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo output file specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			renderOut = baseName( QString::fromLocal8Bit( argv[i] ) ) + ".";
		}
		else if( arg == "--format" || arg == "-f" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo output format specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			const QString ext = QString( argv[i] );

			if( ext == "wav" )
			{
				eff = ProjectRenderer::WaveFile;
			}
#ifdef LMMS_HAVE_OGGVORBIS
			else if( ext == "ogg" )
			{
				eff = ProjectRenderer::OggFile;
			}
#endif
			else
			{
				printf( "\nInvalid output format %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
		}
		else if( arg == "--samplerate" || arg == "-s" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo samplerate specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			sample_rate_t sr = QString( argv[i] ).toUInt();
			if( sr >= 44100 && sr <= 192000 )
			{
				os.samplerate = sr;
			}
			else
			{
				printf( "\nInvalid samplerate %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
		}
		else if( arg == "--bitrate" || arg == "-b" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo bitrate specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			int br = QString( argv[i] ).toUInt();

			if( br >= 64 && br <= 384 )
			{
				os.bitrate = br;
			}
			else
			{
				printf( "\nInvalid bitrate %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
		}
		else if( arg =="--float" || arg == "-a" )
		{
			os.depth = ProjectRenderer::Depth_32Bit;
		}
		else if( arg == "--interpolation" || arg == "-i" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo interpolation method specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			const QString ip = QString( argv[i] );

			if( ip == "linear" )
			{
		qs.interpolation = Mixer::qualitySettings::Interpolation_Linear;
			}
			else if( ip == "sincfastest" )
			{
		qs.interpolation = Mixer::qualitySettings::Interpolation_SincFastest;
			}
			else if( ip == "sincmedium" )
			{
		qs.interpolation = Mixer::qualitySettings::Interpolation_SincMedium;
			}
			else if( ip == "sincbest" )
			{
		qs.interpolation = Mixer::qualitySettings::Interpolation_SincBest;
			}
			else
			{
				printf( "\nInvalid interpolation method %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
		}
		else if( arg == "--oversampling" || arg == "-x" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo oversampling specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			int o = QString( argv[i] ).toUInt();

			switch( o )
			{
				case 1:
		qs.oversampling = Mixer::qualitySettings::Oversampling_None;
		break;
				case 2:
		qs.oversampling = Mixer::qualitySettings::Oversampling_2x;
		break;
				case 4:
		qs.oversampling = Mixer::qualitySettings::Oversampling_4x;
		break;
				case 8:
		qs.oversampling = Mixer::qualitySettings::Oversampling_8x;
		break;
				default:
				printf( "\nInvalid oversampling %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
		}
		else if( arg == "--import" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo file specified for importing.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			fileToImport = QString::fromLocal8Bit( argv[i] );

			// exit after import? (only for debugging)
			if( QString( argv[i + 1] ) == "-e" )
			{
				exitAfterImport = true;
				++i;
			}
		}
		else if( arg == "--profile" || arg == "-p" )
		{
			++i;

			if( i == argc )
			{
				printf( "\nNo profile specified.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[0] );
				return EXIT_FAILURE;
			}


			profilerOutputFile = QString::fromLocal8Bit( argv[1] );
		}
		else
		{
			if( argv[i][0] == '-' )
			{
				printf( "\nInvalid option %s.\n\n"
	"Try \"%s --help\" for more information.\n\n", argv[i], argv[0] );
				return EXIT_FAILURE;
			}
			fileToLoad = QString::fromLocal8Bit( argv[i] );
		}
	}


	ConfigManager::inst()->loadConfigFile();

	// set language
	QString pos = ConfigManager::inst()->value( "app", "language" );
	if( pos.isEmpty() )
	{
		pos = QLocale::system().name().left( 2 );
	}

#ifdef LMMS_BUILD_WIN32
#undef QT_TRANSLATIONS_DIR
#define QT_TRANSLATIONS_DIR ConfigManager::inst()->localeDir()
#endif

#ifdef QT_TRANSLATIONS_DIR
	// load translation for Qt-widgets/-dialogs
	loadTranslation( QString( "qt_" ) + pos,
					QString( QT_TRANSLATIONS_DIR ) );
#endif
	// load actual translation for LMMS
	loadTranslation( pos );


	// try to set realtime priority
#ifdef LMMS_BUILD_LINUX
#ifdef LMMS_HAVE_SCHED_H
#ifndef __OpenBSD__
	struct sched_param sparam;
	sparam.sched_priority = ( sched_get_priority_max( SCHED_FIFO ) +
				sched_get_priority_min( SCHED_FIFO ) ) / 2;
	if( sched_setscheduler( 0, SCHED_FIFO, &sparam ) == -1 )
	{
		printf( "Notice: could not set realtime priority.\n" );
	}
#endif
#endif
#endif

#ifdef LMMS_BUILD_WIN32
	if( !SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS ) )
	{
		printf( "Notice: could not set high priority.\n" );
	}
#endif

	// if we have an output file for rendering, just render the song
	// without starting the GUI
	if( !renderOut.isEmpty() )
	{
		Engine::init( true );

		QFileInfo fileInfo( fileToLoad );
		if ( !fileInfo.exists() )
		{
			printf("The file %s does not exist!\n", fileToLoad.toStdString().c_str());
			exit( 1 );
		}

		printf( "Loading project...\n" );
		Engine::getSong()->loadProject( fileToLoad );
		printf( "Done\n" );

		Engine::getSong()->setExportLoop( renderLoop );

		// create renderer
		QString extension = ( eff == ProjectRenderer::WaveFile ) ? "wav" : "ogg";
		ProjectRenderer * r = new ProjectRenderer( qs, os, eff, renderOut + extension );
		QCoreApplication::instance()->connect( r,
				SIGNAL( finished() ), SLOT( quit() ) );

		// timer for progress-updates
		QTimer * t = new QTimer( r );
		r->connect( t, SIGNAL( timeout() ),
				SLOT( updateConsoleProgress() ) );
		t->start( 200 );

		if( profilerOutputFile.isEmpty() == false )
		{
			Engine::mixer()->profiler().setOutputFile( profilerOutputFile );
		}

		// start now!
		r->startProcessing();
	}
	else // otherwise, start the GUI
	{
		new GuiApplication();

		// re-intialize RNG - shared libraries might have srand() or
		// srandom() calls in their init procedure
		srand( getpid() + time( 0 ) );

		// recover a file?
		QString recoveryFile = ConfigManager::inst()->recoveryFile();

		if( QFileInfo(recoveryFile).exists() &&
			QMessageBox::question( gui->mainWindow(), MainWindow::tr( "Project recovery" ),
						MainWindow::tr( "It looks like the last session did not end properly. "
										"Do you want to recover the project of this session?" ),
						QMessageBox::Yes | QMessageBox::No ) == QMessageBox::Yes )
		{
			fileToLoad = recoveryFile;
		}

		// we try to load given file
		if( !fileToLoad.isEmpty() )
		{
			gui->mainWindow()->show();
			if( fullscreen )
			{
				gui->mainWindow()->showMaximized();
			}

			if( fileToLoad == recoveryFile )
			{
				Engine::getSong()->createNewProjectFromTemplate( fileToLoad );
			}
			else
			{
				Engine::getSong()->loadProject( fileToLoad );
			}
		}
		else if( !fileToImport.isEmpty() )
		{
			ImportFilter::import( fileToImport, Engine::getSong() );
			if( exitAfterImport )
			{
				return EXIT_SUCCESS;
			}

			gui->mainWindow()->show();
			if( fullscreen )
			{
				gui->mainWindow()->showMaximized();
			}
		}
		else
		{
			Engine::getSong()->createNewProject();

			// [Settel] workaround: showMaximized() doesn't work with
			// FVWM2 unless the window is already visible -> show() first
			gui->mainWindow()->show();
			if( fullscreen )
			{
				gui->mainWindow()->showMaximized();
			}
		}
	}

	const int ret = app->exec();
	delete app;

	// cleanup memory managers
	MemoryManager::cleanup();

	return ret;
}
