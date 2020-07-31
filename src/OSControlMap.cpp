#include "plugin.hpp"
#include "OSControlMap.hpp"
#include "OSCBaseListener.hpp"
#include <ui/TextField.hpp>

OSControlMap::OSControlMap() {
	config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	configParam(ACTIVE_PARAM, 0.f, 1.f, 0.f, "");
	configParam(CONFIG_PARAM, 0.f, 1.f, 0.f, "");

	for (int id = 0; id < MAX_CHANNELS; id++) {
		paramHandles[id].color = nvgRGB(0xff, 0xff, 0x40);
		APP->engine->addParamHandle(&paramHandles[id]);
	}
	onReset();
	oscStarted = false;

}

OSControlMap::~OSControlMap() {
	for (int id = 0; id < MAX_CHANNELS; id++) {
		APP->engine->removeParamHandle(&paramHandles[id]);
	}
}

void OSControlMap::onReset() {
	learningId = -1;
	learnedCc = false;
	learnedParam = false;
	clearMaps();
	mapLen = 1;
	for (int i = 0; i < 128; i++) {
		values[i] = -1;
	}
	cleanupOSC(); // Try to clean up OSC if we already have something		
}

void OSControlMap::UpdateValuesFromMap(const ProcessArgs& args){

	// Step channels
	for (int id = 0; id < mapLen; id++) {
		int cc = ccs[id];
		if (cc < 0)
			continue;
		// Check if CC value has been set
		if (values[cc] < 0)
			continue;
		// Get Module
		Module* module = paramHandles[id].module;
		if (!module)
			continue;
		// Get ParamQuantity
		int paramId = paramHandles[id].paramId;
		ParamQuantity* paramQuantity = module->paramQuantities[paramId];
		if (!paramQuantity)
			continue;
		if (!paramQuantity->isBounded())
			continue;
		// Set ParamQuantity
		float v = rescale(values[cc], 0, 127, 0.f, 1.f);
		v = valueFilters[id].process(args.sampleTime, v);
		paramQuantity->setScaledValue(v);
	}
}

void OSControlMap::ProcessOscActions() {
	switch (oscCurrentAction)
	{
	case OSCAction::Enable:
		cleanupOSC(); // Try to clean up OSC if we already have something		
		initOSC();
		oscStarted = true;
		break;
	case OSCAction::None:
	default:
		break;
	}
	oscCurrentAction = OSCAction::None;
}

void OSControlMap::process(const ProcessArgs& args) {

	if ((int)params[ACTIVE_PARAM].getValue() == 1) {
		if (!oscStarted) {
			oscCurrentAction = OSCAction::Enable;
		}
	}else{
		oscStarted = false;
	}

	//show config screen
	oscShowConfigurationScreen = (int) params[CONFIG_PARAM].getValue() == 1;
	
	ProcessOscActions();
	UpdateValuesFromMap(args);

}

/*void processMessage(midi::Message msg) {
	switch (msg.getStatus()) {
		// cc
		case 0xb: {
			processCC(msg);
		} break;
		default: break;
	}
}

void processCC(midi::Message msg) {
	uint8_t cc = msg.getNote();
	int8_t value = msg.getValue();
	// Learn
	if (0 <= learningId && values[cc] != value) {
		ccs[learningId] = cc;
		valueFilters[learningId].reset();
		learnedCc = true;
		commitLearn();
		updateMapLen();
		refreshParamHandleText(learningId);
	}
	values[cc] = value;
}*/

void OSControlMap::clearMap(int id) {
	learningId = -1;
	ccs[id] = -1;
	APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
	valueFilters[id].reset();
	updateMapLen();
	refreshParamHandleText(id);
}

void OSControlMap::clearMaps() {
	learningId = -1;
	for (int id = 0; id < MAX_CHANNELS; id++) {
		ccs[id] = -1;
		APP->engine->updateParamHandle(&paramHandles[id], -1, 0, true);
		valueFilters[id].reset();
		refreshParamHandleText(id);
	}
	mapLen = 0;
}

