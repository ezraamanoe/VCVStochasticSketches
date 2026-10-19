#include "plugin.hpp"
#include <cmath>

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

	Drift() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(CAPTURE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(RECALL_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MODE_PARAM, 0.f, 1.f, 0.f, "");
		configParam(LENGTH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(DRIFT_PARAM_PARAM, 0.f, 1.f, 0.f, "");
		configParam(OCTAVES_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SEED_PARAM, 0.f, 1.f, 0.f, "");
		configInput(CLOCK_INPUT, "");
		configInput(RESET_INPUT, "");
		configInput(PITCH_INPUT, "");
		configInput(GATE_INPUT, "");
		configInput(PALETTE_LINK_INPUT, "");
		configInput(DRIFT_CV_INPUT, "");
		configOutput(PITCH_OUTPUT, "");
		configOutput(GATE_OUTPUT, "");
		configOutput(CHANGE_OUTPUT, "");
		configLight(CAPTURE_LIGHT, "Capture status");
		configLight(PHRASE_LIGHT, "Phrase available");
	}

	void process(const ProcessArgs& args) override {
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

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.041, 13.683)), module, Drift::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(62.688, 13.683)), module, Drift::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(73.336, 13.683)), module, Drift::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(83.983, 13.683)), module, Drift::GATE_INPUT));
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