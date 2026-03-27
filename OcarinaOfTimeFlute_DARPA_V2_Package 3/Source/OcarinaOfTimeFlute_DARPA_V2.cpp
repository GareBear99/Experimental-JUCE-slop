
#include <JuceHeader.h>

/*
    Ocarina of Time Flute — DARPA V2
    --------------------------------
    Upgrades over prototype:
      - Better ocarina / flute tone core
      - Breath envelope and articulation shaping
      - Legato glide
      - Mod wheel vibrato depth control
      - Aftertouch air / expression boost
      - Velocity to tone brightness
      - Stereo air shimmer
      - Hidden unlock:
            Play Zelda's Lullaby -> Epona's Song -> Song of Storms
            to unlock Great Fairy Fountain
      - Info hint includes original sequence clue:
            ↑ ← → ← → ↓

    This is still a single-file JUCE source drop for rapid iteration.
*/

namespace IDs
{
    static constexpr const char* masterGain   = "masterGain";
    static constexpr const char* attackMs     = "attackMs";
    static constexpr const char* releaseMs    = "releaseMs";
    static constexpr const char* vibratoRate  = "vibratoRate";
    static constexpr const char* vibratoDepth = "vibratoDepth";
    static constexpr const char* breath       = "breath";
    static constexpr const char* body         = "body";
    static constexpr const char* reverbMix    = "reverbMix";
    static constexpr const char* stereoWidth  = "stereoWidth";
    static constexpr const char* glideMs      = "glideMs";
    static constexpr const char* expression   = "expression";
    static constexpr const char* songSelect   = "songSelect";
    static constexpr const char* playSong     = "playSong";
}

static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat>(IDs::masterGain,   "Master",        NormalisableRange<float>(-24.0f, 6.0f, 0.01f), -4.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::attackMs,     "Attack",        NormalisableRange<float>(1.0f, 250.0f, 0.01f, 0.35f), 16.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::releaseMs,    "Release",       NormalisableRange<float>(10.0f, 2500.0f, 0.01f, 0.35f), 320.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::vibratoRate,  "Vibrato Rate",  NormalisableRange<float>(0.1f, 9.0f, 0.001f), 5.2f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::vibratoDepth, "Vibrato Depth", NormalisableRange<float>(0.0f, 0.10f, 0.0001f), 0.013f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::breath,       "Breath",        NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.28f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::body,         "Body",          NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.68f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::reverbMix,    "Reverb",        NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.24f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::stereoWidth,  "Stereo Width",  NormalisableRange<float>(0.0f, 2.0f, 0.0001f), 1.20f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::glideMs,      "Legato Glide",  NormalisableRange<float>(0.0f, 250.0f, 0.01f), 42.0f));
    params.push_back (std::make_unique<AudioParameterFloat>(IDs::expression,   "Expression",    NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.82f));

    juce::StringArray songChoices { "Zelda's Lullaby", "Epona's Song", "Song of Storms", "Great Fairy Fountain" };
    params.push_back (std::make_unique<AudioParameterChoice>(IDs::songSelect,  "Song", songChoices, 0));
    params.push_back (std::make_unique<AudioParameterBool>(IDs::playSong,      "Play Song", false));

    return { params.begin(), params.end() };
}

struct SongNote
{
    int midiNote = 60;
    float beats = 1.0f;
    float velocity = 0.92f;
};

class SongSequencer
{
public:
    void prepare (double sr, int)
    {
        sampleRate = sr;
        updateTiming();
        reset();
    }

    void setSong (int index)
    {
        currentSongIndex = juce::jlimit (0, 3, index);
        buildSong();
        reset();
    }

    void setPlayEnabled (bool shouldPlay)
    {
        if (shouldPlay && ! playing)
            reset();
        playing = shouldPlay;
    }

    bool isPlaying() const noexcept { return playing; }