void OSControlMap::updateMapLen() {
	// Find last nonempty map
	int id;
	for (id = MAX_CHANNELS - 1; id >= 0; id--) {
		if (ccs[id] >= 0 || paramHandles[id].moduleId >= 0)
			break;
	}
	mapLen = id + 1;
	// Add an empty "Mapping..." slot
	if (mapLen < MAX_CHANNELS)
		mapLen++;
}


void OSControlMap::setOscMap() {
	// Learn
	if (0 <= learningId) {
		ccs[learningId] = learningId;
		valueFilters[learningId].reset();
		refreshParamHandleText(learningId);
	}
}

void OSControlMap::commitLearn() {
	if (learningId < 0)
		return;
	/*if (!learnedCc)
		return;*/
	if (!learnedParam)
		return;

	setOscMap();

	// Reset learned state
	learnedCc = false;
	learnedParam = false;
	// Find next incomplete map
	while (++learningId < MAX_CHANNELS) {
		if (ccs[learningId] < 0 || paramHandles[learningId].moduleId < 0)
			return;
	}



	learningId = -1;
}

void OSControlMap::enableLearn(int id) {
	if (learningId != id) {
		learningId = id;
		learnedCc = false;
		learnedParam = false;
	}
}

void OSControlMap::disableLearn(int id) {
	if (learningId == id) {
		learningId = -1;
	}
}

void OSControlMap::learnParam(int id, int moduleId, int paramId) {
	APP->engine->updateParamHandle(&paramHandles[id], moduleId, paramId, true);
	learnedParam = true;
	commitLearn();
	updateMapLen();
}

void OSControlMap::refreshParamHandleText(int id) {
	std::string text;
	if (ccs[id] >= 0)
		text = string::f("/%d", ccs[id]);
	else
		text = "OSC-Map";
	paramHandles[id].text = text;
}

json_t* OSControlMap::dataToJson() {
	json_t* rootJ = json_object();

	json_t* mapsJ = json_array();
	for (int id = 0; id < mapLen; id++) {
		json_t* mapJ = json_object();
		json_object_set_new(mapJ, "cc", json_integer(ccs[id]));
		json_object_set_new(mapJ, "moduleId", json_integer(paramHandles[id].moduleId));
		json_object_set_new(mapJ, "paramId", json_integer(paramHandles[id].paramId));
		json_array_append_new(mapsJ, mapJ);
	}
	json_object_set_new(rootJ, "maps", mapsJ);

	//json_object_set_new(rootJ, "midi", midiInput.toJson());
	return rootJ;
}

void OSControlMap::dataFromJson(json_t* rootJ) {
	clearMaps();

	json_t* mapsJ = json_object_get(rootJ, "maps");
	if (mapsJ) {
		json_t* mapJ;
		size_t mapIndex;
		json_array_foreach(mapsJ, mapIndex, mapJ) {
			json_t* ccJ = json_object_get(mapJ, "cc");
			json_t* moduleIdJ = json_object_get(mapJ, "moduleId");
			json_t* paramIdJ = json_object_get(mapJ, "paramId");
			if (!(ccJ && moduleIdJ && paramIdJ))
				continue;
			if (mapIndex >= MAX_CHANNELS)
				continue;
			ccs[mapIndex] = json_integer_value(ccJ);
			APP->engine->updateParamHandle(&paramHandles[mapIndex], json_integer_value(moduleIdJ), json_integer_value(paramIdJ), false);
			refreshParamHandleText(mapIndex);
		}
	}

	updateMapLen();

	/*json_t* midiJ = json_object_get(rootJ, "midi");
	if (midiJ)
		midiInput.fromJson(midiJ);*/
}


void OSControlMap::initOSC()
{
	int portNumber = oscNewSettings.oscRxPort;

	if(!customInputPort.empty() && std::all_of(customInputPort.begin(), customInputPort.end(), ::isdigit)){
		portNumber = std::stoi(customInputPort);
	}

	oscInitialized = OSCRxConnector::StartListener(portNumber, this);
}

