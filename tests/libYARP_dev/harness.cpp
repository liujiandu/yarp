/*
 * Copyright (C) 2006 RobotCub Consortium
 * Authors: Paul Fitzpatrick
 * CopyPolicy: Released under the terms of the LGPLv2.1 or later, see LGPL.TXT
 *
 */


#include <yarp/os/ConstString.h>
#include <yarp/os/impl/Logger.h>
#include <yarp/os/impl/UnitTest.h>
#include <yarp/os/impl/Companion.h>
#include <yarp/os/NetInt32.h>
#include <yarp/os/NetInt32.h>
#include <yarp/os/Network.h>
#include <yarp/os/Os.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/Drivers.h>

#include "TestList.h"

#include "YarpBuildLocation.h"

#include <cstdio>

using namespace yarp::os::impl;
using namespace yarp::os;
using namespace yarp::dev;

using namespace std;

#ifdef YARP2_LINUX
#define CHECK_FOR_LEAKS
#endif

#ifdef CHECK_FOR_LEAKS
// this is just for memory leak checking, and only works in linux
#include <mcheck.h>
#endif


int harness_main(int argc, char *argv[]) {
#ifdef CHECK_FOR_LEAKS
    mtrace();
#endif

    Network yarp;

    bool done = false;
    int result = 0;

    if (argc>1) {
        int verbosity = 0;
        while (ConstString(argv[1])==ConstString("verbose")) {
            verbosity++;
            argc--;
            argv++;
        }
        if (verbosity>0) {
            Logger::get().setVerbosity(verbosity);
        }

        if (ConstString(argv[1])==ConstString("regression")) {
            done = true;

            // Start the testing system
            UnitTest::startTestSystem();
            TestList::collectTests();  // just in case automation doesn't work
            if (argc>2) {
                result = UnitTest::getRoot().run(argc-2,argv+2);
            } else {
                result = UnitTest::getRoot().run();
            }
            UnitTest::stopTestSystem();
        }
    }
    if (!done) {
        Companion::main(argc,argv);
    }

    return result;
}



static ConstString getFile(const char *fname) {
    char buf[25600];
    FILE *fin = fopen(fname,"r");
    if (fin==nullptr) return "";
    ConstString result = "";
    while(fgets(buf, sizeof(buf)-1, fin) != nullptr) {
        result += buf;
    }
    fclose(fin);
    fin = nullptr;
    return result;

    /*
    ifstream fin(fname);
    ConstString txt;
    if (fin.fail()) {
        return "";
    }
    while (!(fin.eof()||fin.fail())) {
        char buf[1000];
        fin.getline(buf,sizeof(buf));
        if (!fin.eof()) {
            txt += buf;
            txt += "\n";
        }
    }
    return txt;
    */
}


static void toDox(PolyDriver& dd, FILE *os) {
    fprintf(os, "<table>\n");
    fprintf(os, "<tr><td>PROPERTY</td><td>DESCRIPTION</td><td>DEFAULT</td></tr>\n");
    Bottle order = dd.getOptions();
    for (int i=0; i<order.size(); i++) {
        ConstString name = order.get(i).toString().c_str();
        if (name=="wrapped"||name.substr(0,10)=="subdevice.") {
            continue;
        }
        ConstString desc = dd.getComment(name.c_str());
        ConstString def = dd.getDefaultValue(name.c_str()).toString();
        ConstString out = "";
        out += "<tr><td>";
        out += name.c_str();
        out += "</td><td>";
        out += desc.c_str();
        out += "</td><td>";
        out += def.c_str();
        out += "</td></tr>";
        fprintf(os,"%s\n",out.c_str());
    }
    fprintf(os, "</table>\n");
}

