#include "plugin.hpp"


struct Palette : Module {
	enum ParamId {
		DENSITY_PARAM,
		NOTE_CS_PARAM,
		NOTE_DS_PARAM,
		NOTE_FS_PARAM,
		NOTE_GS_PARAM,
		NOTE_AS_PARAM,
		NOTE_C_PARAM,
		NOTE_D_PARAM,
		NOTE_E_PARAM,
		NOTE_F_PARAM,
		NOTE_G_PARAM,
		NOTE_A_PARAM,
		NOTE_B_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		PITCH_OUTPUT,
		GATE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		NOTE_B_LIGHT_LIGHT,
		NOTE_AS_LIGHT_LIGHT,
		NOTE_A_LIGHT_LIGHT,
		NOTE_G_LIGHT_LIGHT,
		NOTE_GS_LIGHT_LIGHT,
		NOTE_FS_LIGHT_LIGHT,
		NOTE_F_LIGHT_LIGHT,
		NOTE_E_LIGHT_LIGHT,
		NOTE_DS_LIGHT_LIGHT,
		NOTE_D_LIGHT_LIGHT,
		NOTE_C_LIGHT_LIGHT,
		NOTE_CS_LIGHT_LIGHT,
		LIGHTS_LEN
	};

	dsp::SchmittTrigger clockTrigger;
	int selectedNote = 0;

	void chooseNote() {
		const int noteParams[12] = {
			NOTE_C_PARAM, NOTE_CS_PARAM, NOTE_D_PARAM, NOTE_DS_PARAM,
			NOTE_E_PARAM, NOTE_F_PARAM, NOTE_FS_PARAM, NOTE_G_PARAM,
			NOTE_GS_PARAM, NOTE_A_PARAM, NOTE_AS_PARAM, NOTE_B_PARAM
		};

		float totalWeight = 0.f;
		for (int note = 0; note < 12; note++) {
			totalWeight += params[noteParams[note]].getValue();
		}

		if (totalWeight <= 0.f) {
			return;
		}

		float target = random::uniform() * totalWeight;
		float accumulatedWeight = 0.f;

		for (int note = 0; note < 12; note++) {
			accumulatedWeight += params[noteParams[note]].getValue();

			if (target <= accumulatedWeight) {
				selectedNote = note;
				return;
			}
		}
	}

	Palette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_CS_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_DS_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_FS_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_GS_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_AS_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_C_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_D_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_E_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_F_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_G_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_A_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NOTE_B_PARAM, 0.f, 1.f, 0.f, "");
		configInput(CLOCK_INPUT, "");
		configInput(RESET_INPUT, "");
		configOutput(PITCH_OUTPUT, "");
		configOutput(GATE_OUTPUT, "");
	}

	void process(const ProcessArgs& args) override {
		float clockVoltage = inputs[CLOCK_INPUT].getVoltage();
		bool clockHigh = clockVoltage >= 1.f;

		if (clockTrigger.process(clockVoltage)) {
			chooseNote();
		}

		outputs[GATE_OUTPUT].setVoltage(clockHigh ? 10.f : 0.f);
		outputs[PITCH_OUTPUT].setVoltage(selectedNote / 12.f);
	}
};

struct PaletteArtwork : widget::TransparentWidget {
	std::string imagePath;

	PaletteArtwork() {
		imagePath = asset::plugin(pluginInstance, "res/PaletteBase.png");
	}

	void draw(const DrawArgs& args) override {
		auto image = APP->window->loadImage(imagePath);
		if (!image || image->handle < 0) {
			return;
		}

		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.f, 0.f, box.size.x, box.size.y);
		nvgFillPaint(args.vg, nvgImagePattern(
			args.vg, 0.f, 0.f, box.size.x, box.size.y, 0.f, image->handle, 1.f));
		nvgFill(args.vg);
	}
};

struct PaletteDensityKnob : app::SvgKnob {
	PaletteDensityKnob() {
		minAngle = -0.83f * M_PI;
		maxAngle = 0.83f * M_PI;
		setSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteDensityKnob.svg")
		));
	}
};

struct PaletteWhiteKeySlider : app::SvgSlider {
	PaletteWhiteKeySlider() {
		setBackgroundSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteWhiteKeySlider.svg")
		));
		setHandleSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteSliderHandle.svg")
		));
		setHandlePosCentered(mm2px(Vec(4.f, 19.5f)), mm2px(Vec(4.f, 2.5f)));
	}
};

struct PaletteSharpKeySlider : app::SvgSlider {
	PaletteSharpKeySlider() {
		setBackgroundSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteSharpKeySlider.svg")
		));
		setHandleSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteSliderHandle.svg")
		));
		setHandlePosCentered(mm2px(Vec(4.f, 19.5f)), mm2px(Vec(4.f, 2.5f)));
	}
};

struct PaletteWidget : ModuleWidget {
	PaletteWidget(Palette* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Palette.svg")));
		auto* artwork = new PaletteArtwork;
		artwork->box.size = box.size;
		addChildBottom(artwork);

		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(24.257, 48.873)), module, Palette::DENSITY_PARAM));

		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(12.455, 80.301)), module, Palette::NOTE_CS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(23.664, 80.196)), module, Palette::NOTE_DS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(46.082, 80.196)), module, Palette::NOTE_FS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(57.292, 80.196)), module, Palette::NOTE_GS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(68.679, 80.11)), module, Palette::NOTE_AS_PARAM));

		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(6.822, 106.486)), module, Palette::NOTE_C_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(18.087, 106.486)), module, Palette::NOTE_D_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(29.353, 106.486)), module, Palette::NOTE_E_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(40.619, 106.486)), module, Palette::NOTE_F_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(51.884, 106.486)), module, Palette::NOTE_G_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(63.15, 106.486)), module, Palette::NOTE_A_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(73.953, 106.486)), module, Palette::NOTE_B_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(71.802, 13.236)), module, Palette::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(82.427, 13.236)), module, Palette::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(82.427, 72.36)), module, Palette::PITCH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(82.427, 87.567)), module, Palette::GATE_OUTPUT));

		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(87.718, 41.981)), module, Palette::NOTE_B_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(83.19, 44.467)), module, Palette::NOTE_AS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(80.048, 44.545)), module, Palette::NOTE_A_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(71.983, 47.375)), module, Palette::NOTE_G_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(74.995, 47.403)), module, Palette::NOTE_GS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(66.644, 50.19)), module, Palette::NOTE_FS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(63.634, 50.269)), module, Palette::NOTE_F_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(59.905, 52.945)), module, Palette::NOTE_E_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(53.676, 54.812)), module, Palette::NOTE_DS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(50.585, 54.845)), module, Palette::NOTE_D_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(41.898, 56.26)), module, Palette::NOTE_C_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(44.959, 56.26)), module, Palette::NOTE_CS_LIGHT_LIGHT));

		// mm2px(Vec(27.656, 12.169))
		addChild(createWidget<Widget>(mm2px(Vec(2.66, 29.766))));
	}
};


Model* modelPalette = createModel<Palette, PaletteWidget>("Palette");