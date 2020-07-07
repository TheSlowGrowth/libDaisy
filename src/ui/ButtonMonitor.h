#pragma once
#include <stdint.h>
#include "ui/UiEventQueue.h"
#include "sys/system.h"

/** This class monitors a number of buttons and detects changes in their state.
 *  When a change is detected, an event is added to a UiEventQueue. If required, software
 *  debouncing can be applied.
 *
 *  This class can monitor an arbitrary number of buttons or switches, as configured by its
 *  template argument `kNumButtons`. Each of the buttons is identified by an ID number from
 *  `0 .. kNumButtons - 1`. This number will also be used when events are posted to the
 *  UiEventQueue.
 *
 *  The BackendType class will provide the source data for each button or switch.
 *  An instance of this backend must be supplied. It must implement the following public 
 *  function via which the ButtonMonitor will request the current state of the button:
 *
 *      bool isButtonPressed(uint16_t button_id);
 * 
 *  This allows arbitrary hardware configurations to be used (e.g. combinations of shift 
 *  registers, GPIOs, I2C port expanders, etc.)
 *
 *   @tparam BackendType 	The class type of the backend that will supply button states.
 *   @tparam kNumButtons    The number of buttons to monitor.
 */
template <typename BackendType, uint32_t kNumButtons>
class ButtonMonitor
{
  public:
    /** Initialises the ButtonMonitor.
	 * @param queue_to_add_events_to	The UiEventQueue to which events should be posted.
	 * @param backend				The backend that supplies the current state of each button.
	 * @param debounce_timeout   	A event is posted to the queue if the button state doesn't change
	 * 								for `debounce_timeout` number of successive calls to process(). Must
	 * 								be >= 1, where 1 is no debouncing.
	 * @param double_click_timeout	The timeout (in system ticks) for detecting double clicks.
	 */
    void init(UiEventQueue& queue_to_add_events_to,
              BackendType&  backend,
              uint16_t      debounce_timeout,
              uint32_t      double_click_timeout)
    {
        queue_                = &queue_to_add_events_to;
        backend_              = &backend;
        timeout_              = debounce_timeout;
        double_click_timeout_ = double_click_timeout;

        for(uint32_t i = 0; i < kNumButtons; i++)
        {
            button_states_[i]    = -timeout_; // starting in "released" state
            last_click_times_[i] = 0;
            num_successive_clicks_[i] = 0;
        }
    }

    /** Checks the value of each button and generates messages for the UIEventQueue.
	 *  Call this at regular intervals.
	 */
    void process()
    {
        const auto now = dsy_system_getnow();
        for(uint32_t i = 0; i < kNumButtons; i++)
            processButton(i, backend_->isButtonPressed(i), now);
    }

    /** Returns true, if the given button is currently pressed.
	 *  @param button_id		The unique ID of the button (< kNumButtons)
	 */
    bool isButtonPressed(uint16_t button_id) const
    {
        if(button_id >= kNumButtons)
            return false;
        else
            return button_states_[button_id] >= timeout_;
    }

    /** Returns the BackendType that is used by the monitor. */
    BackendType& getBackend() { return backend_; }

    /** Returns the number of buttons that are monitored by this class. */
    uint16_t getNumButtonsMonitored() const { return kNumButtons; }

  private:
    void processButton(uint16_t id, bool is_pressed, uint32_t now)
    {
        // released or transitioning there...
        if(button_states_[id] < 0)
        {
            if(!is_pressed)
            {
                // transitioning?
                if(button_states_[id] > -timeout_)
                {
                    button_states_[id]--;
                    if(button_states_[id] <= -timeout_)
                        queue_->addButtonReleased(id);
                }
            }
            // start transitioning towards "pressed"
            else
            {
                button_states_[id] = 1;
                // timeout could be set to "1" - no debouncing, send immediately.
                if(button_states_[id] >= timeout_)
                    postButtonDownEvent(id, now);
            }
        }
        else
        {
            if(is_pressed)
            {
                // transitioning?
                if(button_states_[id] < timeout_)
                {
                    button_states_[id]++;
                    if(button_states_[id] >= timeout_)
                        postButtonDownEvent(id, now);
                }
            }
            // start transitioning towards "released"
            else
            {
                button_states_[id] = -1;
                // timeout could be set to "1" - no debouncing, send immediately.
                if(button_states_[id] <= -timeout_)
                    queue_->addButtonReleased(id);
            }
        }
    }

    void postButtonDownEvent(uint16_t id, uint32_t now)
    {
        const auto timeDiff = now - last_click_times_[id];
        if(timeDiff <= double_click_timeout_)
            num_successive_clicks_[id]++;
        else
            num_successive_clicks_[id] = 1;

        last_click_times_[id] = now;
        queue_->addButtonPressed(id, num_successive_clicks_[id]);
    }

    ButtonMonitor(const ButtonMonitor&) = delete;
    ButtonMonitor& operator=(const ButtonMonitor&) = delete;

    UiEventQueue* queue_;
    BackendType*  backend_;
    uint16_t      timeout_;
    uint32_t      double_click_timeout_;
    int16_t       button_states_[kNumButtons]; // <= -timeout >>> not pressed
                                               // >= timeout_ >>> pressed
    uint32_t last_click_times_[kNumButtons];
    uint8_t  num_successive_clicks_[kNumButtons];
};
