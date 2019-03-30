#include "plugin.hpp"


struct ADSR : Module {
	enum ParamIds {
		ATTACK_PARAM,
		DECAY_PARAM,
		SUSTAIN_PARAM,
		RELEASE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ATTACK_INPUT,
		DECAY_INPUT,
		SUSTAIN_INPUT,
		RELEASE_INPUT,
		GATE_INPUT,
		TRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENVELOPE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ATTACK_LIGHT,
		DECAY_LIGHT,
		SUSTAIN_LIGHT,
		RELEASE_LIGHT,
		NUM_LIGHTS
	};

	bool decaying = false;
	float env = 0.f;
	dsp::SchmittTrigger trigger;

	ADSR() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ATTACK_PARAM, 0.f, 1.f, 0.5f, "Attack");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay");
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.5f, "Sustain");
		configParam(RELEASE_PARAM, 0.f, 1.f, 0.5f, "Release");
	}

	void process(const ProcessArgs &args) override {
		float attack = clamp(params[ATTACK_PARAM].getValue() + inputs[ATTACK_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float decay = clamp(params[DECAY_PARAM].getValue() + inputs[DECAY_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float sustain = clamp(params[SUSTAIN_PARAM].getValue() + inputs[SUSTAIN_INPUT].getVoltage() / 10.f, 0.f, 1.f);
		float release = clamp(params[RELEASE_PARAM].getValue() + inputs[RELEASE_INPUT].getVoltage() / 10.f, 0.f, 1.f);

		// Gate and trigger
		bool gated = inputs[GATE_INPUT].getVoltage() >= 1.f;
		if (trigger.process(inputs[TRIG_INPUT].getVoltage()))
			decaying = false;

		const float base = 20000.f;
		const float maxTime = 10.f;
		if (gated) {
			if (decaying) {
				// Decay
				if (decay < 1e-4) {
					env = sustain;
				}
				else {
					env += std::pow(base, 1 - decay) / maxTime * (sustain - env) * args.sampleTime;
				}
			}
			else {
				// Attack
				// Skip ahead if attack is all the way down (infinitely fast)
				if (attack < 1e-4) {
					env = 1.f;
				}
				else {
					env += std::pow(base, 1 - attack) / maxTime * (1.01f - env) * args.sampleTime;
				}
				if (env >= 1.f) {
					env = 1.f;
					decaying = true;
				}
			}
		}
		else {
			// Release
			if (release < 1e-4) {
				env = 0.f;
			}
			else {
				env += std::pow(base, 1 - release) / maxTime * (0.f - env) * args.sampleTime;
			}
			decaying = false;
		}

		bool sustaining = isNear(env, sustain, 1e-3);
		bool resting = isNear(env, 0.f, 1e-3);

		outputs[ENVELOPE_OUTPUT].setVoltage(10.f * env);

		// Lights
		lights[ATTACK_LIGHT].value = (gated && !decaying) ? 1.f : 0.f;
		lights[DECAY_LIGHT].value = (gated && decaying && !sustaining) ? 1.f : 0.f;
		lights[SUSTAIN_LIGHT].value = (gated && decaying && sustaining) ? 1.f : 0.f;
		lights[RELEASE_LIGHT].value = (!gated && !resting) ? 1.f : 0.f;
	}
};


struct ADSRWidget : ModuleWidget {
	ADSRWidget(ADSR *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ADSR.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 57), module, ADSR::ATTACK_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 124), module, ADSR::DECAY_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 191), module, ADSR::SUSTAIN_PARAM));
		addParam(createParam<RoundLargeBlackKnob>(Vec(62, 257), module, ADSR::RELEASE_PARAM));

		addInput(createInput<PJ301MPort>(Vec(9, 63), module, ADSR::ATTACK_INPUT));
		addInput(createInput<PJ301MPort>(Vec(9, 129), module, ADSR::DECAY_INPUT));
		addInput(createInput<PJ301MPort>(Vec(9, 196), module, ADSR::SUSTAIN_INPUT));
		addInput(createInput<PJ301MPort>(Vec(9, 263), module, ADSR::RELEASE_INPUT));

		addInput(createInput<PJ301MPort>(Vec(9, 320), module, ADSR::GATE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(48, 320), module, ADSR::TRIG_INPUT));
		addOutput(createOutput<PJ301MPort>(Vec(87, 320), module, ADSR::ENVELOPE_OUTPUT));

		addChild(createLight<SmallLight<RedLight>>(Vec(94, 41), module, ADSR::ATTACK_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(94, 109), module, ADSR::DECAY_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(94, 175), module, ADSR::SUSTAIN_LIGHT));
		addChild(createLight<SmallLight<RedLight>>(Vec(94, 242), module, ADSR::RELEASE_LIGHT));
	}
};


Model *modelADSR = createModel<ADSR, ADSRWidget>("ADSR");
