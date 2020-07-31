#include <mutex>

#include "../lib/oscpack/osc/OscReceivedElements.h"
#include "../lib/oscpack/osc/OscPacketListener.h"
#include "../lib/oscpack/ip/UdpSocket.h"

static const int MAX_CHANNELS = 128;
//--- OSC defines --
// Default OSC outgoing address (Tx). 127.0.0.1.
#define OSC_ADDRESS_DEF		"127.0.0.1"
// Default OSC outgoing port (Tx). 7000.
#define OSC_OUTPORT_DEF		7000
// Default OSC incoming port (Rx). 7001.
#define OSC_INPORT_DEF		7001


enum OSCAction {
	None,
	Disable,
	Enable
};

// OSC connection information
typedef struct TSOSCInfo {	
	// OSC output IP address.
	std::string oscTxIpAddress;
	// OSC output port number.
	uint16_t oscTxPort;
	// OSC input port number.
	uint16_t oscRxPort;
} TSOSCConnectionInfo;

struct OSControlMap : Module {
	enum ParamIds {
		ACTIVE_PARAM,
		CONFIG_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	int oscId;
	// Mutex for osc messaging.
	std::mutex oscMutex;

	// Flag for doing Input Rack CV -> OSC. 
	bool doCVPort2OSC = true;
	// Flag for Input OSC -> Rack CV
	bool doOSC2CVPort = true;

	bool oscStarted = false;

	/** The smoothing processor (normalized between 0 and 1) of each channel */
	dsp::ExponentialFilter valueFilters[MAX_CHANNELS];
	bool filterInitialized[MAX_CHANNELS] = {};
	dsp::ClockDivider divider;

	/** Number of maps */
	int mapLen = 0;
	/** The mapped param handle of each channel */
	ParamHandle paramHandles[MAX_CHANNELS];
	/** Channel ID of the learning session */
	int learningId;
	/** Whether the param has been set during the learning session */
	bool learnedParam;
	int len = 0; // the number of bytes read from the socket
	/** The mapped CC number of each channel */
	int ccs[MAX_CHANNELS];
	std::map<int, std::string> portNames;

	/** Whether the CC has been set during the learning session */
	bool learnedCc;
	/** The value of each CC number */
	int8_t values[128];

	// Flag if OSC objects have been initialized
	bool oscInitialized = false;

	// OSC message listener
	//OSCRxConnector* oscListener = NULL;

	// If there is an osc error.
	bool oscError = false;

	char oscBuffer[1024]; // declare a buffer into which to read the socket contents
	TSOSCConnectionInfo currentOSCSettings = { OSC_ADDRESS_DEF,  OSC_OUTPORT_DEF , OSC_INPORT_DEF };
	TSOSCInfo oscNewSettings = { OSC_ADDRESS_DEF,  OSC_OUTPORT_DEF , OSC_INPORT_DEF };

	// Flag for our module to either enable or disable osc.
	OSCAction oscCurrentAction = OSCAction::None;

	// Show the OSC configuration screen or not.
	bool oscShowConfigurationScreen = false;
	
	std::string customInputPort;
	std::string spaceName;


	OSControlMap();
	~OSControlMap();

	void onReset() override;
	void process(const ProcessArgs& args) override;
	void clearMap(int id);
	void clearMaps();
	void updateMapLen();
	void commitLearn();
	void enableLearn(int id);
	void disableLearn(int id);
	void learnParam(int id, int moduleId, int paramId);
	void refreshParamHandleText(int id);

	void initOSC();
	void cleanupOSC();
	void setOscMap();
	void UpdateValuesFromMap(const ProcessArgs& args);
	void ProcessOscActions();

	void dataFromJson(json_t* rootJ) override;
	json_t* dataToJson() override;

};