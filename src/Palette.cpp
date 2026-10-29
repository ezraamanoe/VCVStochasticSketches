#include "plugin.hpp"
#include <cmath>

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
		// Keep new parameters at the end so existing saved Palette patches
		// keep their original Density, Seed, and note-slider values.
		MEMORY_PARAM,
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
		PALETTE_LINK_OUTPUT,
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
		MEMORY_1_LIGHT,
		MEMORY_2_LIGHT,
		MEMORY_3_LIGHT,
		MEMORY_4_LIGHT,
		MEMORY_5_LIGHT,
		MEMORY_6_LIGHT,
		MEMORY_7_LIGHT,
		MEMORY_8_LIGHT,
		LIGHTS_LEN
	};

	static constexpr int MAX_VOICES = 16;
	static constexpr int MEMORY_STEPS = 8;

	dsp::SchmittTrigger clockTriggers[MAX_VOICES];
	dsp::SchmittTrigger resetTriggers[MAX_VOICES];
	int selectedNotes[MAX_VOICES] = {};
	bool noteActive[MAX_VOICES] = {};
	int memoryPitches[MAX_VOICES][MEMORY_STEPS] = {};
	bool memoryGates[MAX_VOICES][MEMORY_STEPS] = {};
	bool memoryValid[MAX_VOICES][MEMORY_STEPS] = {};
	int memoryPositions[MAX_VOICES] = {};
	int currentMemorySteps[MAX_VOICES] = {};
	random::Xoroshiro128Plus rngs[MAX_VOICES];
	int currentSeed = -1;
	
	float nextUniform(int voice) {
		// Converts this voice's random number into a value from 0.0 to < 1.0.
		return static_cast<float>(rngs[voice]() >> 40) * (1.f / 16777216.f);
	}
	
	void reseedVoice(int voice, int seed) {
		uint64_t baseSeed = static_cast<uint64_t>(seed);
		uint64_t voiceNumber = static_cast<uint64_t>(voice + 1);
	
		rngs[voice].seed(
			baseSeed + 0x9e3779b97f4a7c15ULL * voiceNumber,
			(baseSeed ^ 0xd1b54a32d192ed03ULL)
				+ 0x94d049bb133111ebULL * voiceNumber
		);
	}

	void clearMemoryVoice(int voice) {
		memoryPositions[voice] = 0;
		currentMemorySteps[voice] = 0;

		for (int step = 0; step < MEMORY_STEPS; step++) {
			memoryPitches[voice][step] = 0;
			memoryGates[voice][step] = false;
			memoryValid[voice][step] = false;
		}
	}
	
	void reseedAll(int seed) {
		for (int voice = 0; voice < MAX_VOICES; voice++) {
			reseedVoice(voice, seed);
			selectedNotes[voice] = 0;
			noteActive[voice] = false;
			clearMemoryVoice(voice);
		}
	}

	bool chooseNote(int voice) {
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
			return false;
		}

		float target = nextUniform(voice) * totalWeight;
		float accumulatedWeight = 0.f;

		for (int note = 0; note < 12; note++) {
			accumulatedWeight += params[noteParams[note]].getValue();

			if (target <= accumulatedWeight) {
				selectedNotes[voice] = note;
				return true;
			}
		}

		return false;
	}

	Palette() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(DENSITY_PARAM, 0.f, 1.f, 1.f, "Density");
		configParam(SEED_PARAM, 1.f, 999.f, 1.f, "Seed");
		getParamQuantity(SEED_PARAM)->snapEnabled = true;
		configParam(NOTE_CS_PARAM, 0.f, 1.f, 0.f, "C#");
		configParam(NOTE_DS_PARAM, 0.f, 1.f, 0.f, "D#");
		configParam(NOTE_FS_PARAM, 0.f, 1.f, 0.f, "F#");
		configParam(NOTE_GS_PARAM, 0.f, 1.f, 0.f, "G#");
		configParam(NOTE_AS_PARAM, 0.f, 1.f, 0.f, "A#");
		configParam(NOTE_C_PARAM, 0.f, 1.f, 0.f, "C");
		configParam(NOTE_D_PARAM, 0.f, 1.f, 0.f, "D");
		configParam(NOTE_E_PARAM, 0.f, 1.f, 0.f, "E");
		configParam(NOTE_F_PARAM, 0.f, 1.f, 0.f, "F");
		configParam(NOTE_G_PARAM, 0.f, 1.f, 0.f, "G");
		configParam(NOTE_A_PARAM, 0.f, 1.f, 0.f, "A");
		configParam(NOTE_B_PARAM, 0.f, 1.f, 0.f, "B");
		configParam(MEMORY_PARAM, 0.f, 1.f, 0.f, "Memory", "%", 0.f, 100.f);
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configOutput(PITCH_OUTPUT, "Pitch (1 V/oct)");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(PALETTE_LINK_OUTPUT, "Palette Link note weights");
	}

	void process(const ProcessArgs& args) override {
		int seed = static_cast<int>(std::round(params[SEED_PARAM].getValue()));

		if (seed != currentSeed) {
		currentSeed = seed;
		reseedAll(seed);
		}

		const int noteParams[12] = {
			NOTE_C_PARAM, NOTE_CS_PARAM, NOTE_D_PARAM, NOTE_DS_PARAM,
			NOTE_E_PARAM, NOTE_F_PARAM, NOTE_FS_PARAM, NOTE_G_PARAM,
			NOTE_GS_PARAM, NOTE_A_PARAM, NOTE_AS_PARAM, NOTE_B_PARAM
		};

		// One output port carries 12 polyphonic channels: C through B.
		outputs[PALETTE_LINK_OUTPUT].setChannels(12);

		for (int note = 0; note < 12; note++) {
			float weight = clamp(params[noteParams[note]].getValue(), 0.f, 1.f);
			outputs[PALETTE_LINK_OUTPUT].setVoltage(weight * 10.f, note);
		}

		int channels = inputs[CLOCK_INPUT].getChannels();
		if (channels < 1) {
			channels = 1;
		}

		outputs[PITCH_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);
		float memoryAmount = params[MEMORY_PARAM].getValue();

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
				reseedVoice(voice, currentSeed);
				selectedNotes[voice] = 0;
				noteActive[voice] = false;
				clearMemoryVoice(voice);
			}

			if (clockTriggers[voice].process(clockVoltage)) {
				int step = memoryPositions[voice];
				bool reuseStoredEvent = false;

				// At 0%, no extra random number is used. This preserves Palette's
				// original random behaviour exactly when Memory is fully off.
				if (memoryValid[voice][step] && memoryAmount > 0.f) {
					reuseStoredEvent = memoryAmount >= 1.f
						|| nextUniform(voice) < memoryAmount;
				}

				if (reuseStoredEvent) {
					selectedNotes[voice] = memoryPitches[voice][step];
					noteActive[voice] = memoryGates[voice][step];
				}
				else {
					noteActive[voice] = nextUniform(voice) < params[DENSITY_PARAM].getValue();

					if (noteActive[voice] && !chooseNote(voice)) {
						noteActive[voice] = false;
					}

					memoryPitches[voice][step] = selectedNotes[voice];
					memoryGates[voice][step] = noteActive[voice];
					memoryValid[voice][step] = true;
				}

				currentMemorySteps[voice] = step;
				memoryPositions[voice] = (step + 1) % MEMORY_STEPS;
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

		for (int step = 0; step < MEMORY_STEPS; step++) {
			float brightness = 0.f;
			if (memoryValid[0][step]) {
				brightness = currentMemorySteps[0] == step ? 1.f : 0.15f;
			}
			lights[MEMORY_1_LIGHT + step].setBrightness(brightness);
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

struct PaletteSeedDisplay : widget::TransparentWidget {
	Palette* module = nullptr;

	PaletteSeedDisplay() {
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

		int seed = module ? static_cast<int>(std::round(module->params[Palette::SEED_PARAM].getValue())) : -1;
		std::string value = seed >= 0 ? string::f("%03d", seed) : "---";

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
	
	bool keyboardHeld[12] = {};

	static int keyToNote(int key) {
		switch (key) {
			case GLFW_KEY_A: return 0;   // C
			case GLFW_KEY_W: return 1;   // C#
			case GLFW_KEY_S: return 2;   // D
			case GLFW_KEY_E: return 3;   // D#
			case GLFW_KEY_D: return 4;   // E
			case GLFW_KEY_F: return 5;   // F
			case GLFW_KEY_T: return 6;   // F#
			case GLFW_KEY_G: return 7;   // G
			case GLFW_KEY_Y: return 8;   // G#
			case GLFW_KEY_H: return 9;   // A
			case GLFW_KEY_U: return 10;  // A#
			case GLFW_KEY_J: return 11;  // B
			default: return -1;
		}
	}

	static int noteParamId(int note) {
	static const int noteParams[12] = {
		Palette::NOTE_C_PARAM,
		Palette::NOTE_CS_PARAM,
		Palette::NOTE_D_PARAM,
		Palette::NOTE_DS_PARAM,
		Palette::NOTE_E_PARAM,
		Palette::NOTE_F_PARAM,
		Palette::NOTE_FS_PARAM,
		Palette::NOTE_G_PARAM,
		Palette::NOTE_GS_PARAM,
		Palette::NOTE_A_PARAM,
		Palette::NOTE_AS_PARAM,
		Palette::NOTE_B_PARAM
	};
		return noteParams[note];
	}

	bool hasHeldKeys() const {
		for (bool held : keyboardHeld) {
			if (held) {
				return true;
			}
		}
		return false;
	}

	void setKeyboardNote(int note, float value) {
		if (!module || note < 0 || note >= 12) {
			return;
		}

		module->getParamQuantity(noteParamId(note))->setValue(value);
	}

	void onHoverKey(const HoverKeyEvent& e) override {
		ModuleWidget::onHoverKey(e);

		if (e.isConsumed() || !module || e.mods != 0) {
			return;
		}

		int note = keyToNote(e.key);
		if (note < 0) {
			return;
		}

		if (e.action == GLFW_PRESS) {
			// No keys held means this is the first note of a new chord.
			// Replace the old Palette, then add this note.
			bool newChord = !hasHeldKeys();

			if (newChord) {
				for (int i = 0; i < 12; i++) {
					setKeyboardNote(i, 0.f);
				}
			}

			keyboardHeld[note] = true;
			setKeyboardNote(note, 1.f);
		}
		else if (e.action == GLFW_RELEASE) {
			// Releasing keys does not change Palette.
			// The captured chord remains active.
			keyboardHeld[note] = false;
		}
		else {
			return;
		}

		e.consume(this);
	}

	void onLeave(const LeaveEvent& e) override {
		// Forget physical held keys without changing the stored chord.
		for (int note = 0; note < 12; note++) {
			keyboardHeld[note] = false;
		}

		ModuleWidget::onLeave(e);
	}

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

		// Main controls: Memory, Seed, and Density share the same orange knob.
		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(96.411f, 80.074f)), module, Palette::MEMORY_PARAM));
		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(110.465f, 80.074f)), module, Palette::SEED_PARAM));
		addParam(createParamCentered<PaletteDensityKnob>(mm2px(Vec(124.519f, 80.074f)), module, Palette::DENSITY_PARAM));

		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(16.159f, 80.301f)), module, Palette::NOTE_CS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(27.368f, 80.196f)), module, Palette::NOTE_DS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(49.787f, 80.196f)), module, Palette::NOTE_FS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(60.996f, 80.196f)), module, Palette::NOTE_GS_PARAM));
		addParam(createParamCentered<PaletteSharpKeySlider>(mm2px(Vec(72.383f, 80.110f)), module, Palette::NOTE_AS_PARAM));

		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(10.526f, 106.486f)), module, Palette::NOTE_C_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(21.792f, 106.486f)), module, Palette::NOTE_D_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(33.057f, 106.486f)), module, Palette::NOTE_E_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(44.323f, 106.486f)), module, Palette::NOTE_F_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(55.589f, 106.486f)), module, Palette::NOTE_G_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(66.854f, 106.486f)), module, Palette::NOTE_A_PARAM));
		addParam(createParamCentered<PaletteWhiteKeySlider>(mm2px(Vec(77.657f, 106.486f)), module, Palette::NOTE_B_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(133.617f, 13.236f)), module, Palette::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(144.243f, 13.236f)), module, Palette::RESET_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(143.479f, 71.239f)), module, Palette::PITCH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(143.479f, 86.447f)), module, Palette::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(143.479f, 102.738f)), module, Palette::PALETTE_LINK_OUTPUT));

		// Eight slots: off means empty, dim means stored, and bright means current.
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(14.710f, 52.755f)), module, Palette::MEMORY_1_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(19.709f, 52.464f)), module, Palette::MEMORY_2_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(24.709f, 52.464f)), module, Palette::MEMORY_3_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(29.709f, 52.518f)), module, Palette::MEMORY_4_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(34.708f, 52.518f)), module, Palette::MEMORY_5_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(39.708f, 52.518f)), module, Palette::MEMORY_6_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(44.708f, 52.518f)), module, Palette::MEMORY_7_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(49.707f, 52.518f)), module, Palette::MEMORY_8_LIGHT));

		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(132.853f, 41.938f)), module, Palette::NOTE_B_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(127.773f, 44.424f)), module, Palette::NOTE_AS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(124.250f, 44.503f)), module, Palette::NOTE_A_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(115.203f, 47.333f)), module, Palette::NOTE_G_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(118.582f, 47.361f)), module, Palette::NOTE_GS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(109.216f, 50.148f)), module, Palette::NOTE_FS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(105.840f, 50.226f)), module, Palette::NOTE_F_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(101.657f, 52.903f)), module, Palette::NOTE_E_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(94.671f, 54.769f)), module, Palette::NOTE_DS_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(91.204f, 54.803f)), module, Palette::NOTE_D_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(84.503f, 56.128f)), module, Palette::NOTE_C_LIGHT_LIGHT));
		addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(81.503f, 56.128f)), module, Palette::NOTE_CS_LIGHT_LIGHT));

		auto* seedDisplay = createWidget<PaletteSeedDisplay>(
			mm2px(Vec(18.623f, 30.061f))
		);
		seedDisplay->box.size = mm2px(Vec(27.656f, 12.169f));
		seedDisplay->module = module;
		addChild(seedDisplay);
	}
};

Model* modelPalette = createModel<Palette, PaletteWidget>("Palette");