    void process (juce::MidiBuffer& midi, int numSamples)
    {
        if (! playing || song.empty())
            return;

        int localSample = 0;

        while (localSample < numSamples)
        {
            if (noteRemainingSamples <= 0)
            {
                if (activeNote >= 0)
                    midi.addEvent (juce::MidiMessage::noteOff (1, activeNote), localSample);

                if (songPos >= static_cast<int> (song.size()))
                {
                    playing = false;
                    activeNote = -1;
                    return;
                }

                const auto& n = song[(size_t) songPos++];
                activeNote = n.midiNote;
                noteRemainingSamples = juce::jmax (1, (int) std::round (n.beats * (double) samplesPerBeat));
                midi.addEvent (juce::MidiMessage::noteOn (1, activeNote, (juce::uint8) juce::jlimit (1, 127, (int) std::round (n.velocity * 127.0f))), localSample);
            }

            const int consume = juce::jmin (numSamples - localSample, noteRemainingSamples);
            noteRemainingSamples -= consume;
            localSample += consume;
        }
    }

private:
    void reset()
    {
        songPos = 0;
        noteRemainingSamples = 0;
        activeNote = -1;
    }

    void updateTiming()
    {
        samplesPerBeat = juce::jmax (1, (int) std::round (sampleRate * 60.0 / bpm));
    }

    void buildSong()
    {
        song.clear();

        switch (currentSongIndex)
        {
            case 0: // Zelda's Lullaby
                bpm = 78.0;
                song = {
                    {69,1.0f,0.90f}, {72,1.0f,0.92f}, {76,2.0f,0.95f},
                    {69,1.0f,0.90f}, {72,1.0f,0.92f}, {76,2.0f,0.95f},
                    {69,1.0f,0.90f}, {72,1.0f,0.91f}, {76,1.0f,0.94f}, {79,1.0f,0.92f}, {77,2.0f,0.95f},
                    {76,1.0f,0.91f}, {72,1.0f,0.90f}, {74,1.0f,0.90f}, {71,1.0f,0.88f}, {69,2.0f,0.93f}
                };
                break;

            case 1: // Epona's Song
                bpm = 98.0;
                song = {
                    {74,1.0f,0.92f}, {72,1.0f,0.90f}, {69,2.0f,0.95f},
                    {74,1.0f,0.92f}, {72,1.0f,0.90f}, {69,2.0f,0.95f},
                    {74,1.0f,0.93f}, {72,1.0f,0.90f}, {69,1.0f,0.90f}, {72,1.0f,0.90f}, {74,2.0f,0.95f},
                    {74,1.0f,0.92f}, {72,1.0f,0.90f}, {69,2.0f,0.95f}
                };
                break;

            case 2: // Song of Storms
                bpm = 132.0;
                song = {
                    {62,0.5f,0.92f}, {65,0.5f,0.92f}, {69,1.0f,0.96f},
                    {62,0.5f,0.92f}, {65,0.5f,0.92f}, {69,1.0f,0.96f},
                    {71,0.5f,0.93f}, {72,0.5f,0.92f}, {71,0.5f,0.92f}, {72,0.5f,0.92f}, {71,1.0f,0.95f},
                    {67,0.5f,0.90f}, {65,0.5f,0.90f}, {64,0.5f,0.88f}, {62,0.5f,0.88f}, {64,2.0f,0.93f}
                };
                break;

            case 3: // Great Fairy Fountain
            default:
                bpm = 88.0;
                song = {
                    {76,0.75f,0.92f}, {79,0.75f,0.92f}, {81,1.5f,0.96f},
                    {79,0.75f,0.91f}, {76,0.75f,0.90f}, {74,1.5f,0.94f},
                    {72,0.75f,0.90f}, {74,0.75f,0.90f}, {76,1.5f,0.94f},
                    {79,0.75f,0.92f}, {81,0.75f,0.93f}, {83,2.0f,0.98f},
                    {81,0.75f,0.91f}, {79,0.75f,0.90f}, {76,1.5f,0.94f},
                    {74,0.75f,0.89f}, {72,0.75f,0.89f}, {74,2.0f,0.94f}
                };
                break;
        }

        updateTiming();
    }

