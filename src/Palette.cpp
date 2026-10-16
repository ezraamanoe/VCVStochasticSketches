#include "plugin.hpp"


struct Palette : Module {
	enum ParamId {
		DENSITY_PARAM,
		SEED_PARAM,
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

	static constexpr int MAX_VOICES = 16;

	dsp::SchmittTrigger clockTriggers[MAX_VOICES];
	dsp::SchmittTrigger resetTriggers[MAX_VOICES];
	int selectedNotes[MAX_VOICES] = {};
	bool noteActive[MAX_VOICES] = {};

	void chooseNote(int voice) {
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
				selectedNotes[voice] = note;
				return;
			}
		}
	}

	Palette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 1.f, 1.f, "Density");
		configParam(SEED_PARAM, 1.f, 999.f, 1.f, "Seed");
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
		int channels = inputs[CLOCK_INPUT].getChannels();
		if (channels < 1) {
			channels = 1;
		}

		outputs[PITCH_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);

		bool voiceZeroClockHigh = false;

		for (int voice = 0; voice < channels; voice++) {
			float clockVoltage = inputs[CLOCK_INPUT].getVoltage(voice);
			bool clockHigh = clockVoltage >= 1.f;

			if (voice == 0) {
				voiceZeroClockHigh = clockHigh;
			}

			int resetChannels = inputs[RESET_INPUT].getChannels();
			float resetVoltage = 0.f;

			if (resetChannels == 1) {
				resetVoltage = inputs[RESET_INPUT].getVoltage(0);
			}
			else if (voice < resetChannels) {
				resetVoltage = inputs[RESET_INPUT].getVoltage(voice);
			}

			if (resetTriggers[voice].process(resetVoltage)) {
				selectedNotes[voice] = 0;
				noteActive[voice] = false;
			}

			if (clockTriggers[voice].process(clockVoltage)) {
				noteActive[voice] = random::uniform() < params[DENSITY_PARAM].getValue();

				if (noteActive[voice]) {
					chooseNote(voice);
				}
			}

			outputs[GATE_OUTPUT].setVoltage(
				clockHigh && noteActive[voice] ? 10.f : 0.f,
				voice
			);
			outputs[PITCH_OUTPUT].setVoltage(
				selectedNotes[voice] / 12.f,
				voice
			);
		}

		const int noteLights[12] = {
			NOTE_C_LIGHT_LIGHT, NOTE_CS_LIGHT_LIGHT, NOTE_D_LIGHT_LIGHT,
			NOTE_DS_LIGHT_LIGHT, NOTE_E_LIGHT_LIGHT, NOTE_F_LIGHT_LIGHT,
			NOTE_FS_LIGHT_LIGHT, NOTE_G_LIGHT_LIGHT, NOTE_GS_LIGHT_LIGHT,
			NOTE_A_LIGHT_LIGHT, NOTE_AS_LIGHT_LIGHT, NOTE_B_LIGHT_LIGHT
		};

		for (int note = 0; note < 12; note++) {
			bool isPlaying = noteActive[0] && voiceZeroClockHigh
				&& selectedNotes[0] == note;
			lights[noteLights[note]].setBrightness(isPlaying ? 1.f : 0.f);
		}
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

struct PaletteToneDisplay : widget::TransparentWidget {
	Palette* module = nullptr;

	PaletteToneDisplay() {
		box.size = mm2px(Vec(29.f, 14.f));
	}

	void drawSegment(const DrawArgs& args, int segment, float x, float y, float w, float h, NVGcolor color) {
		float thickness = h * 0.12f;
		float verticalLength = h * 0.33f;
		float horizontalLength = w * 0.58f;
		float radius = thickness * 0.45f;
		nvgFillColor(args.vg, color);
		nvgBeginPath(args.vg);
		switch (segment) {
			case 0: nvgRoundedRect(args.vg, x + (w - horizontalLength) / 2.f, y, horizontalLength, thickness, radius); break;
			case 1: nvgRoundedRect(args.vg, x + w - thickness, y + thickness * 0.75f, thickness, verticalLength, radius); break;
			case 2: nvgRoundedRect(args.vg, x + w - thickness, y + h - verticalLength - thickness * 0.75f, thickness, verticalLength, radius); break;
			case 3: nvgRoundedRect(args.vg, x + (w - horizontalLength) / 2.f, y + h - thickness, horizontalLength, thickness, radius); break;
			case 4: nvgRoundedRect(args.vg, x, y + h - verticalLength - thickness * 0.75f, thickness, verticalLength, radius); break;
			case 5: nvgRoundedRect(args.vg, x, y + thickness * 0.75f, thickness, verticalLength, radius); break;
			case 6: nvgRoundedRect(args.vg, x + (w - horizontalLength) / 2.f, y + (h - thickness) / 2.f, horizontalLength, thickness, radius); break;
		}
		nvgFill(args.vg);
	}

