#include <mutex>
#include <map>
#include <vector>
#include "../lib/oscpack/osc/OscOutboundPacketStream.h"
#include "../lib/oscpack/ip/UdpSocket.h"
#include "../lib/oscpack/osc/OscReceivedElements.h"
#include "../lib/oscpack/osc/OscPacketListener.h"
#include <thread>


//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Simple single message to/from OSC. 
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
struct OSCSimpleMessage {
	// Channel Number (1-N)
	int channelNum;
	//float rxVal;
	//uint32_t uintRxVal;
	
	// [v1.0.2] Support for polyphonic (16-channel) cables
	std::vector<float> rxVals;
	
	
	OSCSimpleMessage(int chNum, std::vector<float>& vals)
	{
		channelNum = chNum;
		for (int i = 0; i < static_cast<int>(vals.size()); i++) 
			rxVals.push_back(vals[i]); 
		return;
	}

	OSCSimpleMessage(int chNum, float recvVal)
	{
		channelNum = chNum;
		//rxVal = recvVal;
		rxVals.push_back(recvVal);
		return;
	}
	// TSOSCCVSimpleMessage(int chNum, float recvVal, uint32_t uintVal)
	// {
		// channelNum = chNum;
		// //rxVal = recvVal;
		// rxVals.push_back(recvVal);		
		// uintRxVal = uintVal;
		// return;
	// }
};


// Osc port information
struct TSOSCPortInfo {
	enum PortUse
	{
		// Normal behavior for sequencers Rx and Tx. Only one module can register the port.
		Singular,
		// Tx and Rx will be allowed for multiple cvOSCcv's, but shouldn't be allowed for sequencers (it doesn't make sense 
		// for > 1 sequencer to talk to the same endpoint and possibly fight each other, especially since we have feedback to the endpoint...).
		// For now (for debugging), allow cvOSCcv's to talk to each other. Perhaps this may cause problems down the line though...
		Multiple
	};
	PortUse portUse = PortUse::Singular;
	
	enum PortType
	{
		Rx,
		Tx
	};	
	PortType portType = PortType::Rx;
	// List of ids using this port.
	std::vector<int> ids;	
	
	TSOSCPortInfo(PortUse pUse, PortType pType, int id)
	{
		portUse = pUse;
		portType = pType;
		ids.push_back(id);
		return;
	}
};




//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Base Listener for OSC incoming messages.
// This listener should route the messages on one Rx port to N oscCV or trowaSoft sequencer modules. 
// Apparently in C++, your parent thread's death does not result in your own demise, so if whichever thread spawns the slave listener exits,
// the listener thread won't die... Yay!
// So type <T> should be oscCV or TSSequencerBase if we allow sequencers to share the same ports...
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
template<class T>
class OSCBaseMsgRouter : public osc::OscPacketListener {
public:

	OSControlMap* module;

	// Modules that are registered for messages.
	typename std::vector<T*> modules;
	
	// Instantiate a listener.
	OSCBaseMsgRouter()
	{
		/// TODO: Make a damn base OSC interface class.
		//static_assert(std::is_base_of<Module, T>::value, "Must be a Module.");
		return;
	}
	// Instantiate a listener.
	OSCBaseMsgRouter(T* oscModule)
	{
		modules.push_back(oscModule);
		return;
	}
	// Add a module to the list.
	void addModule(T* oscModule)
	{
		std::lock_guard<std::mutex> lock(mutModule);
		if (std::find(modules.begin(), modules.end(), oscModule) == modules.end())
		{
			modules.push_back(oscModule);
		}
		return;
	}
	// Remove a module.
	void removeModule(T* oscModule)
	{
		std::lock_guard<std::mutex> lock(mutModule);
		typename std::vector<T*>::iterator it = std::find(modules.begin(), modules.end(), oscModule);
		if (it != modules.end())
		{
			// Remove reference but don't delete the reference.
			modules.erase(it);
		}
		return;
	}
protected:
	// Mutex for adjust modules
	std::mutex mutModule;

};




