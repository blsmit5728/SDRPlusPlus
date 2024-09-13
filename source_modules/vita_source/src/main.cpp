#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <core.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <utils/optionlist.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netpacket/packet.h>

SDRPP_MOD_INFO{
    /* Name:            */ "vita_source",
    /* Description:     */ "VITA source module for SDR++",
    /* Author:          */ "blsmit5728",
    /* Version:         */ 0, 0, 1,
    /* Max instances    */ 1
};

ConfigManager config;

class VitaSourceModule : public ModuleManager::Instance {
public:
    VitaSourceModule(uint16_t port) {
        this->name = "Vita";

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        this->refresh();
        config.acquire();
        std::string r = config.conf["device"];
        config.release();
        sigpath::sourceManager.registerSource("Vita", &handler);
    }
private:
    /***************************************************************************
     * VARIABLES
    ***************************************************************************/
    std::string name;
    SourceManager::SourceHandler handler;
    /***************************************************************************
     * FUNCTIONS
    ***************************************************************************/
};