    double sampleRate = 44100.0;
    double bpm = 96.0;
    int samplesPerBeat = 27562;
    int currentSongIndex = 0;
    int songPos = 0;
    int noteRemainingSamples = 0;
    int activeNote = -1;
    bool playing = false;
    std::vector<SongNote> song;
};

class SecretSongUnlocker
{
public:
    void registerSongStart (int songIndex)
    {
        if (unlocked)
            return;

        constexpr int target[3] = { 0, 1, 2 };

        if (songIndex == target[progress])
        {
            ++progress;
            if (progress >= 3)
                unlocked = true;
        }
        else
        {
            progress = (songIndex == target[0]) ? 1 : 0;
        }
    }

    void forceUnlocked()
    {
        unlocked = true;
        progress = 3;
    }

    bool isUnlocked() const noexcept { return unlocked; }

private:
    int progress = 0;
    bool unlocked = false;
};

class OcarinaSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class OcarinaVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<OcarinaSound*> (s) != nullptr;
    }

    void prepare (double sr, int samplesPerBlock, int outputChannels)
    {
        sampleRate = sr;
        adsr.setSampleRate (sampleRate);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (64, samplesPerBlock), (juce::uint32) juce::jmax (1, outputChannels) };

        bodyFilter.reset();
        bodyFilter.prepare (spec);
        bodyFilter.state = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 1150.0, 0.62f);

        airFilter.reset();
        airFilter.prepare (spec);
        airFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 1800.0);

        warmthFilter.reset();
        warmthFilter.prepare (spec);
        warmthFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 4200.0);

        pitchHz.reset (sampleRate, 0.03);
        brightnessSmoothed.reset (sampleRate, 0.04);
        breathBoostSmoothed.reset (sampleRate, 0.04);
    }

    void setGlobalParameters (float attackMs, float releaseMs, float vibratoRateIn, float vibratoDepthIn,
                              float breathIn, float bodyIn, float glideMsIn, float expressionIn)
    {
        adsrParams.attack  = attackMs * 0.001f;
        adsrParams.decay   = 0.08f;
        adsrParams.sustain = 0.96f;
        adsrParams.release = releaseMs * 0.001f;
        adsr.setParameters (adsrParams);

        baseVibRate = vibratoRateIn;
        baseVibDepth = vibratoDepthIn;
        breathAmount = breathIn;
        bodyAmount = bodyIn;
        expression = expressionIn;

        const double glideSeconds = juce::jlimit (0.0, 0.25, (double) glideMsIn * 0.001);
        pitchHz.reset (sampleRate, glideSeconds);
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        currentMidiNote = midiNoteNumber;
        noteVelocity = juce::jlimit (0.0f, 1.0f, velocity);
        targetFrequencyHz = (float) juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

        if (! hasEverStarted)
        {
            pitchHz.setCurrentAndTargetValue (targetFrequencyHz);
            hasEverStarted = true;
        }
        else
        {
            pitchHz.setTargetValue (targetFrequencyHz);
        }

        currentAngle = 0.0f;
        bodyPhase = 0.0f;
        vibratoPhase = 0.0f;
        flutterPhase = 0.0f;

        brightnessSmoothed.setCurrentAndTargetValue (0.55f + 0.45f * noteVelocity);
        breathBoostSmoothed.setCurrentAndTargetValue (0.0f);
        breathBoostSmoothed.setTargetValue (1.0f);

        level = noteVelocity;
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            adsr.noteOff();
        else
        {
            clearCurrentNote();
            adsr.reset();
        }
    }

    void pitchWheelMoved (int value) override
    {
        pitchWheelSemis = juce::jmap ((float) value, 0.0f, 16383.0f, -2.0f, 2.0f);
    }

    void controllerMoved (int controllerNumber, int newValue) override
    {
        if (controllerNumber == 1) // mod wheel
            modWheel = juce::jlimit (0.0f, 1.0f, newValue / 127.0f);

        if (controllerNumber == 11) // expression
            ccExpression = juce::jlimit (0.0f, 1.0f, newValue / 127.0f);
    }

    void aftertouchChanged (int value) override
    {
        aftertouch = juce::jlimit (0.0f, 1.0f, value / 127.0f);
    }

    void channelPressureChanged (int value) override
    {
        aftertouchChanged (value);
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        scratch.setSize (2, numSamples, false, false, true);
        scratch.clear();

        auto* left = scratch.getWritePointer (0);
        auto* right = scratch.getWritePointer (1);

        const float twoPi = juce::MathConstants<float>::twoPi;
        const float sr = (float) sampleRate;

        juce::Random rng ((int64) (currentMidiNote * 257 + 17));

        for (int i = 0; i < numSamples; ++i)
        {
            const float env = adsr.getNextSample();
            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }

            const float freqFromPitchWheel = targetFrequencyHz * std::pow (2.0f, pitchWheelSemis / 12.0f);
            pitchHz.setTargetValue (freqFromPitchWheel);
            const float currentFreq = pitchHz.getNextValue();

            const float vibratoDepthNow = baseVibDepth * (0.45f + 0.55f * modWheel);
            const float vibrato = std::sin (vibratoPhase) * vibratoDepthNow;
            vibratoPhase += twoPi * (baseVibRate + 0.3f * aftertouch) / sr;
            if (vibratoPhase >= twoPi)
                vibratoPhase -= twoPi;

            flutterPhase += twoPi * 11.0f / sr;
            if (flutterPhase >= twoPi)
                flutterPhase -= twoPi;

            const float flutter = std::sin (flutterPhase) * 0.0014f * (0.4f + aftertouch * 0.6f);
            const float playedFreq = currentFreq * (1.0f + vibrato + flutter);

            currentAngle += twoPi * playedFreq / sr;
            if (currentAngle >= twoPi)
                currentAngle -= twoPi;

            bodyPhase += playedFreq / sr;
            if (bodyPhase >= 1.0f)
                bodyPhase -= 1.0f;

            const float sine = std::sin (currentAngle);
            const float triangle = 2.0f * std::abs (2.0f * bodyPhase - 1.0f) - 1.0f;
            const float softTriangle = std::tanh (triangle * 1.25f);

            const float articulationRise = breathBoostSmoothed.getNextValue();
            const float airNoise = (rng.nextFloat() * 2.0f - 1.0f);
            const float breathDynamic = breathAmount * (0.45f + 0.55f * articulationRise) * (0.75f + 0.5f * aftertouch);
            const float expressionDynamic = expression * (0.65f + 0.35f * ccExpression);

            const float velocityBrightness = brightnessSmoothed.getNextValue();
            const float harmonicBlend = juce::jlimit (0.0f, 1.0f, 0.34f + 0.42f * velocityBrightness + 0.18f * aftertouch);

            const float edge = std::tanh ((sine * (1.6f + harmonicBlend) + softTriangle * (0.5f + 0.4f * bodyAmount)) * 0.85f);
            const float airy = airNoise * breathDynamic * (0.18f + 0.42f * std::abs (sine));
            const float whistle = std::sin (currentAngle * 2.0f) * 0.13f * harmonicBlend;
            const float bodyRes = std::sin (currentAngle * 0.5f) * 0.10f * bodyAmount;

            float sample = (sine * 0.54f) + (edge * 0.18f) + (whistle * 0.12f) + (bodyRes * 0.10f) + (airy * 0.30f);
            sample *= env * level * expressionDynamic;

            const float stereoAir = airNoise * breathDynamic * 0.09f;
            left[i] = sample - stereoAir;
            right[i] = sample + stereoAir;
        }

        juce::dsp::AudioBlock<float> block (scratch);
        juce::dsp::ProcessContextReplacing<float> context (block);
        bodyFilter.process (context);
        warmthFilter.process (context);
        airFilter.process (context);

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addFrom (ch, startSample, scratch, juce::jmin (1, ch), 0, numSamples);
    }