//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Information about the port, the socket, and the modules that are subscribed to receive messages.
// Type <T> should be oscCV or TSSequencerBase if we allow sequencers to share the same ports...
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
template<class T>
struct OscRxDetails
{
	// Receiving OSC socket.
	UdpListeningReceiveSocket* oscRxSocket = NULL;
	// The port.
	uint16_t port;
	// The message router.
	OSCBaseMsgRouter<T>* router = NULL;
	// The OSC listener thread
	std::thread oscListenerThread;	
	OscRxDetails(uint16_t port)
	{
		//static_assert(std::is_base_of<Module, T>::value, "Must be a Module.");		
		this->port = port;
	}	
	~OscRxDetails()
	{
		cleanUp();
		return;
	}
	void cleanUp()
	{
		if (oscRxSocket != NULL) // No more modules
		{
			oscRxSocket->AsynchronousBreak();
			oscListenerThread.join(); // Wait for him to finish
			delete oscRxSocket;			
			oscRxSocket = NULL;
			
			delete router;
		}					
	}
};
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// OSC Rx Connector.
// Type <T> should be oscCV or TSSequencerBase if we allow sequencers to share the same ports...
// Type <R> should be derived from TOSCBaseMsgRouter.
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
template<class T, class R>
class TSOSCRxConnector
{
private:

	TSOSCRxConnector()
	{
		//static_assert(std::is_base_of<Module, T>::value, "Must be a Module.");
		//static_assert(std::is_base_of<OSCBaseMsgRouter<T>, R>::value, "Must be a OSCBaseMsgRouter.");		
		return;
	}
	static TSOSCRxConnector<T, R>* _instance;
	// The ports that are receiving messages.
	std::map<uint16_t, OscRxDetails<T>*> _portMap;
	// Clean up thread and everything.
	//void cleanUpListener(OscRxDetails<T>* details);
	// Port mutex.
	std::mutex _mutex;
public:
	static TSOSCRxConnector<T, R>* Connector()
	{
		if (!TSOSCRxConnector<T, R>::_instance)
			TSOSCRxConnector<T, R>::_instance = new TSOSCRxConnector<T, R>();
		return _instance;	
	}

	bool startListener(uint16_t rxPort, T* module)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		DEBUG("TSOSCRxConnector::startListener(port %d) - Starting for module id %d.", rxPort,  module->id);
		bool success = false;
		OscRxDetails<T>* item = (_portMap.count(rxPort) < 1) ? NULL : _portMap[rxPort];
		if (item == NULL)
		{
			item = new OscRxDetails<T>(rxPort);
		}
		if (item->router == NULL)
		{
			item->router = new R();// OSCBaseMsgRouter<T>();
			item->router->module = module;
			DEBUG("TSOSCRxConnector::startListener(port %d) - Create router/listener object.", rxPort);
		}
		item->router->addModule(module);
		DEBUG("TSOSCRxConnector::startListener(port %d) - Add module to router module list. Now it has %d items.", rxPort, item->router->modules.size());
		if (item->oscRxSocket == NULL)
		{
			DEBUG("TSOSCRxConnector::startListener(port %d) - Creating Rx socket and starting listener thread.", rxPort);
			item->oscRxSocket = new UdpListeningReceiveSocket(IpEndpointName(IpEndpointName::ANY_ADDRESS, rxPort), item->router);
			// Start the thread
			item->oscListenerThread = std::thread(&UdpListeningReceiveSocket::Run, item->oscRxSocket);
		}
		_portMap[rxPort] = item;
		success = true;
		return success;		
	} // end startListener()

	bool stopListener(uint16_t rxPort, T* module)
	{
		DEBUG("TSOSCRxConnector::stopListener(port %d, id=%d) - Stopping listener for module id %d.", rxPort, module->id, module->id);
		std::lock_guard<std::mutex> lock(_mutex);	
		bool success = false;
		typename std::map<uint16_t, OscRxDetails<T>*>::iterator it = _portMap.find(rxPort);
		OscRxDetails<T>* item = (it == _portMap.end()) ? NULL : it->second;
		if (item != NULL)
		{
			if (item->router != NULL)
			{
				DEBUG("TSOSCRxConnector::stopListener(port %d, id=%d) - Removing module from list.", rxPort, module->id);
				item->router->removeModule(module); // Remove this module from the list.
			}		
			if (item->router == NULL || item->router->modules.size() < 1)
			{
				DEBUG("TSOSCRxConnector::stopListener(port %d, id=%d) - NO MORE modules listening to this port. Deleting...", rxPort, module->id);
				// No more modules are listening to this
				// No more registered, remove.
				item->cleanUp();
				delete it->second;			
				_portMap.erase(it);
			}
			success = true;
		}
		else
		{
			DEBUG("TSOSCRxConnector::stopListener(port %d, id=%d) - No item found for port!!!! Nothing to stop!", rxPort, module->id);		
		}
		return success;			
	} // end stopListener()
	
	static bool StartListener(uint16_t rxPort, T* module)
	{
		return Connector()->startListener(rxPort, module);
	}
	static bool StopListener(uint16_t rxPort, T* module)
	{
		return Connector()->stopListener(rxPort, module);
	}


	// Clear the usage of these ports.
	bool clearPorts(int id, uint16_t txPort, uint16_t rxPort)
	{
	#if TROWA_DEBUG_MSGS >= TROWA_DEBUG_LVL_LOW	
		DEBUG("OSC Connector - Clearing ports for id %d: %u and %u.", id, txPort, rxPort);
	#endif
		std::lock_guard<std::mutex> lock(_mutex);
		std::map<uint16_t, TSOSCPortInfo*>::iterator it;
		int nErased = 0;
		
		uint16_t port = txPort;
		it = _portMap.find(port);
		if (it != _portMap.end())  // _portMap[port] == id)
		{
			std::vector<int>::iterator position = std::find(_portMap[port]->ids.begin(), _portMap[port]->ids.end(), id);
			if (position != _portMap[port]->ids.end())
			{
				// This id was registered with this port
				_portMap[port]->ids.erase(position);			
			}
			if (_portMap[port]->ids.size() < 1)
			{
				// No more registered, remove.
				delete it->second; // Delete port info
				_portMap.erase(it);			
			}
			nErased++;
		}	
		
		port = rxPort;
		it = _portMap.find(port);
		if (it != _portMap.end())  // _portMap[port] == id)
		{
			std::vector<int>::iterator position = std::find(_portMap[port]->ids.begin(), _portMap[port]->ids.end(), id);
			if (position != _portMap[port]->ids.end())
			{
				// This id was registered with this port
				_portMap[port]->ids.erase(position);			
			}
			if (_portMap[port]->ids.size() < 1)
			{
				// No more registered, remove.
				delete it->second; // Delete port info
				_portMap.erase(it);			
			}
			nErased++;
		}	
		return nErased == 2;
	} // end clearPorts()	
};