int main(int argc, char *argv[]) {
    Property p;
    p.fromCommand(argc,argv);

    // To make sure that the dev test are able to find all the devices
    // compile by YARP, also the one compiled as dynamic plugins
    // we add the build directory to the YARP_DATA_DIRS enviromental variable
    // CMAKE_CURRENT_DIR is the define by the CMakeLists.txt tests file
    ConstString dirs = CMAKE_BINARY_DIR +
                       yarp::os::Network::getDirectorySeparator() +
                       "share" +
                       yarp::os::Network::getDirectorySeparator() +
                       "yarp";

    // Add TEST_DATA_DIR to YARP_DATA_DIRS in order to find the contexts used
    // by the tests
    dirs += yarp::os::Network::getPathSeparator() +
            TEST_DATA_DIR;

    // If set, append user YARP_DATA_DIRS
    // FIXME check if this can be removed
    Network::getEnvironment("YARP_DATA_DIRS");
    if (!Network::getEnvironment("YARP_DATA_DIRS").empty()) {
        dirs += yarp::os::Network::getPathSeparator() +
                Network::getEnvironment("YARP_DATA_DIRS");
    }

    Network::setEnvironment("YARP_DATA_DIRS", dirs);
    printf("YARP_DATA_DIRS=\"%s\"\n", Network::getEnvironment("YARP_DATA_DIRS").c_str());

    // check where to put description of device
    ConstString dest = "";
    dest = p.check("doc",Value("")).toString();

    ConstString fileName = p.check("file",Value("default.ini")).asString();

    if (p.check("file")) {
        p.fromConfigFile(fileName);
    }

    ConstString deviceName = p.check("device",Value("")).asString();

    // if no device given, we should be operating a completely
    // standard test harness like for libYARP_OS and libYARP_sig
    if (deviceName=="") {
        return harness_main(argc,argv);
    }

    // device name given - use special device testing procedure

#ifdef CHECK_FOR_LEAKS
    mtrace();
#endif

    int result = 0;

    Network::init();
    Network::setLocalMode(true);

    ConstString seek = fileName.c_str();
    ConstString exampleName = "";
    int pos = seek.rfind('/');
    if (pos==-1) {
        pos = seek.rfind('\\');
    }
    if (pos==-1) {
        pos = 0;
    } else {
        pos++;
    }
    int len = seek.find('.',pos);
    if (len==-1) {
        len = seek.length();
    } else {
        len -= pos;
    }
    exampleName = seek.substr(pos,len).c_str();
    ConstString shortFileName = seek.substr(pos,seek.length()).c_str();

    PolyDriver dd;
	YARP_DEBUG(Logger::get(), "harness opening...");

    bool ok = dd.open(p);
    YARP_DEBUG(Logger::get(), "harness opened.");
    result = ok?0:1;

    ConstString wrapperName = "";
    ConstString codeName = "";

    DriverCreator *creator =
        Drivers::factory().find(deviceName.c_str());
    if (creator!=nullptr) {
        wrapperName = creator->getWrapper();
        codeName = creator->getCode();
    }


    if (dest!="") {
        ConstString dest2 = dest.c_str();
        if (result!=0) {
            dest2 += ".fail";
        }
        FILE *fout = fopen(dest2.c_str(),"w");
        if (fout==nullptr) {
            printf("Problem writing to %s\n", dest2.c_str());
            std::exit(1);
        }
        fprintf(fout,"/**\n");
        fprintf(fout," * \\ingroup dev_examples\n");
        fprintf(fout," *\n");
        fprintf(fout," * \\defgroup %s Example for %s (%s)\n\n",
                exampleName.c_str(),
                deviceName.c_str(),
                exampleName.c_str());
        fprintf(fout, "Instantiates \\ref cmd_device_%s \"%s\" device implemented by %s.\n",
                deviceName.c_str(), deviceName.c_str(), codeName.c_str());
        fprintf(fout, "\\verbatim\n%s\\endverbatim\n",
                getFile(fileName.c_str()).c_str());
        fprintf(fout, "If this text is saved in a file called %s then the device can be created by doing:\n",
                shortFileName.c_str());
        fprintf(fout, "\\verbatim\nyarpdev --file %s\n\\endverbatim\n",
                shortFileName.c_str());
        fprintf(fout, "Of course, the configuration could be passed just as command line options, or as a yarp::os::Property object in a program:\n");
        fprintf(fout, "\\code\n");
        fprintf(fout, "Property p;\n");
        fprintf(fout, "p.fromConfigFile(\"%s\");\n",
                shortFileName.c_str());
        fprintf(fout, "// of course you could construct the Property object on-the-fly\n");
        fprintf(fout, "PolyDriver dev;\n");
        fprintf(fout, "dev.open(p);\n");
        fprintf(fout, "if (dev.isValid()) { /* use the device via view method */ }\n" );
        fprintf(fout, "\\endcode\n");
        fprintf(fout, "Here is a list of properties checked when starting up a device based on this configuration file.  Note that which properties are checked can depend on whether other properties are present.  In some cases properties can also vary between operating systems.  So this is just an example\n\n");
        toDox(dd,fout);
        fprintf(fout, "\n\\sa %s\n\n",
                codeName.c_str());
        fprintf(fout, " */\n");
        fclose(fout);
        fout = nullptr;
    }

    if (ok) {
        YARP_DEBUG(Logger::get(), "harness closing...");
        dd.close();
        YARP_DEBUG(Logger::get(), "harness closed.");
    }

    // just checking for crashes, not device creation
    return result; //result;
}