//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
// Clean up OSC.
//-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-
void OSControlMap::cleanupOSC() 
{
	//oscMutex.lock();
	try
	{
		DEBUG("oscCV::cleanupOSC() - Cleaning up OSC");
		oscInitialized = false;
		oscError = false;
		//OSCRxConnector::ClearPorts(oscId, currentOSCSettings.oscTxPort, currentOSCSettings.oscRxPort);
		DEBUG("oscCV::cleanupOSC() - Cleaning up RECV socket.");
		if (doOSC2CVPort)
		{
			OSCRxConnector::StopListener(currentOSCSettings.oscRxPort, this);
		}

		/*if (oscTxSocket != NULL)
		{
			DEBUG("oscCV::cleanupOSC() - Cleanup TRANS socket.");
			delete oscTxSocket;
			oscTxSocket = NULL;
		}*/
		DEBUG("oscCV::cleanupOSC() - OSC cleaned");
	}
	catch (const std::exception& ex)
	{
		DEBUG("oscCV::cleanupOSC() - Exception caught:\n%s", ex.what());
	}
	//oscMutex.unlock();
} // end cleanupOSC()


struct MapChoice : LedDisplayChoice {
	OSControlMap* module;
	int id;
	int disableLearnFrames = -1;

	void setModule(OSControlMap* module) {
		this->module = module;
	}

	void onButton(const event::Button& e) override {
		e.stopPropagating();
		if (!module)
			return;

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			e.consume(this);
		}

		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			module->clearMap(id);
			e.consume(this);
		}
	}

	void onSelect(const event::Select& e) override {
		if (!module)
			return;

		ScrollWidget* scroll = getAncestorOfType<ScrollWidget>();
		scroll->scrollTo(box);

		// Reset touchedParam
		APP->scene->rack->touchedParam = NULL;
		module->enableLearn(id);
	}

	void onDeselect(const event::Deselect& e) override {
		if (!module)
			return;
		// Check if a ParamWidget was touched
		ParamWidget* touchedParam = APP->scene->rack->touchedParam;
		if (touchedParam) {
			APP->scene->rack->touchedParam = NULL;
			int moduleId = touchedParam->paramQuantity->module->id;
			int paramId = touchedParam->paramQuantity->paramId;
			module->learnParam(id, moduleId, paramId);
		}
		else {
			module->disableLearn(id);
		}
	}

	void step() override {
		if (!module)
			return;

		// Set bgColor and selected state
		if (module->learningId == id) {
			bgColor = color;
			bgColor.a = 0.5;

			// HACK
			if (APP->event->selectedWidget != this)
				APP->event->setSelected(this);
		}
		else {
			bgColor = nvgRGBA(0, 0, 0, 0);

			// HACK
			if (APP->event->selectedWidget == this)
				APP->event->setSelected(NULL);
		}

		// Set text
		text = "";
		if (!module->spaceName.empty()) {
			text += "/" + module->spaceName;
		}
		if (module->ccs[id] > -1) {
			text += string::f("/%d - ", module->ccs[id]);
		}
		if (module->paramHandles[id].moduleId >= 0) {
			text += getParamName();
		}
		if (module->ccs[id] < 0 && module->paramHandles[id].moduleId < 0) {
			if (module->learningId == id) {
				text = "Mapping...";
			}
			else {
				text = "Unmapped";
			}
		}

		// Set text color
		if ((module->ccs[id] >= 0 && module->paramHandles[id].moduleId >= 0) || module->learningId == id) {
			color.a = 1.0;
		}
		else {
			color.a = 0.5;
		}
	}

	std::string getParamName() {
		if (!module)
			return "";
		if (id >= module->mapLen)
			return "";
		ParamHandle* paramHandle = &module->paramHandles[id];
		if (paramHandle->moduleId < 0)
			return "";
		ModuleWidget* mw = APP->scene->rack->getModule(paramHandle->moduleId);
		if (!mw)
			return "";
		// Get the Module from the ModuleWidget instead of the ParamHandle.
		// I think this is more elegant since this method is called in the app world instead of the engine world.
		Module* m = mw->module;
		if (!m)
			return "";
		int paramId = paramHandle->paramId;
		if (paramId >= (int) m->params.size())
			return "";
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		std::string s;
		s += mw->model->name;
		s += " ";
		s += paramQuantity->label;
		return s;
	}
};