template <class T, class R>
TSOSCRxConnector<T, R>* TSOSCRxConnector<T, R>::_instance = NULL;


//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Listener for OSC incoming messages.
// This listener should route the messages to N modules. 
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
class OSCRxMsgRouter : public OSCBaseMsgRouter<OSControlMap> {
public:


	//--------------------------------------------------------------------------------------------------------------------------------------------
	// ProcessMessage()
	// @rxMsg : (IN) The received message from the OSC library.
	// @remoteEndPoint: (IN) The remove end point (sender).
	// Handler for receiving messages from the OSC library. Taken from their example listener.
	// Should create a generic TSExternalControlMessage for our trowaSoft sequencers and dump it in the module instance's queue.
	//--------------------------------------------------------------------------------------------------------------------------------------------
	void ProcessMessage(const osc::ReceivedMessage& rxMsg, const IpEndpointName& remoteEndpoint) override
	{
		(void)remoteEndpoint; // suppress unused parameter warning

		float stepVal = 0.0;
		/*osc::int32 step = -1;
		osc::int32 intVal = -1;*/
		try {
			std::string incompingPath = rxMsg.AddressPattern();
			//DEBUG("incompingPath: %s", incompingPath.c_str());

			for (int id = 0; id < module->mapLen; id++) {
				int cc = module->ccs[id];
				if (cc < 0)
					continue;

				std::string mappedPath = "/" + std::to_string(cc);
				if (!module->spaceName.empty()) {
					mappedPath = "/" + module->spaceName + mappedPath;
				}
				DEBUG("mappedPath: %s", mappedPath.c_str());

				if ( mappedPath == incompingPath){

					osc::ReceivedMessageArgumentStream args = rxMsg.ArgumentStream();
					args >> stepVal >> osc::EndMessage;

					DEBUG("stepVal: %f", stepVal);
					module->values[cc] =  (int) (stepVal * 127.f);
				}

			}

		} // end try
		catch (osc::Exception& e) {
			DEBUG("Error parsing OSC message %s: %s", rxMsg.AddressPattern(), e.what());
		} // end catch
		return;
	} // end ProcessMessage()
	
	
};


// OSC Receiver Connector
class OSCRxConnector : public TSOSCRxConnector<OSControlMap, OSCRxMsgRouter>
{
public:

private:
};
