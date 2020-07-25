#include "PushMap.hpp"

struct Push2Display : FramebufferWidget {

	unsigned char frame_header[16] = {
          0xFF, 0xCC, 0xAA, 0x88,
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00 };

	libusb_device_handle *device_handle;

public:

	ssize_t count;
	bool display_connected = false;

  	unsigned char* image;

  	ParamHandle ** paramHandles;

  	int * len;
  	char ** names;
  	float * values;
  	int * ccs;
  	int skip = 0;

  	int open() {

	    int result = libusb_init(NULL);

	    if (result != 0)
	    {
	      	return NULL;
	    }

		device_handle = libusb_open_device_with_vid_pid(NULL, ABLETON_VENDOR_ID, PUSH2_PRODUCT_ID);

		if (device_handle == NULL)
		{
        	//DEBUG("%s", "Could not initilialize usblib");
	      	return NULL;
	    }

		if ((result = libusb_claim_interface(device_handle, 0)) < 0)
		{
			libusb_close(device_handle);
			device_handle = NULL;
		}
		else
		{
        	DEBUG("%s", "Display Connected");
  			display_connected = true;
		}

    }

    void close() {
    	display_connected = false;
	    if (device_handle) {
	        libusb_release_interface(device_handle, 0);
	        libusb_close(device_handle);
			device_handle = NULL;
	    }
	}

	void draw(NVGcontext * vg) {
		if (len == nullptr) return;
		int l = *len - 1;
		for (int i = 0; i < l; i ++) {
			ParamHandle* paramHandle = paramHandles[i];
			if (paramHandle->moduleId < 0) continue;
			ModuleWidget* mw = APP->scene->rack->getModule(paramHandle->moduleId);
			if (!mw) continue;
			
			Module* m = mw->module;
			if (!m) continue;
			int paramId = paramHandle->paramId;
			if (paramId >= (int) m->params.size()) continue;
			ParamQuantity* paramQuantity = m->paramQuantities[paramId];
			std::string s;
			s += paramQuantity->label;

			nvgBeginPath(vg);
			nvgFontSize(vg, 17.f);		
			char param_name[20];
			nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			// sprintf(param_name, "param%d", i);
			nvgText(vg, 98*i + (2*i + 1)*11 + 50, 20, s.c_str(), NULL);
			nvgFillColor(vg, nvgRGBA(255,255,255,120));
			nvgFill(vg);
			nvgClosePath(vg);


			nvgBeginPath(vg);
	        nvgArc(vg, 98*i + (2*i + 1)*11 + 50, 100, 40, M_PI * (0.5f + 2 * values[ccs[i]] / 127.f), M_PI * 0.5f, NVG_CCW);
	        nvgStrokeWidth(vg, 5.f);
	        nvgStrokeColor(vg, nvgRGBA(255,255,255,120));
	        nvgStroke(vg);
	        nvgClosePath(vg);


	        nvgBeginPath(vg);
			nvgFontSize(vg, 17.f);		
			char val[20];
			nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
			sprintf(val, "%d", (int)values[ccs[i]]);
			nvgText(vg, 98*i + (2*i + 1)*11 + 50, 100, val, NULL);
			nvgFillColor(vg, nvgRGBA(255,255,255,120));
			nvgFill(vg);
			nvgClosePath(vg);
		}
	}

	void setLabelsAndValues(int * len, ParamHandle ** paramHandles, float * values, int * ccs) {
		this->len = len;
		this->paramHandles = paramHandles;
		this->values = values;
		this->ccs = ccs;
	}

	Push2Display() {
		display_connected = false;
		device_handle = NULL;
		len = nullptr;
		image = (unsigned char*) malloc(PUSH2_DISPLAY_IMAGE_BUFFER_SIZE);
		//image = (t_uint8*) sysmem_newptrclear(PUSH2_DISPLAY_IMAGE_BUFFER_SIZE);//(unsigned char*)malloc(960*160*4);
	}

	~Push2Display() {
		//printf("Delete push\n");
		if (display_connected) close();
	}

	void sendDisplay(unsigned char* imageDisplay) {
		int actual_length;

		if (device_handle == nullptr) {
			if (display_connected){
				//printf("no device\n");
				display_connected = false;
			} 
		}else{ //print one frame

			if (!display_connected){
				//printf("Push connected\n");
				display_connected = true;
			}

			int result = libusb_bulk_transfer(device_handle, PUSH2_BULK_EP_OUT, frame_header, sizeof(frame_header), &actual_length, PUSH2_TRANSFER_TIMEOUT);

			if (result != 0)
				return;

			for (int i = 0; i < 160; i++)
				libusb_bulk_transfer(device_handle, PUSH2_BULK_EP_OUT, &imageDisplay[(159 - i)*1920], 2048, &actual_length, PUSH2_TRANSFER_TIMEOUT);

		}
	}

	void drawFramebuffer() override {

		// It's more important to not lag the frame than to draw the framebuffer
		NVGcontext* vg = APP->window->vg;

	    dirty = true;

	    skip++;
		if (skip < 3)
			return;
	    skip = 0;
		//if (module->divider.process()) {

	    if (display_connected) {

	    	nvgSave(vg);
			glViewport(0, 0, 960, 160);
		    glClearColor(0, 0, 0, 0);
		    glClear(GL_COLOR_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	    	nvgBeginFrame(vg, 960,  160, 1);

		    draw(vg);

		    nvgEndFrame(vg);

		    glReadPixels(0, 0, 960, 160, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, image); 

		    for (int i = 0; i < 80*960; i ++){
		      image[i * 4] ^= 0xE7;
		      image[i * 4 + 1] ^= 0xF3;
		      image[i * 4 + 2] ^= 0xE7;
		      image[i * 4 + 3] ^= 0xFF;
		    }
	    	
	    	nvgRestore(vg);
	    }

	    if (display_connected) {
		    if (APP->window->isFrameOverdue())
				return;

	    	sendDisplay(image);
	    }


	}

};