private:
    double sampleRate = 44100.0;
    int currentMidiNote = 60;
    bool hasEverStarted = false;

    float noteVelocity = 0.9f;
    float level = 0.9f;
    float targetFrequencyHz = 440.0f;
    float currentAngle = 0.0f;
    float bodyPhase = 0.0f;
    float vibratoPhase = 0.0f;
    float flutterPhase = 0.0f;

    float baseVibRate = 5.2f;
    float baseVibDepth = 0.013f;
    float breathAmount = 0.28f;
    float bodyAmount = 0.68f;
    float expression = 0.82f;

    float pitchWheelSemis = 0.0f;
    float modWheel = 0.0f;
    float aftertouch = 0.0f;
    float ccExpression = 1.0f;

    juce::ADSR adsr;
    juce::ADSR::Parameters adsrParams;
    juce::AudioBuffer<float> scratch;

    juce::dsp::IIR::Filter<float> bodyFilter;
    juce::dsp::IIR::Filter<float> airFilter;
    juce::dsp::IIR::Filter<float> warmthFilter;

    juce::SmoothedValue<float> pitchHz;
    juce::SmoothedValue<float> brightnessSmoothed;
    juce::SmoothedValue<float> breathBoostSmoothed;
};

class OcarinaOfTimeAudioProcessor final : public juce::AudioProcessor
{
public:
    OcarinaOfTimeAudioProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", createParameterLayout())
    {
        for (int i = 0; i < 10; ++i)
            synth.addVoice (new OcarinaVoice());

        synth.addSound (new OcarinaSound());
    }