struct OSCMapDisplay : Widget {

	OSControlMap* module;
	ScrollWidget* scroll;
	MapChoice* choices[MAX_CHANNELS];
	LedDisplaySeparator* separators[MAX_CHANNELS];

	void setModule(OSControlMap* module) {
		this->module = module;

		scroll = new ScrollWidget;
		scroll->box.pos = Vec(0., 48.);
		scroll->box.size.x = box.size.x;
		scroll->box.size.y = box.size.y - scroll->box.pos.y;
		addChild(scroll);

		LedDisplaySeparator* separator = createWidget<LedDisplaySeparator>(scroll->box.pos);
		separator->box.size.x = box.size.x;
		addChild(separator);
		separators[0] = separator;

		Vec pos = Vec(0., 10.);

		for (int id = 0; id < MAX_CHANNELS; id++) {
			if (id > 0) {
				LedDisplaySeparator* separator = createWidget<LedDisplaySeparator>(pos);
				separator->box.size.x = box.size.x;
				scroll->container->addChild(separator);
				separators[id] = separator;
			}

			MapChoice* choice = createWidget<MapChoice>(pos);
			choice->box.size.x = box.size.x;
			choice->id = id;
			choice->setModule(module);
			scroll->container->addChild(choice);
			choices[id] = choice;

			pos = choice->box.getBottomLeft();
		}
	}

	void step() override {
		if (module) {

			int mapLen = module->mapLen;
			for (int id = 0; id < MAX_CHANNELS; id++) {
				choices[id]->visible = (id < mapLen);
				separators[id]->visible = (id < mapLen);
			}
		}

		Widget::step();
	}

	void draw(const DrawArgs &args) override{
		if(!module)
			return;
		if(module->oscShowConfigurationScreen){
			return;
		}
		if (!this->visible)
		{
			return;
		}
		
		// Screen:
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, nvgRGB(1.f, 0.5f, 1.f));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStroke(args.vg);
		// Draw children:
		this->Widget::draw(args);	
	}
};

struct OSCConfigWidget : OpaqueWidget
{

	OSControlMap* module;

	void setModule(OSControlMap* module) {
		this->module = module;
	}

	void step() override {
		Widget::step();
	}

	void draw(const DrawArgs &args) override{
		if(!module)
			return;

		if(!module->oscShowConfigurationScreen){
			return;
		}

		if (!this->visible)
		{
			return;
		}
		
		// Screen:
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
		nvgFillColor(args.vg, nvgRGB(1.f, 0.5f, 1.f));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStroke(args.vg);

		// Draw labels
		/*nvgFillColor(args.vg, nvgRGB(0xee, 0xee, 0xee));
		nvgFontSize(args.vg, 12);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);

		float y = 15 - 1;
		float x;
		x = textBoxes[0]->box.pos.x + 2;
		nvgText(args.vg, x, y, "IP Address:", NULL);
		x = textBoxes[1]->box.pos.x + 2;
		nvgText(args.vg, x + 1, y, "spaceName", NULL);

		// Current status:
		/*x = box.size.x - 8;
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, statusColor);
		nvgText(args.vg, x, y, statusMsg.c_str(), NULL);
		// Status 2
		y += textBoxes[0]->box.size.y + 2;
		if (!statusMsg2.empty())
		{
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			nvgText(args.vg, x, y, statusMsg2.c_str(), NULL);
		}

		// Draw Messages:
		x = textBoxes[0]->box.pos.x + 2;
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		if (!errorMsg.empty())
		{
			nvgFillColor(args.vg, errorColor);		
			nvgText(args.vg, x, y, errorMsg.c_str(), NULL);
		}
		else if (!successMsg.empty())
		{
			nvgFillColor(args.vg, successColor);
			nvgText(args.vg, x, y, successMsg.c_str(), NULL);
		}
		else
		{
			nvgFillColor(args.vg, textColor);
			nvgText(args.vg, x, y, "Open Sound Control Configuration", NULL);
		}

		// Draw children:
		this->OpaqueWidget::draw(args);	
		
		// Quick and dirty -- Draw labels on buttons:
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		y = btnSave->box.pos.y + btnSave->box.size.y / 2.0 + 1;
		// Save:
		x = btnSave->box.pos.x + btnSave->box.size.x / 2.0 + 7;
		if (btnActionEnable)
		{
			nvgFillColor(args.vg, TSColors::COLOR_TS_GREEN);
			nvgText(args.vg, x, y, "ENABLE", NULL);
		}
		else
		{
			nvgFillColor(args.vg, TSColors::COLOR_TS_ORANGE);
			nvgText(args.vg, x, y, "DISABLE", NULL);
		}*/
		this->Widget::draw(args);	

	}

};

