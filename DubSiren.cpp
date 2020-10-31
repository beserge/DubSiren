#include "daisysp.h"
#include "daisy_petal.h"

#define MAX_DELAY static_cast<size_t>(48000 * 2.5f)

using namespace daisysp;
using namespace daisy;


static DaisyPetal petal;

Oscillator Osc, Lfo;

static DelayLine <float, MAX_DELAY>  DSY_SDRAM_BSS delline;
static Parameter params[6];

struct FiltStruct{
	Svf filt;
	
	bool isLowpass; //else highpass
	
	void Init(float samplerate){
		filt.Init(samplerate);
		filt.SetRes(.1f);
		filt.SetDrive(.1f);
	}
	
	void SetFreq(float freq){
		filt.SetFreq(freq);
	}
	
	float Process(float in){
		filt.Process(in);
		return (isLowpass ? filt.Low() : filt.High());
	}
};

FiltStruct Filt;

struct DelStruct{
	void Init(){
		delline.Init();
	}
	
	float slewRate = .0002f;  //a magic number
	float targetRate, currentRate;
	
	float fdbk;
	
	void Update(float delRate, float feedback){
		targetRate = delRate;
		fdbk = feedback;
	}
	
	float Process(float in){
		fonepole(currentRate, targetRate, slewRate);
		delline.SetDelay(currentRate);
		
		float out = delline.Read();
		out = Filt.Process(out);
		delline.Write((fdbk * out) + in);
		return (fdbk * out) + ((1.0f - fdbk) * in);
		
    }

};

DelStruct Del;

enum paramNames{
	lfoRate,
	lfoDepth,
	oscPitch,
	delRate,
	delFdbk,
	cutoff,
	FINAL,
};

bool regOn, lfoOn = false;
float baseFreq;
float volume;

void UpdateControls(){
	petal.UpdateAnalogControls();
	petal.DebounceControls();
	
	//regular
	regOn = petal.switches[0].Pressed();
	//lfo
	lfoOn = petal.switches[1].Pressed();
	//filter type
	Filt.isLowpass = petal.switches[6].Pressed();

	Lfo.SetFreq(params[lfoRate].Process());
	Lfo.SetAmp(params[lfoDepth].Process());
	baseFreq = params[oscPitch].Process();
	Del.Update(params[delRate].Process(), params[delFdbk].Process());
	Filt.SetFreq(params[cutoff].Process());

	if(petal.encoder.Increment() == 1){
		petal.SetFootswitchLed((DaisyPetal::FootswitchLed)0, 1);
	}

	volume += .05f * petal.encoder.Increment();
	volume = volume > 1.f ? 1.f : volume;
	volume = volume < 0.f ? 0.f : volume;
}


static void AudioCallback(float **in, float **out, size_t size)
{
	UpdateControls();
		
    for (size_t i = 0; i < size; i++)
    {
		float lfoVal = Lfo.Process();
		lfoVal *= lfoOn;
		
		Osc.SetFreq(lfoVal + baseFreq);
		float oscOut = Osc.Process();
		oscOut *= (regOn | lfoOn);
		
		float delOut = Del.Process(oscOut);
		
		out[0][i] = out[1][i] = delOut * volume;
	}
}


void UpdateLeds(){
	petal.ClearLeds();

	//footswitch leds
	petal.SetFootswitchLed((DaisyPetal::FootswitchLed)0, petal.switches[0].Pressed());
	petal.SetFootswitchLed((DaisyPetal::FootswitchLed)1, petal.switches[1].Pressed());
	
	//ring leds
	int32_t whole;
    float   frac;
    whole = (int32_t)(volume / .125f);
    frac  = volume / .125f - whole;
   
    // Set full bright
    for(int i = 0; i < whole; i++)
    {
        petal.SetRingLed(static_cast<DaisyPetal::RingLed>(i), 0, 1, 0);
    }

    // Set Frac
    if(whole < 7 && whole > 0)
	{
        petal.SetRingLed(static_cast<DaisyPetal::RingLed>(whole - 1), 0, 1 * frac, 0);
    }

	petal.UpdateLeds();
}

int main(void)
{
    float samplerate;
    petal.Init(); // Initialize hardware (daisy seed, and petal)
    samplerate = petal.AudioSampleRate();

	Del.Init();

	Osc.Init(samplerate);
	Osc.SetAmp(1);
	Osc.SetWaveform(Osc.WAVE_SQUARE);

	Lfo.Init(samplerate);
	Lfo.SetWaveform(Osc.WAVE_SIN);

	Filt.Init(samplerate);
	
	volume = .5f;
		
	params[lfoRate].Init(petal.knob[0], .25, 15, Parameter::LOGARITHMIC);
	params[lfoDepth].Init(petal.knob[1], 0, 500, Parameter::LINEAR);
	params[oscPitch].Init(petal.knob[2], 50, 5000, Parameter::LOGARITHMIC);
	params[delRate].Init(petal.knob[3], samplerate * .05, MAX_DELAY, Parameter::LOGARITHMIC);
	params[delFdbk].Init(petal.knob[4], 0, 1, Parameter::LINEAR);
	params[cutoff].Init(petal.knob[5], 20, 20000, Parameter::LOGARITHMIC);

    petal.StartAdc();
    petal.StartAudio(AudioCallback);
    while(1) 
    {
		UpdateLeds();
		petal.DelayMs(6);
	}
}