    const juce::String getName() const override { return "OcarinaOfTimeFlute_DARPA_V2"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);

        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* v = dynamic_cast<OcarinaVoice*> (synth.getVoice (i)))
                v->prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (64, samplesPerBlock), (juce::uint32) juce::jmax (2, getTotalNumOutputChannels()) };

        reverb.reset();
        reverb.prepare (spec);

        master.reset (sampleRate, 0.04);
        master.setCurrentAndTargetValue (1.0f);

        sequencer.prepare (sampleRate, samplesPerBlock);
        updateSongSelection();
    }

    void releaseResources() override {}
    bool hasEditor() const override { return true; }

    class Editor final : public juce::AudioProcessorEditor,
                         private juce::Timer
    {
    public:
        explicit Editor (OcarinaOfTimeAudioProcessor& p)
            : AudioProcessorEditor (&p), processor (p),
              masterSlider (rotary(), textbox()),
              attackSlider (rotary(), textbox()),
              releaseSlider (rotary(), textbox()),
              vibratoRateSlider (rotary(), textbox()),
              vibratoDepthSlider (rotary(), textbox()),
              breathSlider (rotary(), textbox()),
              bodySlider (rotary(), textbox()),
              reverbSlider (rotary(), textbox()),
              widthSlider (rotary(), textbox()),
              glideSlider (rotary(), textbox()),
              expressionSlider (rotary(), textbox())
        {
            setSize (980, 620);

            for (auto* s : getAllSliders())
            {
                configureSlider (*s);
                addAndMakeVisible (*s);
            }

            title.setText ("Ocarina of Time Flute — DARPA V2", juce::dontSendNotification);
            title.setFont (juce::FontOptions (30.0f, juce::Font::bold));
            title.setColour (juce::Label::textColourId, juce::Colour (0xffffe3a0));
            addAndMakeVisible (title);

            info.setText (
                "Cinematic ocarina instrument. Built-in songs, expressive modulation, legato glide, and hidden unlock.\n"
                "Easter egg hint: the original clue remains. Fairy pattern: ↑ ← → ← → ↓\n"
                "Unlock rule: play Zelda's Lullaby, then Epona's Song, then Song of Storms in succession.",
                juce::dontSendNotification);
            info.setColour (juce::Label::textColourId, juce::Colour (0xfffff3cf));
            info.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (info);

            status.setColour (juce::Label::textColourId, juce::Colour (0xfff9d87d));
            status.setFont (juce::FontOptions (16.0f, juce::Font::bold));
            addAndMakeVisible (status);

            songBox.addItemList ({ "Zelda's Lullaby", "Epona's Song", "Song of Storms", "Great Fairy Fountain ?" }, 1);
            songBox.onChange = [this]
            {
                const int requested = juce::jmax (0, songBox.getSelectedId() - 1);

                if (requested == 3 && ! processor.isSecretSongUnlocked())
                {
                    processor.hintStatus = "Great Fairy Fountain is still sealed.";
                    songBox.setSelectedId (1, juce::dontSendNotification);
                    processor.apvts.getParameterAsValue (IDs::songSelect).setValue (0.0);
                    return;
                }

                processor.apvts.getParameterAsValue (IDs::songSelect).setValue ((double) requested);
            };
            addAndMakeVisible (songBox);

            playButton.setButtonText ("Play Built-In Song");
            playButton.onClick = [this]
            {
                const bool next = ! processor.isSongPlaying();
                processor.setSongPlaybackEnabled (next);
            };
            addAndMakeVisible (playButton);

            attachFloat (masterAttach, masterSlider, IDs::masterGain);
            attachFloat (attackAttach, attackSlider, IDs::attackMs);
            attachFloat (releaseAttach, releaseSlider, IDs::releaseMs);
            attachFloat (vibratoRateAttach, vibratoRateSlider, IDs::vibratoRate);
            attachFloat (vibratoDepthAttach, vibratoDepthSlider, IDs::vibratoDepth);
            attachFloat (breathAttach, breathSlider, IDs::breath);
            attachFloat (bodyAttach, bodySlider, IDs::body);
            attachFloat (reverbAttach, reverbSlider, IDs::reverbMix);
            attachFloat (widthAttach, widthSlider, IDs::stereoWidth);
            attachFloat (glideAttach, glideSlider, IDs::glideMs);
            attachFloat (expressionAttach, expressionSlider, IDs::expression);
            songAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, IDs::songSelect, songBox);

            startTimerHz (15);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff0a0d12));

            juce::ColourGradient bg (juce::Colour (0xff16202c), 0, 0,
                                     juce::Colour (0xff0b0e14), 0, (float) getHeight(), false);
            g.setGradientFill (bg);
            g.fillRect (getLocalBounds());

            auto panel = getLocalBounds().reduced (16).toFloat();
            g.setColour (juce::Colour (0xdd161b23));
            g.fillRoundedRectangle (panel, 24.0f);

            g.setColour (juce::Colour (0x55ffd98b));
            g.drawRoundedRectangle (panel, 24.0f, 2.0f);

            // Decorative Triforce-esque glow
            juce::Path tri;
            const float cx = (float) getWidth() - 165.0f;
            const float cy = 96.0f;
            tri.addTriangle (cx, cy - 32.0f, cx - 28.0f, cy + 16.0f, cx + 28.0f, cy + 16.0f);
            tri.addTriangle (cx - 33.0f, cy + 22.0f, cx - 61.0f, cy + 70.0f, cx - 5.0f, cy + 70.0f);
            tri.addTriangle (cx + 33.0f, cy + 22.0f, cx + 5.0f, cy + 70.0f, cx + 61.0f, cy + 70.0f);
            g.setColour (juce::Colour (0x24ffd86e));
            g.fillPath (tri);
            g.setColour (juce::Colour (0x80ffe29b));
            g.strokePath (tri, juce::PathStrokeType (2.0f));
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (26);

            auto top = r.removeFromTop (120);
            title.setBounds (top.removeFromTop (36));
            info.setBounds (top.removeFromTop (58));
            status.setBounds (top.removeFromTop (22));

            auto transport = r.removeFromTop (68);
            songBox.setBounds (transport.removeFromLeft (260).reduced (4));
            playButton.setBounds (transport.removeFromLeft (210).reduced (4));

            auto grid = r.reduced (4);
            const int cols = 6;
            const int rows = 2;
            const int cellW = grid.getWidth() / cols;
            const int cellH = grid.getHeight() / rows;

            auto sliders = getAllSliders();
            int idx = 0;
            for (int y = 0; y < rows; ++y)
            {
                for (int x = 0; x < cols; ++x)
                {
                    if (idx >= (int) sliders.size())
                        return;

                    sliders[(size_t) idx++]->setBounds (grid.getX() + x * cellW + 6,
                                                        grid.getY() + y * cellH + 6,
                                                        cellW - 12,
                                                        cellH - 12);
                }
            }
        }

    private:
        static juce::Slider::SliderStyle rotary() { return juce::Slider::RotaryHorizontalVerticalDrag; }
        static juce::Slider::TextEntryBoxPosition textbox() { return juce::Slider::TextBoxBelow; }

        void configureSlider (juce::Slider& s)
        {
            s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd4af65));
            s.setColour (juce::Slider::thumbColourId, juce::Colour (0xfffff0b4));
            s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        }

        std::vector<juce::Slider*> getAllSliders()
        {
            return {
                &masterSlider, &attackSlider, &releaseSlider, &vibratoRateSlider, &vibratoDepthSlider, &breathSlider,
                &bodySlider, &reverbSlider, &widthSlider, &glideSlider, &expressionSlider
            };
        }

        void attachFloat (std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& slot,
                          juce::Slider& slider, const char* param)
        {
            slot = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, param, slider);
        }

        void timerCallback() override
        {
            playButton.setButtonText (processor.isSongPlaying() ? "Stop Song" : "Play Built-In Song");
            status.setText (processor.isSecretSongUnlocked()
                                ? "Secret unlocked: Great Fairy Fountain is available."
                                : processor.hintStatus,
                            juce::dontSendNotification);

            if (processor.isSecretSongUnlocked() && songBox.getNumItems() >= 4)
                songBox.changeItemText (4, "Great Fairy Fountain");
        }

        OcarinaOfTimeAudioProcessor& processor;

        juce::Label title, info, status;
        juce::ComboBox songBox;
        juce::TextButton playButton;

        juce::Slider masterSlider, attackSlider, releaseSlider, vibratoRateSlider, vibratoDepthSlider,
                     breathSlider, bodySlider, reverbSlider, widthSlider, glideSlider, expressionSlider;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach, attackAttach, releaseAttach,
                                                                              vibratoRateAttach, vibratoDepthAttach,
                                                                              breathAttach, bodyAttach, reverbAttach,
                                                                              widthAttach, glideAttach, expressionAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> songAttach;
    };

    juce::AudioProcessorEditor* createEditor() override
    {
        return new Editor (*this);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        updateVoices();
        updateSongSelection();
        processUnlockLogic();

        juce::MidiBuffer mergedMidi (midiMessages);
        sequencer.process (mergedMidi, buffer.getNumSamples());
        synth.renderNextBlock (buffer, mergedMidi, 0, buffer.getNumSamples());

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        juce::dsp::Reverb::Parameters params;
        params.roomSize = 0.72f;
        params.damping = 0.32f;
        params.wetLevel = *apvts.getRawParameterValue (IDs::reverbMix) * 0.40f;
        params.dryLevel = 1.0f - (*apvts.getRawParameterValue (IDs::reverbMix) * 0.15f);
        params.width = juce::jlimit (0.0f, 1.0f, *apvts.getRawParameterValue (IDs::stereoWidth) * 0.5f);
        params.freezeMode = 0.0f;
        reverb.setParameters (params);
        reverb.process (context);

        master.setTargetValue (juce::Decibels::decibelsToGain (*apvts.getRawParameterValue (IDs::masterGain)));
        const float g = master.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.applyGain (ch, 0, buffer.getNumSamples(), g);
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto state = apvts.copyState();
        state.setProperty ("greatFairyUnlocked", unlocker.isUnlocked(), nullptr);

        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
        {
            if (xml->hasTagName (apvts.state.getType()))
            {
                auto state = juce::ValueTree::fromXml (*xml);
                apvts.replaceState (state);

                if ((bool) state.getProperty ("greatFairyUnlocked", false))
                {
                    unlocker.forceUnlocked();
                    hintStatus = "Secret restored from saved state.";
                }
            }
        }
    }

    bool isSongPlaying() const { return sequencer.isPlaying(); }
    bool isSecretSongUnlocked() const { return unlocker.isUnlocked(); }

    void setSongPlaybackEnabled (bool shouldPlay)
    {
        const int selected = getSelectedSongIndex();
        if (shouldPlay && selected == 3 && ! unlocker.isUnlocked())
        {
            hintStatus = "Great Fairy Fountain is still sealed.";
            sequencer.setPlayEnabled (false);
            return;
        }

        sequencer.setPlayEnabled (shouldPlay);
    }

    juce::AudioProcessorValueTreeState apvts;
    juce::String hintStatus = "Secret song locked.";