struct CustomOSControlTextField : TextField{

	OSControlMap* module;
	std::string oldText;

	void setModule(OSControlMap* module) {
		this->module = module;
	}

	void onDeselect(const event::Deselect &e) override
	{

		if (text.compare(oldText) != 0 && placeholder.compare("127.0.0.1:7001") == 0){
			module->customInputPort = module->oscNewSettings.oscRxPort;

			if(!text.empty() && std::all_of(text.begin(), text.end(), ::isdigit)){

				int portNumber = std::stoi(text);
				if(portNumber && (1000 < portNumber && portNumber < 9999)){
					module->customInputPort = text;
				}
			}

			text = "127.0.0.1:" + module->customInputPort;
			module->oscCurrentAction = OSCAction::Enable;
		}

		if (text.compare(oldText) != 0 && placeholder.compare("spaceName") == 0){
			module->spaceName = text;
		}

		oldText = text;
	}

	void step() override {
		if (!module)
			return;

		TextField::step();

	}
};


struct OSControlMapWidget : ModuleWidget {
	LedDisplaySeparator* separators[MAX_CHANNELS];


	OSControlMapWidget(OSControlMap* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/OSControlMapPanel.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		OSCConfigWidget* oscConfigWidget = createWidget<OSCConfigWidget>(mm2px(Vec(3.2, 8.)));
		oscConfigWidget->box.size = mm2px(Vec(43.999, 90.));
		oscConfigWidget->setModule(module);
		addChild(oscConfigWidget);

		OSCMapDisplay* oscMapWidget = createWidget<OSCMapDisplay>(mm2px(Vec(3.2, 8.)));
		oscMapWidget->box.size = mm2px(Vec(43.999, 90.));
		oscMapWidget->setModule(module);
		addChild(oscMapWidget);

		CustomOSControlTextField* customPortTextField = createWidget<CustomOSControlTextField>(Vec(15., 30.));
		customPortTextField->box.size = Vec(120, 20);
		customPortTextField->placeholder = "127.0.0.1:7001";
		customPortTextField->setModule(module);
		addChild(customPortTextField);

		CustomOSControlTextField* spaceText = createWidget<CustomOSControlTextField>(customPortTextField->box.getBottomLeft());
		spaceText->box.size = Vec(120, 20);
		spaceText->placeholder = "spaceName";
		spaceText->setModule(module);
		addChild(spaceText);

		addParam(createParam<CKSS>(mm2px(Vec(9.549, 109.451)), module, OSControlMap::ACTIVE_PARAM));
		addParam(createParam<CKSS>(mm2px(Vec(37.065, 109.451)), module, OSControlMap::CONFIG_PARAM));


	}
};




Model* modelOSControlMap = createModel<OSControlMap, OSControlMapWidget>("OSControlMap");
