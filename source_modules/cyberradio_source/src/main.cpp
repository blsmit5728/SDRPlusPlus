#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/widgets/stepped_slider.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <utils/optionlist.h>
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

#define VALUE_DEC(val) std::dec << val
#define VALUE_HEX(val,bits) "0x" << std::hex << std::setw(bits/4) << std::setfill('0') << val
#define VALUE_DEC_HEX(val,bits) VALUE_DEC(val) << " (" << VALUE_HEX(val,bits) << ")"

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
        std::memset(this->_hostname, 0, 1024);
        //std::memset(this->_streamIntf, 0, 1024);

        _interfaceTxtList = getNetworkInterfaces();

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

    std::string getNetworkInterfaces( )
    {
        struct ifaddrs *addrs,*tmp;
        getifaddrs(&addrs);
        tmp = addrs;
        std::string ret;
        while (tmp)
        {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET)
            {
                ret += tmp->ifa_name;
                this->_interfaceVector.push_back(std::string(tmp->ifa_name));
                ret += '\0';
            }
            tmp = tmp->ifa_next;
        }

        freeifaddrs(addrs);
        return ret;
    }

    void selectDevice(std::string name) {
        spdlog::info("CyberRadioModule: selectDevice({0}) Host: {1}", name.c_str(), this->_hostname);
        _handler = LibCyberRadio::Driver::getRadioObject(name, this->_hostname, -1, true);
        _connected = true;
        _handler->setTunerFrequency(0, 100000000.0);
        _currentTunedFreq = 100000000.0;
        if ( _handler->getNumWbddc() ){
            _wbddcRates = _handler->getWbddcRateSet();
        }
        if ( _handler->getNumNbddc() ){
            _nbddcRates = _handler->getNbddcRateSet();
        }
        this->_radioName = name;
        // Create the information for Channel Selection.
        this->_numChannels = _handler->getNumTuner();
        _channelsTxtList = "";
        for( int i = 0; i < this->_numChannels; i++)
        {
            _channelsTxtList += std::to_string(i);
            _channelsTxtList += '\0';
        }
        // Create stuff for Rate Selection
        for( auto &it : _nbddcRates )
        {
            std::pair<int, double> a(it.first, it.second);
            sampleRates.push_back(it.second);
            _rateSet.insert(a);
        }
        for( auto &it : _wbddcRates )
        {
            std::pair<int, double> a(it.first, it.second);
            sampleRates.push_back(it.second);
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

    void selectSourceChannel( int channel )
    {
        this->_handler->setWbddcSource( 0, channel );
    }

    std::string createSourceIPAddress( std::string input )
    {
        std::vector<std::string> strings;
        std::istringstream inputStream(input);
        std::string ret;
        std::string s;    
        while (getline(inputStream, s, '.')) {
            //std::cout << s << std::endl;
            strings.push_back(s);
        }
        int c = 0;
        for( auto element : strings )
        {
            int octet = std::stoi(element);
            if (c == 3)
            {
                octet = 100;
            }
            ret.append(std::to_string(octet));
            if( c != 3 )
            {
                ret.push_back('.');
            }
            c++;
        }
        return ret;
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
        //std::string s(this->_streamIntf);
        std::string ip = this->getIPAddress( this->_streamIntf );
        std::string mac = this->getMacAddress( this->_streamIntf );
        spdlog::info("CyberRadioModule: Iface: {0} IP: {1} MAC: {2}", 
                    this->_streamIntf, ip.c_str(), mac.c_str());
        // assume Channel 0 DDC 0 ETH0 etc.
        _handler->setWbddcSource( 0, 0 );
        _handler->enableTuner( 0, true );
        _handler->setDataPortSourceIP(0, createSourceIPAddress(ip));
        _handler->setDataPortDestInfo(0, 0, ip, mac, 4991, 4991);

        std::string n = "VITA_SDRPP";

        _vitaPacketSize =  _handler->getVitaHeaderSize() + 
                           _handler->getVitaPayloadSize() +
                           _handler->getVitaTailSize();
        _vitaSamplesPerPacket = _handler->getVitaPayloadSize() / 4;

        spdlog::info("-- Vita Header  : {0}", _handler->getVitaHeaderSize());
        spdlog::info("-- Vita Payload : {0}", _handler->getVitaPayloadSize());
        spdlog::info("-- Vita Trailer : {0}", _handler->getVitaTailSize());
        spdlog::info("-- Vita Packet  : {0}", _vitaPacketSize);

        int vitaType = 0;
        if( this->_radioName == "ndr551"  || 
            this->_radioName == "ndr358"  ||
            this->_radioName == "ndr357")
        {   
            vitaType = 551;
        } else if ( this->_radioName == "ndr324" )
        {
            vitaType = 324;
        }

        _stream = new LibCyberRadio::VitaIqSource(n,
                    vitaType, 
                    _handler->getVitaPayloadSize(), // 8224 for 
                    _handler->getVitaHeaderSize(),
                    _handler->getVitaTailSize(),
                    true,
                    false,
                    "0.0.0.0",
                    4991, true);
    }

    static void start(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        spdlog::info("CyberRadioModule '{0}': Start!", _this->name);
        _this->setupStream();
        _this->_handler->enableWbddc(0, true);
        _this->running = true;
        _this->workerThread = std::thread(&CyberRadioModule::_workerThread, _this);
    }

    static void stop(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        spdlog::info("CyberRadioModule '{0}': Stop!", _this->name);
        _this->stream.stopWriter();
        _this->running = false;
        _this->workerThread.join();
        _this->stream.clearWriteStop();
        
    }

    void setSampleRate( double rate )
    {
        // determine NBDDC, WBDDC
        if( _handler->getNumWbddc() > 0 ){
            for ( const auto& it : _wbddcRates )
            {
                if( it.second == rate )
                {
                    _handler->setWbddcRateIndex( 0, it.first );
                }
            }
        }
        if( _handler->getNumNbddc() > 0 ) {
            for ( const auto& it : _nbddcRates )
            {
                if( it.second == rate )
                {
                    _handler->setNbddcRateIndex( 0, it.first );
                }
            }
        }
    }

    static void menuHandler(void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
                
        SmGui::LeftLabel("IP Addr");
        SmGui::FillWidth();
        if(SmGui::InputText(CONCAT("##_ip_addr_",_this->name), _this->_hostname, 1023))
        {
            config.acquire();
            config.conf[_this->name]["host"] = std::string(_this->_hostname);
            config.release();
        }
        SmGui::LeftLabel("Stream Intf");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_streaming_intf_", _this->name), &_this->intfId,  _this->_interfaceTxtList.c_str()))
        //if(SmGui::InputText(CONCAT("##_streaming_intf_",_this->name), _this->_streamIntf, 1023))
        {
            _this->_streamIntf = _this->_interfaceVector.at(_this->intfId);
            spdlog::info("Setting Streaming intf: {0}", _this->_streamIntf);
            config.acquire();
            config.conf[_this->name]["intf"] = _this->_streamIntf;
            config.release();            
        }
        SmGui::LeftLabel("Radio Type");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_dev_select_", _this->name), &_this->devId,  _this->txtDevList.c_str())) {
            _this->selectDevice(_this->devList[_this->devId]);
        }
        SmGui::LeftLabel("Sample Rate");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_sr_select_", _this->name), &_this->srId, _this->txtSrList.c_str())) {
            spdlog::info("CyberRadioSource: SR: ID: {0} VALUE: {1}", _this->srId, _this->sampleRates.at(_this->srId));
            _this->selectSampleRate(_this->sampleRates.at(_this->srId));
            //_this->_handler->setSampleRate(_this->sampleRate);            
        }
        SmGui::LeftLabel("Channel");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_ch_select_", _this->name), &_this->chId, _this->_channelsTxtList.c_str())) {
            spdlog::info("CyberRadioSource: Channel: ID: {0} VALUE: {1}", _this->chId, _this->chId);
            _this->selectSourceChannel( _this->chId );
            //_this->_handler->setSampleRate(_this->sampleRate);            
        }
        // If no device is selected, draw only the refresh button
        if (SmGui::Button(CONCAT("Save Config##_dev_select_", _this->name))) {
            //_this->refresh();
            //_this->selectDevice(config.conf["device"]);
            _this->saveCurrent();
        }
    }
    static void tune(double freq, void* ctx) {
        CyberRadioModule* _this = (CyberRadioModule*)ctx;
        // 404656000.0
        int freq_int = (uint64_t)freq;
        spdlog::info("CyberRadioModule: Tune Requested for: {0}MHz", freq_int);
        if( _this->_connected ){
            if ( freq_int % 10000000 == 0 ){
                _this->_handler->setTunerFrequency(0, freq );
                _this->_currentTunedFreq = (double)freq_int;
                spdlog::info("CyberRadioModule '{0}': Tuner: {1}!", _this->name, freq);
            } 
            else {
                int remainder = (freq_int % 10000000);
                int freq_tens = freq_int - remainder;
                _this->_handler->setTunerFrequency(0, (double)freq_tens);
                _this->_currentTunedFreq = (double)freq_tens;
                double wbddc_offset = 0;
                if ( freq > _this->_currentTunedFreq )
                {
                    wbddc_offset = freq - _this->_currentTunedFreq;
                    spdlog::info("CyberRadioModule '{0}': WBDDC: {1} = {2} - {3}", _this->name, wbddc_offset, freq, _this->_currentTunedFreq);
                } else {
                    wbddc_offset = _this->_currentTunedFreq - freq;
                    wbddc_offset *= -1;
                    spdlog::info("CyberRadioModule '{0}': WBDDC: {1} = {2} - {3}", _this->name, wbddc_offset, _this->_currentTunedFreq,freq);
                }
                spdlog::info("CyberRadioModule '{0}': WBDDC: {1}", _this->name, wbddc_offset);
                _this->_handler->setWbddcFrequency(0, wbddc_offset);
            }
            _this->freq = freq;
            
            spdlog::info("CyberRadioModule '{0}': Tune: {1} :: {2}!", _this->name, freq, _this->_currentTunedFreq);
        }
        
    }

    void _workerThread( )
    {
        while ( this->running ) {
            LibCyberRadio::Vita49PacketVector packets;
            int blockSize = 2000;
            int samples = this->_handler->getVitaPayloadSize()/4;
            int payload_size = samples * 4;
            //spdlog::info("RX: Requested {0} Packets", blockSize);
            int rxd = this->_stream->getPackets(blockSize, packets);
            //spdlog::info("RX: {0} Packets", rxd);
            int y = 0;
            
            for( auto& it : packets )
            {
                volk_16i_s32f_convert_32f((float *)this->stream.writeBuf, 
                                        it.sampleData, 
                                        32768.0, 
                                        2*samples);
                this->stream.swap(samples);
            }
#if 0            
            std::stringstream st;
            st << std::hex << packets.at(0).frameTrailerWord;
            std::string g( st.str() );
            //spdlog::info("{0}", g);

            try {
                spdlog::info("{0}",packets.at(0).dump().c_str());
            } catch (int e) {
                spdlog::info("No Packet Availible");
            }
#endif
        }
    }

    void saveCurrent() {
        spdlog::info("CyberRadioModule '{0}' Save Called", this->name);
        json conf;
        conf["sampleRate"] = this->sampleRate;
        int i = 0;
        conf["host"] = this->_hostname;
        conf["streamingIf"] = this->_streamIntf;
        config.acquire();
        config.conf["devices"][this->name] = conf;
        config.release(true);
        config.save(true);
    }
 

    std::shared_ptr<LibCyberRadio::Driver::RadioHandler> _handler;

    std::string name;
    bool _connected = false;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    char _hostname[1024];
    std::string _streamIntf;
    std::string txtDevList;
    std::string txtSrList;
    std::thread workerThread;
    LibCyberRadio::Driver::WbddcRateSet _wbddcRates;
    LibCyberRadio::Driver::NbddcRateSet _nbddcRates;
    LibCyberRadio::VitaIqSource *_stream;
    std::map<int, double> _rateSet;
    int devId = -1;
    int intfId = -1;
    double freq;
    double sampleRate;
    bool running = false;
    bool hasAgc = false;
    bool agc = false;
    std::vector<double> sampleRates;
    int _numChannels;
    int srId = -1;
    int chId = -1;
    float* uiGains;
    int channelCount = 1;
    int channelId = 0;
    int uiAntennaId = 0;
    double _currentTunedFreq;
    std::vector<std::string> antennaList;
    std::string txtAntennaList;
    std::vector<std::string> gainList;
    int uiBandwidthId = 0;
    std::vector<float> bandwidthList;
    std::string txtBwList;
    std::vector<std::string> devList;
    std::string _interfaceTxtList;
    std::vector<std::string> _interfaceVector;
    int _vitaPacketSize;
    int _vitaSamplesPerPacket;
    std::string _radioName;
    std::string _channelsTxtList;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/cyberradio_source_config.json");
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
