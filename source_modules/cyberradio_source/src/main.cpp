#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/widgets/stepped_slider.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <options.h>
#include <LibCyberRadio/Driver/Driver.h>
#include "LibCyberRadio/Common/VitaIqSource.h"
#include <vector>
#include <sys/types.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netpacket/packet.h>


SDRPP_MOD_INFO{
    /* Name:            */ "cyberradio_source",
    /* Description:     */ "CyberRadio source module for SDR++",
    /* Author:          */ "blsmit5728",
    /* Version:         */ 0, 0, 1,
    /* Max instances    */ 1
};

#define CONCAT(a, b) ((std::string(a) + b).c_str())

ConfigManager config;

class CyberRadioModule : public ModuleManager::Instance {
public:
    CyberRadioModule(std::string name) {
        this->sampleRate = 10e6;
        this->name = "CRD";

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        this->devId = 0;
        std::memset(this->hostname, 0, 1024);
        std::memset(this->_streamIntf, 0, 1024);

        this->refresh();
        config.acquire();
        std::string r = config.conf["device"];
        config.release();
        sigpath::sourceManager.registerSource("CyberRadio", &handler);
    }
    ~CyberRadioModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("CyberRadio");
    }
    void postInit() {
        spdlog::info("CyberRadioModule '{0}': Post Init!", this->name);
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void refresh() {
        this->devList.clear();
        this->devList = LibCyberRadio::Driver::getSupportedDevices();
        txtDevList = "";
        for( auto it : this->devList )
        {
            txtDevList += it.c_str();
            txtDevList += '\0';
            spdlog::info("CyberRadioModule devList '{0}'", it.c_str());
        }
    }
private:

    std::string getIPAddress(std::string interface)
    {
        std::string ipAddress="Unable to get IP Address";
        std::string macAddress="Unable to get Mac Address";
    //    std::pair<std::string, std::string> ip_pair("none", "none");
        struct ifaddrs *interfaces = NULL;
        struct ifaddrs *ifa = NULL;
        int success = 0;
        char buffer[20];
        // retrieve the current interfaces - returns 0 on success
        success = getifaddrs(&interfaces);
        if (success == 0) {
            // Loop through linked list of interfaces
            for ( ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next)
            {
                if( (ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_INET) ) {
                    if( strstr(ifa->ifa_name, interface.c_str()) != NULL ){
                        ipAddress=inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr);
                    }
                }
                if( (ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET) ) {
                    if( strstr(ifa->ifa_name, interface.c_str()) != NULL ){
                        struct sockaddr_ll *s = (struct sockaddr_ll*)ifa->ifa_addr;
                        snprintf(&buffer[0], 20, "%02X:%02X:%02X:%02X:%02X:%02X",
                                s->sll_addr[0],
                                s->sll_addr[1],
                                s->sll_addr[2],
                                s->sll_addr[3],
                                s->sll_addr[4],
                                s->sll_addr[5]);
                        macAddress=buffer;
                    }
                }
            }
        }
        // Free memory
        freeifaddrs(interfaces);
        return ipAddress;
        //return ipAddress;
    }

    std::string getMacAddress(std::string interface)
    {
        std::string ipAddress="Unable to get IP Address";
        std::string macAddress="Unable to get Mac Address";
    //    std::pair<std::string, std::string> ip_pair("none", "none");
        struct ifaddrs *interfaces = NULL;
        struct ifaddrs *ifa = NULL;
        int success = 0;
        char buffer[20];
        // retrieve the current interfaces - returns 0 on success
        success = getifaddrs(&interfaces);
        if (success == 0) {
            // Loop through linked list of interfaces
            for ( ifa = interfaces; ifa != NULL; ifa = ifa->ifa_next)
            {
                if( (ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_INET) ) {
                    if( strstr(ifa->ifa_name, interface.c_str()) != NULL ){
                        ipAddress=inet_ntoa(((struct sockaddr_in*)ifa->ifa_addr)->sin_addr);
                    }
                }
                if( (ifa->ifa_addr) && (ifa->ifa_addr->sa_family == AF_PACKET) ) {
                    if( strstr(ifa->ifa_name, interface.c_str()) != NULL ){
                        struct sockaddr_ll *s = (struct sockaddr_ll*)ifa->ifa_addr;
                        snprintf(&buffer[0], 20, "%02X:%02X:%02X:%02X:%02X:%02X",
                                s->sll_addr[0],
                                s->sll_addr[1],
                                s->sll_addr[2],
                                s->sll_addr[3],
                                s->sll_addr[4],
                                s->sll_addr[5]);
                        macAddress=buffer;
                    }
                }
            }
        }
        // Free memory
        freeifaddrs(interfaces);
        return macAddress;
    }

    void selectDevice(std::string name) {
        spdlog::info("CyberRadioModule: selectDevice({0}) Host: {1}", name.c_str(), this->hostname);
        _handler = LibCyberRadio::Driver::getRadioObject(name, this->hostname, -1, true);
        _wbddcRates = _handler->getWbddcRateSet();
        _nbddcRates = _handler->getNbddcRateSet();
        for( auto &it : _wbddcRates )
        {
            std::pair<int, double> a(it.first, it.second);
            _rateSet.insert(a);
        }
        txtSrList = "";
        for( auto it : _rateSet )
        {
            std::string temp = std::to_string(it.second);
            txtSrList += temp.c_str();
            txtSrList += '\0';
            spdlog::info(" CyberRadioModule: RateInd: {0} Rate {1}",it.first, it.second);
        }
    }

    void selectSampleRate(double samplerate) {
        spdlog::info("CyberRadioModule: Setting sample rate to {0}", samplerate);
        if (_rateSet.size() == 0) {
            devId = -1;
            return;
        }
        bool found = false;
        for (auto& sr : _rateSet) {
            if (sr.second == samplerate) {
                srId = sr.first;
                sampleRate = sr.second;
                found = true;
                core::setInputSampleRate(sampleRate);
                break;
            }
        }
        if (!found) {
            // Select default sample rate
            selectSampleRate(_rateSet[0]);
        } else {
            setSampleRate( samplerate );
        }
    }

    static void menuSelected(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        spdlog::info("CyberRadioModule '{0}': Menu Select!", _this->name);
        if (_this->devList.size() == 0) {
            return;
        }
        core::setInputSampleRate(_this->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        spdlog::info("CyberRadioModule '{0}': Menu Deselect!", _this->name);        
    }

    void setupStream( void )
    {
        std::string n = "VITA_SDRPP";
        _stream = new LibCyberRadio::VitaIqSource(n,
                    3, this->_handler->getVitaPayloadSize(),
                    this->_handler->getVitaHeaderSize(),
                    this->_handler->getVitaTailSize(),
                    true,
                    false,
                    "0.0.0.0",
                    4991, true);
    }

    static void start(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        spdlog::info("CyberRadioModule '{0}': Start!", _this->name);
        _this->setupStream();
        
        
    }

    static void stop(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        spdlog::info("CyberRadioModule '{0}': Stop!", _this->name);
    }

    void setSampleRate( double rate )
    {
        LibCyberRadio::Driver::WbddcRateSet::iterator itW;
        // determine NBDDC, WBDDC
        if( _handler->getNumWbddc() > 0 ){
            itW = _wbddcRates.find(rate);
            if( itW != _wbddcRates.end() ){
                _handler->setWbddcRateIndex( 0, itW->first );
            }
        }
        if( _handler->getNumNbddc() > 0 ) {
            LibCyberRadio::Driver::NbddcRateSet::iterator itN;
            if( itN != _nbddcRates.end() ){
                _handler->setNbddcRateIndex( 0, itN->first );
            }
        }
    }

    static void menuHandler(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvailWidth();
                
        ImGui::SetNextItemWidth(menuWidth);
        if(ImGui::InputText(CONCAT("##_ip_addr_",_this->name), _this->hostname, 1023))
        {
            config.acquire();
            config.conf[_this->name]["host"] = std::string(_this->hostname);
            config.release();
        }
        ImGui::SetNextItemWidth(menuWidth);
        if(ImGui::InputText(CONCAT("##_streaming_intf_",_this->name), _this->_streamIntf, 1023))
        {
            config.acquire();
            config.conf[_this->name]["intf"] = std::string(_this->_streamIntf);
            config.release();
            std::string s(_this->_streamIntf);
            std::string ip = _this->getIPAddress( s );
            std::string mac = _this->getMacAddress( s );
            spdlog::info("CyberRadioModule: Iface: {0} IP: {1} MAC: {2}", 
                        _this->_streamIntf, ip.c_str(), mac.c_str());
        }
        // If no device is selected, draw only the refresh button
        if (ImGui::Button(CONCAT("Refresh##_dev_select_", _this->name), ImVec2(menuWidth, 0))) {
            _this->refresh();
            _this->selectDevice(config.conf["device"]);
        }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_dev_select_", _this->name), &_this->devId,  _this->txtDevList.c_str())) {
            _this->selectDevice(_this->devList[_this->devId]);
        }
        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_sr_select_", _this->name), &_this->srId, _this->txtSrList.c_str())) {
            spdlog::info("CyberRadioSource: SR: {0} {1}", _this->srId, _this->_rateSet[_this->srId]);
            _this->selectSampleRate(_this->_rateSet[_this->srId]);
            //_this->_handler->setSampleRate(_this->sampleRate);            
        }
    }
    static void tune(double freq, void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        if (_this->running) {
            //hackrf_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        spdlog::info("CyberRadioModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void _workerThread( CyberRadioModule * _this )
    {
        LibCyberRadio::Vita49PacketVector packets;

    }

#if 0

    static int callback(hackrf_transfer* transfer) {
        HackRFSourceModule* _this = (HackRFSourceModule*)transfer->rx_ctx;
        int count = transfer->valid_length / 2;
        int8_t* buffer = (int8_t*)transfer->buffer;
        volk_8i_s32f_convert_32f((float*)_this->stream.writeBuf, buffer, 128.0f, count * 2);
        if (!_this->stream.swap(count)) { return -1; }
        return 0;
    }
#endif    

    

    std::shared_ptr<LibCyberRadio::Driver::RadioHandler> _handler;

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    char hostname[1024];
    char _streamIntf[1024];
    std::string txtDevList;
    std::string txtSrList;
    std::thread workerThread;
    LibCyberRadio::Driver::WbddcRateSet _wbddcRates;
    LibCyberRadio::Driver::NbddcRateSet _nbddcRates;
    LibCyberRadio::VitaIqSource *_stream;
    std::map<int, double> _rateSet;
    int devId = -1;
    double freq;
    double sampleRate;
    bool running = false;
    bool hasAgc = false;
    bool agc = false;
    std::vector<double> sampleRates;
    int srId = -1;
    float* uiGains;
    int channelCount = 1;
    int channelId = 0;
    int uiAntennaId = 0;
    std::vector<std::string> antennaList;
    std::string txtAntennaList;
    std::vector<std::string> gainList;
    int uiBandwidthId = 0;
    std::vector<float> bandwidthList;
    std::string txtBwList;
    std::vector<std::string> devList;
};

MOD_EXPORT void _INIT_() {
    config.setPath(options::opts.root + "/cyberradio_source_config.json");
    json defConf;
    defConf["device"] = "";
    defConf["devices"] = json({});
    config.load(defConf);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new CyberRadioModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (CyberRadioModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
