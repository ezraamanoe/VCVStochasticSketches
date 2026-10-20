#include "plugin.hpp"
#include <cmath>
#include <algorithm>

struct Drift : Module {
	enum ParamId {
		CAPTURE_PARAM,
		RECALL_PARAM,
		MODE_PARAM,
		LENGTH_PARAM,
		DRIFT_PARAM,
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

	// This is the phrase that Drift actually plays and will later mutate.
	float workingPitch[PORT_MAX_CHANNELS][kMaxSteps] = {};
	bool workingGate[PORT_MAX_CHANNELS][kMaxSteps] = {};

	random::Xoroshiro128Plus rngs[PORT_MAX_CHANNELS];
	dsp::PulseGenerator changePulses[PORT_MAX_CHANNELS];
	int currentSeed = -1;

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
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.25f, "Mutation chance", "%", 0.f, 100.f);
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

	static uint64_t mixSeed(uint64_t value) {
		value += 0x9e3779b97f4a7c15ULL;
		value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
		value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
		return value ^ (value >> 31);
	}

	void reseedVoice(int channel, int seed) {
		uint64_t source = static_cast<uint64_t>(seed)
			+ static_cast<uint64_t>(channel) * 0x9e3779b9ULL;

		rngs[channel].seed(
			mixSeed(source),
			mixSeed(source + 0x6a09e667f3bcc909ULL)
		);
	}