	void drawDigit(const DrawArgs& args, char digit, float x, float y, float w, float h) {
		static const bool digitSegments[10][7] = {
			{true, true, true, true, true, true, false},
			{false, true, true, false, false, false, false},
			{true, true, false, true, true, false, true},
			{true, true, true, true, false, false, true},
			{false, true, true, false, false, true, true},
			{true, false, true, true, false, true, true},
			{true, false, true, true, true, true, true},
			{true, true, true, false, false, false, false},
			{true, true, true, true, true, true, true},
			{true, true, true, true, false, true, true}
		};

		const NVGcolor on = nvgRGB(0x9d, 0xff, 0xc5);
		const NVGcolor off = nvgRGB(0x0a, 0x55, 0x3a);
		for (int segment = 0; segment < 7; segment++) {
			bool active = digit >= '0' && digit <= '9' && digitSegments[digit - '0'][segment];
			
			if (digit == '-') {
				active = segment == 6;
			}
			drawSegment(args, segment, x, y, w, h, active ? on : off);
		}
	}

	void draw(const DrawArgs& args) override {
		const NVGcolor face = nvgRGB(0x03, 0x3f, 0x2b);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, mm2px(1.f));
		nvgFillColor(args.vg, face);
		nvgFill(args.vg);

		int tone = module && module->noteActive[0] ? module->selectedNotes[0] : -1;
		std::string value = tone >= 0 ? string::f("%03d", tone + 1) : "---";
		float digitWidth = box.size.x * 0.20f;
		float digitHeight = box.size.y * 0.62f;
		float gap = box.size.x * 0.055f;
		float startX = (box.size.x - (digitWidth * 3.f + gap * 2.f)) / 2.f;
		float startY = (box.size.y - digitHeight) / 2.f;
		for (int digit = 0; digit < 3; digit++) {
			drawDigit(args, value[digit], startX + digit * (digitWidth + gap), startY, digitWidth, digitHeight);
		}
	}
};

struct PaletteWidget : ModuleWidget {
	PaletteWidget(Palette* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Palette.svg")));
		auto* artwork = new PaletteArtwork;
		artwork->box.size = box.size;
		addChildBottom(artwork);

		addChild(createWidget<ScrewSilver>(Vec(5, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 20, 0)));
		addChild(createWidget<ScrewSilver>(Vec(5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 20, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(24.257, 48.873)), module, Palette::DENSITY_PARAM));
		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(9.163f, 48.961f)), module, Palette::SEED_PARAM));

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

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(87.718, 41.981)), module, Palette::NOTE_B_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(83.19, 44.467)), module, Palette::NOTE_AS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(80.048, 44.545)), module, Palette::NOTE_A_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(71.983, 47.375)), module, Palette::NOTE_G_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(74.995, 47.403)), module, Palette::NOTE_GS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(66.644, 50.19)), module, Palette::NOTE_FS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(63.634, 50.269)), module, Palette::NOTE_F_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(59.905, 52.945)), module, Palette::NOTE_E_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(53.676, 54.812)), module, Palette::NOTE_DS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(50.585, 54.845)), module, Palette::NOTE_D_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(41.898, 56.26)), module, Palette::NOTE_C_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(44.959, 56.26)), module, Palette::NOTE_CS_LIGHT_LIGHT));

		auto* toneDisplay = createWidget<PaletteToneDisplay>(
			mm2px(Vec(2.66f, 29.766f))
		);
		toneDisplay->box.size = mm2px(Vec(27.656f, 12.169f));
		toneDisplay->module = module;
		addChild(toneDisplay);
	}
};


Model* modelPalette = createModel<Palette, PaletteWidget>("Palette");