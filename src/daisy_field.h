#pragma once
#ifndef DSY_FIELD_BSP_H
#define DSY_FIELD_BSP_H /**< & */
#include "daisy_seed.h"

/**
   @brief Hardware defines and helpers for daisy field platform.
*/
namespace daisy
{
/** @addtogroup boards
    @{
*/
class DaisyField
{
  public:
    /** enums for controls, etc.
*/
    enum
    {
        SW_1,    /**< tactile switch */
        SW_2,    /**< tactile switch */
        SW_LAST, /**< & */
    };

    /** All knobs connect to Daisy Seed's ADC1 pin via CD4051 mux
        Knobs are in order that they are laid out on hardware.
    */
    enum
    {
        KNOB_1,    /**< & */
        KNOB_4,    /**< & */
        KNOB_2,    /**< & */
        KNOB_5,    /**< & */
        KNOB_3,    /**< & */
        KNOB_6,    /**< & */
        KNOB_7,    /**< & */
        KNOB_8,    /**< & */
        KNOB_LAST, /**< & */
    };

    enum
    {
        CV_1,    /** Connected to ADC1_INP17 */
        CV_2,    /** Connected  to ADC1_INP15 */
        CV_3,    /** Connected to ADC1_INP4 */
        CV_4,    /** Connected to ADC1_INP11 */
        CV_LAST, /**< & */
    };

    enum
    {
        LED_KEY_A8, /**< & */
        LED_KEY_A7, /**< & */
        LED_KEY_A6, /**< & */
        LED_KEY_A5, /**< & */
        LED_KEY_A4, /**< & */
        LED_KEY_A3, /**< & */
        LED_KEY_A2, /**< & */
        LED_KEY_A1, /**< & */
        LED_KEY_B1, /**< & */
        LED_KEY_B2, /**< & */
        LED_KEY_B3, /**< & */
        LED_KEY_B4, /**< & */
        LED_KEY_B5, /**< & */
        LED_KEY_B6, /**< & */
        LED_KEY_B7, /**< & */
        LED_KEY_B8, /**< & */
        LED_KNOB_1, /**< & */
        LED_KNOB_2, /**< & */
        LED_KNOB_3, /**< & */
        LED_KNOB_4, /**< & */
        LED_KNOB_5, /**< & */
        LED_KNOB_6, /**< & */
        LED_KNOB_7, /**< & */
        LED_KNOB_8, /**< & */
        LED_SW_1,   /**< & */
        LED_SW_2,   /**< & */
        LED_LAST    /**< & */
    };

    DaisyField() {}
    ~DaisyField() {}

    /**Initializes the Daisy Field, and all of its hardware.*/
    void Init();

    /** Starts the Audio Engine, calling the specified interleaving callback when new data is ready */
    void StartAudio(dsy_audio_callback cb)
    {
        dsy_audio_set_callback(DSY_AUDIO_INTERNAL, cb);
        dsy_audio_start(DSY_AUDIO_INTERNAL);
    }

    /** Starts the Audio Engine, calling the specified non-interleaving callback when new data is ready */
    void StartAudio(dsy_audio_mc_callback cb)
    {
        dsy_audio_set_mc_callback(cb);
        dsy_audio_start(DSY_AUDIO_INTERNAL);
    }

    /** Starts Transfering data from the ADC */
    void StartAdc() { seed.adc.Start(); }

    /** Returns the samplerate of the audio engine. */
    inline float SampleRate() const { return samplerate_; }

    /** Returns the rate at which the audio callback is called. */
    inline float BlockRate() const { return blockrate_; }

    /** Returns the size of the audio buffer in frames .*/
    inline size_t BlockSize() const { return blocksize_; }

    /** Processes the ADC inputs, updating their values */
    void ProcessAnalogControls()
    {
        for(size_t i = 0; i < KNOB_LAST; i++)
            knob_[i].Process();
        for(size_t i = 0; i < CV_LAST; i++)
            cv_[i].Process();
    }

    /** Debounces, the tactile switches and keyboard states */
    void UpdateDigitalControls()
    {
        // Switches
        for(size_t i = 0; i < SW_LAST; i++)
        {
            sw_[i].Debounce();
        }
        // Keyboard SM
        dsy_sr_4021_update(&keyboard_sr_);
        for(size_t i = 0; i < 16; i++)
        {
            uint8_t keyidx, keyoffset;
            keyoffset               = i > 7 ? 8 : 0;
            keyidx                  = (7 - (i % 8)) + keyoffset;
            keyboard_state_[keyidx] = dsy_sr_4021_state(&keyboard_sr_, i)
                                      | (keyboard_state_[keyidx] << 1);
        }
        // Gate Input
        gate_in_trig_ = gate_in_.Trig();
    }

    inline bool KeyboardState(size_t idx) const
    {
        return keyboard_state_[idx] == 0x00;
    }

    inline bool KeyboardRisingEdge(size_t idx) const
    {
        return keyboard_state_[idx] == 0x80;
    }

    inline bool KeyboardFallingEdge(size_t idx) const
    {
        return keyboard_state_[idx] == 0x7F;
    }

    inline float GetKnobValue(size_t idx) const
    {
        return knob_[idx < KNOB_LAST ? idx : 0].Value();
    }

    inline float GetCvValue(size_t idx) const
    {
        return cv_[idx < CV_LAST ? idx : 0].Value();
    }

    inline Switch* GetSwitch(size_t idx)
    {
        return &sw_[idx < SW_LAST ? idx : 0];
    }

    inline AnalogControl* GetKnob(size_t idx)
    {
        return &knob_[idx < KNOB_LAST ? idx : 0];
    }

    inline AnalogControl* GetCv(size_t idx)
    {
        return &cv_[idx < CV_LAST ? idx : 0];
    }

    /**Light show, cycling through all LEDs, and OLED
     **/
    void VegasMode();

    DaisySeed   seed;
    OledDisplay display;
    dsy_gpio    gate_out_;

  private:
    float                     samplerate_, blockrate_;
    size_t                    blocksize_;
    Switch                    sw_[SW_LAST];
    dsy_sr_4021_handle        keyboard_sr_;
    GateIn                    gate_in_;
    AnalogControl             knob_[KNOB_LAST];
    AnalogControl             cv_[CV_LAST];
    uint8_t                   keyboard_state_[16];
    uint32_t                  last_led_update_; // for vegas mode
    bool                      gate_in_trig_;    // True when triggered.
    LedDriverPca9685<2, true> led_driver_;
};

/** @} */

} // namespace daisy

#endif
