#include "plugin.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelPushMap);
	p->addModel(modelOSControlMap);
}
