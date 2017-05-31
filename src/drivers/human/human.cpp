/***************************************************************************

    created              : Sat Mar 18 23:16:38 CET 2000
    copyright            : (C) 2000 by Eric Espie
    email                : torcs@free.fr
    version              : $Id: human.cpp 5522 2013-06-17 21:03:25Z torcs-ng $

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/** @file

    @author	<a href=mailto:torcs@free.fr>Eric Espie</a>
    @version	$Id: human.cpp 5522 2013-06-17 21:03:25Z torcs-ng $
*/

/* 2013/3/21 Tom Low-Shang
 *
 * Moved original contents of
 *
 * drivers/human/human.cpp,
 * drivers/human/human.h,
 * drivers/human/pref.cpp,
 * drivers/human/pref.h,
 *
 * to libs/robottools/rthumandriver.cpp.
 *
 * CMD_* defines from pref.h are in interfaces/playerpref.h.
 *
 * Robot interface entry points are still here.
 */

#include <humandriver.h>

#include <iostream>
#include <thread>
#include <pistache/endpoint.h>

using namespace Net;

#include <gpsSensor.h>

static HumanDriver robot("human");

static void initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s);
static void drive_mt(int index, tCarElt* car, tSituation *s);
static void drive_at(int index, tCarElt* car, tSituation *s);
static void newrace(int index, tCarElt* car, tSituation *s);
static void resumerace(int index, tCarElt* car, tSituation *s);
static int  pitcmd(int index, tCarElt* car, tSituation *s);

static GPSSensor gps = GPSSensor();

static std::thread test;
static tCarElt* mycar;
static tSituation* mysituation;

class HelloHandler : public Http::Handler {
public:

    HTTP_PROTOTYPE(HelloHandler)

    void onRequest(const Http::Request& request, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, std::string("{") +
          std::string("\"speed\":\"") + std::to_string(mycar->_speed_x) + std::string("\",") +
          std::string("\"name\":\"") + std::string(mycar->_name) + std::string("\",") +
          std::string("\"rpm\":\"") + std::to_string(mycar->_enginerpm) + std::string("\",") +
          std::string("\"gear\":\"") + std::to_string(mycar->_gear) + std::string("\",") +
          std::string("\"gearNb\":\"") + std::to_string(mycar->_gearNb) + std::string("\",") +
          std::string("\"pos\":\"") + std::to_string(mycar->_pos) + std::string("\",") +
          std::string("\"ncars\":\"") + std::to_string(mysituation->_ncars) + std::string("\"") +
        std::string("}"));
    }
};

#ifdef _WIN32
/* Must be present under MS Windows */
BOOL WINAPI DllEntryPoint (HINSTANCE hDLL, DWORD dwReason, LPVOID Reserved)
{
    return TRUE;
}
#endif


static void
shutdown(const int index)
{
    robot.shutdown(index);
}//shutdown

static void
api()
{
  Net::Address addr(Net::Ipv4::any(), Net::Port(9080));
  auto opts = Net::Http::Endpoint::options()
  .threads(1);

  Http::Endpoint server(addr);
  server.init(opts);
  server.setHandler(Http::make_handler<HelloHandler>());
  server.serve();

  server.shutdown();
}


/**
 *
 *	InitFuncPt
 *
 *	Robot functions initialisation.
 *
 *	@param pt	pointer on functions structure
 *  @return 0
 */
static int
InitFuncPt(int index, void *pt)
{
	tRobotItf *itf = (tRobotItf *)pt;

    robot.init_context(index);

	itf->rbNewTrack = initTrack;	/* give the robot the track view called */
	/* for every track change or new race */
	itf->rbNewRace  = newrace;
	itf->rbResumeRace  = resumerace;

	/* drive during race */
	itf->rbDrive = robot.uses_at(index) ? drive_at : drive_mt;
	itf->rbShutdown = shutdown;
	itf->rbPitCmd   = pitcmd;
	itf->index      = index;

  test = std::thread(api);

	return 0;
}//InitFuncPt


/**
 *
 * moduleWelcome
 *
 * First function of the module called at load time :
 *  - the caller gives the module some information about its run-time environment
 *  - the module gives the caller some information about what he needs
 * MUST be called before moduleInitialize()
 *
 * @param	welcomeIn Run-time info given by the module loader at load time
 * @param welcomeOut Module run-time information returned to the called
 * @return 0 if no error occured, not 0 otherwise
 */
extern "C" int
moduleWelcome(const tModWelcomeIn* welcomeIn, tModWelcomeOut* welcomeOut)
{
	welcomeOut->maxNbItf = robot.count_drivers();

	return 0;
}//moduleWelcome


/**
 *
 * moduleInitialize
 *
 * Module entry point
 *
 * @param modInfo	administrative info on module
 * @return 0 if no error occured, -1 if any error occured
 */
extern "C" int
moduleInitialize(tModInfo *modInfo)
{
    return robot.initialize(modInfo, InitFuncPt);
}//moduleInitialize


/**
 * moduleTerminate
 *
 * Module exit point
 *
 * @return 0
 */
extern "C" int
moduleTerminate()
{
        robot.terminate();

	return 0;
}//moduleTerminate


/**
 * initTrack
 *
 * Search under robots/human/cars/<carname>/<trackname>.xml
 *
 * @param index
 * @param track
 * @param carHandle
 * @param carParmHandle
 * @param s situation provided by the sim
 *
 */
static void
initTrack(int index, tTrack* track, void *carHandle, void **carParmHandle, tSituation *s)
{
    robot.init_track(index, track, carHandle, carParmHandle, s);
}//initTrack


/**
 *
 * newrace
 *
 * @param index
 * @param car
 * @param s situation provided by the sim
 *
 */
void
newrace(int index, tCarElt* car, tSituation *s)
{
    robot.new_race(index, car, s);
}//newrace


void
resumerace(int index, tCarElt* car, tSituation *s)
{
    robot.resume_race(index, car, s);
}

/*
 * Function
 *
 *
 * Description
 *
 *
 * Parameters
 *
 *
 * Return
 *
 *
 * Remarks
 *
 */
static void
drive_mt(int index, tCarElt* car, tSituation *s)
{
    mycar = car;
    mysituation = s;
    robot.drive_mt(index, car, s);
}//drive_mt


/*
 * Function
 *
 *
 * Description
 *
 *
 * Parameters
 *
 *
 * Return
 *
 *
 * Remarks
 *
 */
static void
drive_at(int index, tCarElt* car, tSituation *s)
{
    /*gps.update(car);
    vec2 myPos = gps.getPosition();
    printf("Players's position according to GPS is (%f, %f)\n", myPos.x, myPos.y);*/
    mycar = car;
    mysituation = s;

    robot.drive_at(index, car, s);
}//drive_at


static int
pitcmd(int index, tCarElt* car, tSituation *s)
{
    return robot.pit_cmd(index, car, s);
}//pitcmd
