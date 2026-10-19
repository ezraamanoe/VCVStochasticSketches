#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Drift : Module {
	enum ParamId {
		CAPTURE_PARAM,
		RECALL_PARAM,
		MODE_PARAM,
		LENGTH_PARAM,
		DRIFT_PARAM_PARAM,
		OCTAVES_PARAM,
		SEED_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		PITCH_INPUT,
		GATE_INPUT,
		PALETTE_LINK_INPUT,
		DRIFT_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		PITCH_OUTPUT,
		GATE_OUTPUT,
		CHANGE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		CAPTURE_LIGHT,
		PHRASE_LIGHT,
		STEP_1_LIGHT,
		STEP_2_LIGHT,
		STEP_3_LIGHT,
		STEP_4_LIGHT,
		STEP_5_LIGHT,
		STEP_6_LIGHT,
		STEP_7_LIGHT,
		STEP_8_LIGHT,
		CHANGE_LIGHT,
		LIGHTS_LEN
	};

	static constexpr int kMaxSteps = 8;

	// One captured phrase per polyphonic channel.
	float capturedPitch[PORT_MAX_CHANNELS][kMaxSteps] = {};
	bool capturedGate[PORT_MAX_CHANNELS][kMaxSteps] = {};

	int phraseLength[PORT_MAX_CHANNELS] = {};
	int captureStep[PORT_MAX_CHANNELS] = {};
	int playbackStep[PORT_MAX_CHANNELS] = {};

	bool capturePending[PORT_MAX_CHANNELS] = {};
	bool capturing[PORT_MAX_CHANNELS] = {};
	bool phraseReady[PORT_MAX_CHANNELS] = {};

	// These watch a whole clock-high phase. This matters because Palette may
	// update its gate a fraction after Drift sees the shared clock edge.
	float capturePitch[PORT_MAX_CHANNELS] = {};
	bool captureGateSeen[PORT_MAX_CHANNELS] = {};
	bool lastClockHigh[PORT_MAX_CHANNELS] = {};

	int currentStep[PORT_MAX_CHANNELS] = {};

	// Values currently being played by the looping phrase.
	float heldPitch[PORT_MAX_CHANNELS] = {};
	bool heldGate[PORT_MAX_CHANNELS] = {};

	dsp::SchmittTrigger captureTrigger;
	dsp::SchmittTrigger resetTriggers[PORT_MAX_CHANNELS];
	dsp::SchmittTrigger recallTrigger;

	Drift() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(CAPTURE_PARAM, "Capture phrase");
		configButton(RECALL_PARAM, "Recall captured phrase");
		configSwitch(MODE_PARAM, 0.f, 2.f, 2.f, "Mutation mode", {
			"Octave: keep the note degree",
			"Palette: choose another allowed note",
			"Both: palette note and octave jump"
		});
		configParam(LENGTH_PARAM, 1.f, 8.f, 8.f, "Capture length", " steps");
		getParamQuantity(LENGTH_PARAM)->snapEnabled = true;
		configParam(DRIFT_PARAM_PARAM, 0.f, 1.f, 0.25f, "Mutation chance", "%", 0.f, 100.f);
		configParam(OCTAVES_PARAM, 0.f, 2.f, 1.f, "Maximum octave jump", " octaves");
		getParamQuantity(OCTAVES_PARAM)->snapEnabled = true;
		configParam(SEED_PARAM, 1.f, 999.f, 1.f, "Seed");
		getParamQuantity(SEED_PARAM)->snapEnabled = true;
		configInput(CLOCK_INPUT, "Clock (polyphonic)");
		configInput(RESET_INPUT, "Reset");
		configInput(PITCH_INPUT, "Pitch (polyphonic)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(PALETTE_LINK_INPUT, "Palette Link note weights");
		configInput(DRIFT_CV_INPUT, "Mutation chance CV");
		configOutput(PITCH_OUTPUT, "Pitch (polyphonic)");
		configOutput(GATE_OUTPUT, "Gate (polyphonic)");
		configOutput(CHANGE_OUTPUT, "Mutation trigger (polyphonic)");
		configLight(CAPTURE_LIGHT, "Capture status");
		configLight(PHRASE_LIGHT, "Phrase available");
		for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
			currentStep[channel] = -1;
		}
	}

	float inputVoltage(int inputId, int channel) {
		int inputChannels = inputs[inputId].getChannels();

		if (inputChannels <= 0) {
			return 0.f;
		}

		// A mono cable is copied to every polyphonic voice.
		return inputs[inputId].getVoltage(
			std::min(channel, inputChannels - 1)
		);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(
			1,
			std::max(
				inputs[CLOCK_INPUT].getChannels(),
				std::max(
					inputs[PITCH_INPUT].getChannels(),
					inputs[GATE_INPUT].getChannels()
				)
			)
		);
	
		outputs[PITCH_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);
		outputs[CHANGE_OUTPUT].setChannels(channels);
	
		int captureLength = std::max(
			1,
			std::min(
				kMaxSteps,
				static_cast<int>(std::round(params[LENGTH_PARAM].getValue()))
			)
		);
	
		// Capture arms every possible polyphonic voice. Each active voice begins
		// recording on its own next clock edge.
		if (captureTrigger.process(params[CAPTURE_PARAM].getValue())) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				capturePending[channel] = true;
			}
		}
	
		// For now Recall simply rewinds the stored phrase. Later, once mutation
		// exists, it will also restore the untouched original phrase.
		if (recallTrigger.process(params[RECALL_PARAM].getValue())) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				playbackStep[channel] = 0;
				currentStep[channel] = -1;
			}
		}
	
		for (int channel = 0; channel < channels; channel++) {
			float clockVoltage = inputVoltage(CLOCK_INPUT, channel);
			bool clockHigh = clockVoltage >= 1.f;
			bool risingEdge = clockHigh && !lastClockHigh[channel];
			bool fallingEdge = !clockHigh && lastClockHigh[channel];
	
			float incomingPitch = inputVoltage(PITCH_INPUT, channel);
			bool incomingGate = inputVoltage(GATE_INPUT, channel) >= 1.f;
	
			if (resetTriggers[channel].process(inputVoltage(RESET_INPUT, channel))) {
				playbackStep[channel] = 0;
				currentStep[channel] = -1;
			}
	
			// Start recording on a clock edge, rather than halfway through a step.
			if (risingEdge && capturePending[channel]) {
				capturePending[channel] = false;
				capturing[channel] = true;
				phraseReady[channel] = false;
	
				phraseLength[channel] = captureLength;
				captureStep[channel] = 0;
				playbackStep[channel] = 0;
				currentStep[channel] = -1;
	
				capturePitch[channel] = incomingPitch;
				captureGateSeen[channel] = incomingGate;
			}
	
			// Watch the complete high phase of the clock. This allows Drift to
			// capture Palette correctly even if Palette updates after Drift.
			if (capturing[channel] && clockHigh) {
				capturePitch[channel] = incomingPitch;
				captureGateSeen[channel] =
					captureGateSeen[channel] || incomingGate;
			}
	
			// Store the step only after the clock pulse finishes.
			if (capturing[channel] && fallingEdge) {
				int step = captureStep[channel];
	
				capturedPitch[channel][step] = capturePitch[channel];
				capturedGate[channel][step] = captureGateSeen[channel];
	
				captureStep[channel]++;
	
				if (captureStep[channel] >= phraseLength[channel]) {
					capturing[channel] = false;
					phraseReady[channel] = true;
					playbackStep[channel] = 0;
					currentStep[channel] = -1;
				}
				else {
					capturePitch[channel] = incomingPitch;
					captureGateSeen[channel] = false;
				}
			}
	
			// Once the phrase exists, read the next stored event at each clock edge.
			if (risingEdge && phraseReady[channel] && !capturing[channel]) {
				int step = playbackStep[channel] % phraseLength[channel];
	
				heldPitch[channel] = capturedPitch[channel][step];
				heldGate[channel] = capturedGate[channel][step];
				currentStep[channel] = step;
	
				playbackStep[channel] =
					(playbackStep[channel] + 1) % phraseLength[channel];
			}
	
			// While recording, Drift passes the original sequence through.
			// Once ready, it outputs the stored looping phrase.
			if (phraseReady[channel] && !capturing[channel]) {
				outputs[PITCH_OUTPUT].setVoltage(heldPitch[channel], channel);
				outputs[GATE_OUTPUT].setVoltage(
					clockHigh && heldGate[channel] ? 10.f : 0.f,
					channel
				);
			}
			else {
				outputs[PITCH_OUTPUT].setVoltage(incomingPitch, channel);
				outputs[GATE_OUTPUT].setVoltage(
					incomingGate ? 10.f : 0.f,
					channel
				);
			}
	
			// Mutation does not exist yet.
			outputs[CHANGE_OUTPUT].setVoltage(0.f, channel);
	
			lastClockHigh[channel] = clockHigh;
		}
	
		// The interface shows the status of voice 1.
		lights[CAPTURE_LIGHT].setBrightnessSmooth(
			capturePending[0] || capturing[0] ? 1.f : 0.f,
			args.sampleTime
		);
		lights[PHRASE_LIGHT].setBrightnessSmooth(
			phraseReady[0] ? 1.f : 0.f,
			args.sampleTime
		);
	
		for (int step = 0; step < kMaxSteps; step++) {
			bool selected = phraseReady[0] && currentStep[0] == step;
			lights[STEP_1_LIGHT + step].setBrightness(
				selected ? 1.f : 0.f
			);
		}
	
		lights[CHANGE_LIGHT].setBrightness(0.f);
	}
};

