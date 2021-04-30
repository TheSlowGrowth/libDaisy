#pragma once
#include <stdint.h>
#include "UiEventQueue.h"
#include "../sys/system.h"

namespace daisy
{
/** @brief A button monitor that generates events in a UiEventQueue
 *  @author jelliesen
 *  @addtogroup ui
 * 
 *  This class monitors a number of buttons and detects changes in their state.
 *  When a change is detected, an event is added to a UiEventQueue. If required, software
 *  debouncing can be applied.
 *
 *  This class can monitor an arbitrary number of buttons or switches, as configured by its
 *  template argument `numButtons`. Each of the buttons is identified by an ID number from
 *  `0 .. numButtons - 1`. This number will also be used when events are posted to the
 *  UiEventQueue. It's suggested to define an enum in your project like this:
 *  
 *      enum ButtonId { bttnOkay = 0, bttnCancel = 1, bttnStart = 2 };
 * 
 *  In different projects, diffent ways of reading the button states will be
 *  used. That's why this class uses a generic backend that you'll have to write.
 *  The BackendType class will provide the source data for each button or switch.
 *  An instance of this backend must be supplied via the constructor.
 *  It must implement the following public function via which the ButtonMonitor
 *  will request the current state of the button:
 *
 *      bool IsButtonPressed(uint16_t buttonId);
 *
 *   @tparam BackendType 	The class type of the backend that will supply button states.
 *   @tparam numButtons     The number of buttons to monitor.
 */
template <typename BackendType, uint32_t numButtons>
class ButtonMonitor
{
  public:
    ButtonMonitor()
    : queue_(nullptr), backend_(nullptr), timeout_(0), doubleClickTimeout_(0)
    {
    }

    /** Initialises the ButtonMonitor.
     * @param queueToAddEventsTo	The UiEventQueue to which events should be posted.
     * @param backend				The backend that supplies the current state of each button.
     * @param debounceTimeoutMs   	A event is posted to the queue if the button state doesn't change
     * 								for `debounceTimeoutMs`. Can be 0 to disable debouncing.
     * @param doubleClickTimeoutMs	The timeout for detecting double clicks.
     */
    void Init(UiEventQueue& queueToAddEventsTo,
              BackendType&  backend,
              uint16_t      debounceTimeoutMs,
              uint32_t      doubleClickTimeoutMs)
    {
        queue_              = &queueToAddEventsTo;
        backend_            = &backend;
        timeout_            = debounceTimeoutMs;
        doubleClickTimeout_ = doubleClickTimeoutMs;

        for(uint32_t i = 0; i < numButtons; i++)
        {
            buttonStates_[i]        = -timeout_; // starting in "released" state
            lastClickTimes_[i]      = 0;
            numSuccessiveClicks_[i] = 0;
        }

        lastCallSysTime_ = System::GetNow();
    }

    /** Checks the value of each button and generates messages for the UIEventQueue.
     *  Call this at regular intervals, ideally from your main() idle loop.
     */
    void Process()
    {
        const auto now      = System::GetNow();
        const auto timeDiff = now - lastCallSysTime_;
        lastCallSysTime_    = now;

        for(uint32_t i = 0; i < numButtons; i++)
            ProcessButton(i, backend_->IsButtonPressed(i), timeDiff, now);
    }

    /** Returns true, if the given button is currently pressed.
     *  @param buttonId		The unique ID of the button (< numButtons)
     */
    bool IsButtonPressed(uint16_t buttonId) const
    {
        if(buttonId >= numButtons)
            return false;
        else
            return buttonStates_[buttonId] >= timeout_;
    }

    /** Returns the BackendType that is used by the monitor. */
    BackendType& GetBackend() { return backend_; }

    /** Returns the number of buttons that are monitored by this class. */
    uint16_t GetNumButtonsMonitored() const { return numButtons; }

  private:
    void ProcessButton(uint16_t id,
                       bool     isPressed,
                       uint32_t timeInMsSinceLastCall,
                       uint32_t currentSystemTime)
    {
        // released or transitioning there...
        if(buttonStates_[id] < 0)
        {
            if(!isPressed)
            {
                // transitioning?
                if(buttonStates_[id] + 1 > -timeout_)
                {
                    buttonStates_[id] -= timeInMsSinceLastCall;
                    if(buttonStates_[id] + 1 <= -timeout_)
                        queue_->AddButtonReleased(id);
                }
            }
            // start transitioning towards "pressed"
            else
            {
                buttonStates_[id] = 1;
                // timeout could be set to "0" - no debouncing, send immediately.
                if(buttonStates_[id] - 1 >= timeout_)
                    PostButtonDownEvent(id, currentSystemTime);
            }
        }
        else
        {
            if(isPressed)
            {
                // transitioning?
                if(buttonStates_[id] - 1 < timeout_)
                {
                    buttonStates_[id] += timeInMsSinceLastCall;
                    if(buttonStates_[id] - 1 >= timeout_)
                        PostButtonDownEvent(id, currentSystemTime);
                }
            }
            // start transitioning towards "released"
            else
            {
                buttonStates_[id] = -1;
                // timeout could be set to "0" - no debouncing, send immediately.
                if(buttonStates_[id] + 1 <= -timeout_)
                    queue_->AddButtonReleased(id);
            }
        }
    }

    void PostButtonDownEvent(uint16_t id, uint32_t currentSystemTime)
    {
        const auto timeDiff = currentSystemTime - lastClickTimes_[id];
        if(timeDiff <= doubleClickTimeout_)
            numSuccessiveClicks_[id]++;
        else
            numSuccessiveClicks_[id] = 1;

        lastClickTimes_[id] = currentSystemTime;
        queue_->AddButtonPressed(id, numSuccessiveClicks_[id]);
    }

    ButtonMonitor(const ButtonMonitor&) = delete;
    ButtonMonitor& operator=(const ButtonMonitor&) = delete;

    UiEventQueue* queue_;
    BackendType*  backend_;
    uint16_t      timeout_;
    uint32_t      doubleClickTimeout_;
    int16_t       buttonStates_[numButtons]; // <= -timeout --> not pressed,
                                             // >= timeout_ --> pressed
    uint32_t lastClickTimes_[numButtons];
    uint8_t  numSuccessiveClicks_[numButtons];
    uint32_t lastCallSysTime_;
};

} // namespace daisy