private:
    int getSelectedSongIndex() const
    {
        return (int) *apvts.getRawParameterValue (IDs::songSelect);
    }

    void updateVoices()
    {
        const auto attack = *apvts.getRawParameterValue (IDs::attackMs);
        const auto release = *apvts.getRawParameterValue (IDs::releaseMs);
        const auto vibRate = *apvts.getRawParameterValue (IDs::vibratoRate);
        const auto vibDepth = *apvts.getRawParameterValue (IDs::vibratoDepth);
        const auto breath = *apvts.getRawParameterValue (IDs::breath);
        const auto body = *apvts.getRawParameterValue (IDs::body);
        const auto glide = *apvts.getRawParameterValue (IDs::glideMs);
        const auto expr = *apvts.getRawParameterValue (IDs::expression);

        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (auto* voice = dynamic_cast<OcarinaVoice*> (synth.getVoice (i)))
                voice->setGlobalParameters (attack, release, vibRate, vibDepth, breath, body, glide, expr);
    }

    void updateSongSelection()
    {
        int selected = getSelectedSongIndex();

        if (selected == 3 && ! unlocker.isUnlocked())
        {
            selected = 0;
            apvts.getParameterAsValue (IDs::songSelect).setValue (0.0);
        }

        if (selected != lastSongSelection)
        {
            lastSongSelection = selected;
            sequencer.setSong (selected);
        }
    }

    void processUnlockLogic()
    {
        const bool nowPlaying = sequencer.isPlaying();

        if (nowPlaying && ! wasPlayingLastBlock)
        {
            const int song = getSelectedSongIndex();
            if (song >= 0 && song <= 2)
            {
                const bool wasUnlocked = unlocker.isUnlocked();
                unlocker.registerSongStart (song);

                if (! wasUnlocked && unlocker.isUnlocked())
                    hintStatus = "Secret unlocked: Great Fairy Fountain awakened.";
                else
                    hintStatus = "Sequence registered...";
            }
        }

        wasPlayingLastBlock = nowPlaying;
    }

    juce::Synthesiser synth;
    SongSequencer sequencer;
    SecretSongUnlocker unlocker;
    int lastSongSelection = -1;
    bool wasPlayingLastBlock = false;

    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> master;

    friend class Editor;

public:
    static juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
    {
        return new OcarinaOfTimeAudioProcessor();
    }
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OcarinaOfTimeAudioProcessor();
}