struct DriftArtwork : widget::TransparentWidget {
	std::string imagePath;

	DriftArtwork() {
		imagePath = asset::plugin(pluginInstance, "res/DriftBase.png");
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

struct DriftPaletteKnob : app::SvgKnob {
	DriftPaletteKnob() {
		minAngle = -0.83f * M_PI;
		maxAngle = 0.83f * M_PI;
		setSvg(window::Svg::load(
			asset::plugin(pluginInstance, "res/PaletteDensityKnob.svg")
		));
	}
};

struct DriftModeSelector : app::SvgSwitch {
	DriftModeSelector() {
		shadow->opacity = 0.f;
		addFrame(window::Svg::load(
			asset::plugin(pluginInstance, "res/DriftModeSelectorOctave.svg")
		));
		addFrame(window::Svg::load(
			asset::plugin(pluginInstance, "res/DriftModeSelectorPalette.svg")
		));
		addFrame(window::Svg::load(
			asset::plugin(pluginInstance, "res/DriftModeSelectorBoth.svg")
		));
	}
};

struct DriftWidget : ModuleWidget {
	DriftWidget(Drift* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Drift.svg")));
		auto* artwork = new DriftArtwork;
		artwork->box.size = box.size;
		addChildBottom(artwork);

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const Vec captureButtonPos = mm2px(Vec(18.582f, 52.848f));
		const Vec recallButtonPos = mm2px(Vec(38.072f, 52.884f));

		addParam(createParamCentered<LEDButton>(
			captureButtonPos, module, Drift::CAPTURE_PARAM
		));
		addChild(createLightCentered<MediumLight<RedLight>>(
			captureButtonPos, module, Drift::CAPTURE_LIGHT
		));

		addParam(createParamCentered<LEDButton>(
			recallButtonPos, module, Drift::RECALL_PARAM
		));
		addChild(createLightCentered<MediumLight<GreenLight>>(
			recallButtonPos, module, Drift::PHRASE_LIGHT
		));

		addParam(createParam<DriftModeSelector>(
			mm2px(Vec(67.727f, 48.755f)),
			module, Drift::MODE_PARAM
		));

		addParam(createParamCentered<DriftPaletteKnob>(
			mm2px(Vec(18.372f, 75.629f)),
			module, Drift::LENGTH_PARAM
		));
		addParam(createParamCentered<DriftPaletteKnob>(
			mm2px(Vec(36.615f, 75.629f)),
			module, Drift::DRIFT_PARAM_PARAM
		));
		addParam(createParamCentered<DriftPaletteKnob>(
			mm2px(Vec(54.857f, 75.629f)),
			module, Drift::OCTAVES_PARAM
		));
		addParam(createParamCentered<DriftPaletteKnob>(
			mm2px(Vec(73.100f, 75.629f)),
			module, Drift::SEED_PARAM
		));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(52.040f, 13.700f)), module, Drift::CLOCK_INPUT
		));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(62.687f, 13.700f)), module, Drift::RESET_INPUT
		));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(73.335f, 13.700f)), module, Drift::PITCH_INPUT
		));
		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(83.982f, 13.700f)), module, Drift::GATE_INPUT
		));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(63.47, 103.187)), module, Drift::PALETTE_LINK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(80.744, 103.187)), module, Drift::DRIFT_CV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.743, 102.886)), module, Drift::PITCH_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(26.986, 102.886)), module, Drift::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45.228, 102.886)), module, Drift::CHANGE_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(12.84, 40.805)), module, Drift::STEP_1_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(22.167, 40.805)), module, Drift::STEP_2_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(31.494, 40.805)), module, Drift::STEP_3_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(40.821, 40.805)), module, Drift::STEP_4_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(50.148, 40.805)), module, Drift::STEP_5_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(59.475, 40.805)), module, Drift::STEP_6_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(68.803, 40.805)), module, Drift::STEP_7_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(78.13, 40.805)), module, Drift::STEP_8_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(87.128, 52.755)), module, Drift::CHANGE_LIGHT));
	}
};


Model* modelDrift = createModel<Drift, DriftWidget>("Drift");