	void reseedAll(int seed) {
		for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
			reseedVoice(channel, seed);
		}
	}

	float nextUniform(int channel) {
		return static_cast<float>(rngs[channel]() >> 40)
			* (1.f / 16777216.f);
	}

	void recallVoice(int channel) {
		if (!phraseReady[channel]) {
			return;
		}

		for (int step = 0; step < phraseLength[channel]; step++) {
			workingPitch[channel][step] = capturedPitch[channel][step];
			workingGate[channel][step] = capturedGate[channel][step];
		}

		playbackStep[channel] = 0;
		currentStep[channel] = -1;
		heldGate[channel] = false;

		reseedVoice(channel, currentSeed);
	}

	static int positiveModulo(int value, int divisor) {
		int result = value % divisor;
		return result < 0 ? result + divisor : result;
	}

	static int voltageToSemitone(float voltage) {
		return (int) std::round(voltage * 12.f);
	}

	static float semitoneToVoltage(int semitone) {
		return semitone / 12.f;
	}

	int chooseCapturedDegree(int channel) {
		int availableDegrees[kMaxSteps];
		int degreeCount = 0;

		for (int step = 0; step < phraseLength[channel]; step++) {
			if (capturedGate[channel][step]) {
				int semitone = voltageToSemitone(capturedPitch[channel][step]);
				availableDegrees[degreeCount++] = positiveModulo(semitone, 12);
			}
		}

		if (degreeCount == 0)
			return 0;

		int index = std::min(
			(int) (nextUniform(channel) * degreeCount),
			degreeCount - 1
		);

		return availableDegrees[index];
	}

	int choosePaletteDegree(int channel) {
		if (!inputs[PALETTE_LINK_INPUT].isConnected()) {
			return chooseCapturedDegree(channel);
		}

		float totalWeight = 0.f;
		float weights[12] = {};
		int linkChannels = inputs[PALETTE_LINK_INPUT].getChannels();

		for (int degree = 0; degree < 12; degree++) {
			if (degree < linkChannels) {
				weights[degree] = clamp(
					inputs[PALETTE_LINK_INPUT].getVoltage(degree) / 10.f,
					0.f,
					1.f
				);
				totalWeight += weights[degree];
			}
		}

		if (totalWeight <= 0.f) {
			return chooseCapturedDegree(channel);
		}

		float choice = nextUniform(channel) * totalWeight;

		for (int degree = 0; degree < 12; degree++) {
			choice -= weights[degree];

			if (choice <= 0.f) {
				return degree;
			}
		}

		return 11;
	}

	void mutatePhrase(int channel) {
		if (!phraseReady[channel])
			return;

		float chance = params[DRIFT_PARAM].getValue()
			+ inputVoltage(DRIFT_CV_INPUT, channel) / 10.f;
		chance = clamp(chance, 0.f, 1.f);

		if (nextUniform(channel) >= chance)
			return;

		// Select one active note in the phrase to change.
		int activeSteps[kMaxSteps];
		int activeCount = 0;

		for (int step = 0; step < phraseLength[channel]; step++) {
			if (workingGate[channel][step]) {
				activeSteps[activeCount++] = step;
			}
		}

		if (activeCount == 0)
			return;

		int chosenStep = activeSteps[
			std::min(
				(int) (nextUniform(channel) * activeCount),
				activeCount - 1
			)
		];

		int mode = (int) std::round(params[MODE_PARAM].getValue());
		bool changeDegree = mode == 1 || mode == 2;
		bool changeOctave = mode == 0 || mode == 2;

		// Always use the original capture as the reference point.
		int originalSemitone = voltageToSemitone(
			capturedPitch[channel][chosenStep]
		);
		int degree = positiveModulo(originalSemitone, 12);
		int octave = (int) std::floor(originalSemitone / 12.f);

		// Palette mode chooses a degree from Palette Link, or from the
		// captured phrase itself if no Palette Link cable is connected.
		if (changeDegree) {
			degree = choosePaletteDegree(channel);
		}

		// Octave mode moves up or down by 1–N octaves.
		int maxOctaves = (int) std::round(
			params[OCTAVES_PARAM].getValue()
		);

		if (changeOctave && maxOctaves > 0) {
			int direction = nextUniform(channel) < 0.5f ? -1 : 1;
			int distance = 1 + std::min(
				(int) (nextUniform(channel) * maxOctaves),
				maxOctaves - 1
			);

			octave += direction * distance;
		}

		workingPitch[channel][chosenStep] =
			semitoneToVoltage(octave * 12 + degree);

		// 10 ms is long enough for the LED to be visible.
		changePulses[channel].trigger(0.01f);
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

		int seed = static_cast<int>(std::round(params[SEED_PARAM].getValue()));

		if (seed != currentSeed) {
			currentSeed = seed;
			reseedAll(seed);
		}
	
		// Capture arms every possible polyphonic voice. Each active voice begins
		// recording on its own next clock edge.
		if (captureTrigger.process(params[CAPTURE_PARAM].getValue())) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				capturePending[channel] = true;
			}
		}
	
		if (recallTrigger.process(params[RECALL_PARAM].getValue())) {
			for (int channel = 0; channel < PORT_MAX_CHANNELS; channel++) {
				recallVoice(channel);
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

				workingPitch[channel][step] = capturePitch[channel];
				workingGate[channel][step] = captureGateSeen[channel];
	
				captureStep[channel]++;
	
				if (captureStep[channel] >= phraseLength[channel]) {
					capturing[channel] = false;
					phraseReady[channel] = true;
					playbackStep[channel] = 0;
					currentStep[channel] = -1;
					reseedVoice(channel, currentSeed);
				}
				else {
					capturePitch[channel] = incomingPitch;
					captureGateSeen[channel] = false;
				}
			}
	
			// Once the phrase exists, read the next stored event at each clock edge.
			if (risingEdge && phraseReady[channel] && !capturing[channel]) {
				int step = playbackStep[channel] % phraseLength[channel];
	
				heldPitch[channel] = workingPitch[channel][step];
				heldGate[channel] = workingGate[channel][step];
				currentStep[channel] = step;
	
				playbackStep[channel]++;

				if (playbackStep[channel] >= phraseLength[channel]) {
					playbackStep[channel] = 0;
					mutatePhrase(channel);
				}
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
	
			// Send a short trigger whenever a mutation occurs.
			float changeVoltage = changePulses[channel].process(args.sampleTime) ? 10.f : 0.f;
			outputs[CHANGE_OUTPUT].setVoltage(changeVoltage, channel);

			if (channel == 0)
				lights[CHANGE_LIGHT].setBrightness(changeVoltage > 0.f ? 1.f : 0.f);
	
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
			module, Drift::DRIFT_PARAM
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

		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(12.84, 40.805)), module, Drift::STEP_1_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(22.167, 40.805)), module, Drift::STEP_2_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(31.494, 40.805)), module, Drift::STEP_3_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(40.821, 40.805)), module, Drift::STEP_4_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(50.148, 40.805)), module, Drift::STEP_5_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(59.475, 40.805)), module, Drift::STEP_6_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(68.803, 40.805)), module, Drift::STEP_7_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(78.13, 40.805)), module, Drift::STEP_8_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(87.128, 52.755)), module, Drift::CHANGE_LIGHT));
	}
};


Model* modelDrift = createModel<Drift, DriftWidget>("Drift");