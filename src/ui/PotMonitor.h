#pragma once
#include <stdint.h>
#include "ui/UiEventQueue.h"
#include "sys/system.h"

namespace daisy
{
/** This class monitors a number of potentiometers and detects pot movements.
 *  When a movement is detected, an event is added to a UiEventQueue.
 *  Pots can be either "idle" or "moving" in which case different dead bands
 *  are applied to them. The current state and value of a pot can be requested at any time.
 *
 *  This class can monitor an arbitrary number of potentiometers, as configured by its
 *  template argument `kNumPots`. Each of the pots is identified by an ID number from
 *  `0 .. kNumPots - 1`. This number will also be used when events are posted to the
 *  UiEventQueue.
 *
 *  The BackendType class will provide the source data for each potentiometer.
 *  An instance of this backend must be supplied via the constructor.
 *  It must implement the following public function via which the PotMonitor
 *  will request the current value of the potentiometer in the range `0 .. 1`:
 *
 *      float getPotValue(uint16_t potId);
 * 
 *  This allows arbitrary hardware setups to be used with this class. 
 *
 *   @tparam BackendType 	The class type of the backend that will supply pot values.
 *   @tparam kNumPots 		The number of pots to monitor.
 */
template <typename BackendType, uint32_t kNumPots>
class PotMonitor
{
  public:
    /** Initialises the PotMonitor.
	 * @param queue_to_add_events_to The UiEventQueue to which events should be posted.
	 * @param backend				 The backend that supplies the current value of each potentiometer.
	 * @param dead_band_idle 		 The dead band that must be exceeded before a movement is detected
	 *                      		 when the pot is currently idle.
	 * @param dead_band 			 The dead band that must be exceeded before a movement is detected
	 *                      		 when the pot is currently moving.
	 * @param idle_timeout   		 When the pot is currently moving, but no event is generated over
	 *                      		 "idle_timeout" systicks, the pot enters the idle state.
	 */
    void init(UiEventQueue& queue_to_add_events_to,
              BackendType&  backend,
              uint16_t      idle_timeout,
              float         dead_band_idle = 1.0 / (1 << 10),
              float         dead_band      = 1.0 / (1 << 12))
    {
        queue_          = &queue_to_add_events_to;
        dead_band_      = dead_band;
        backend_        = &backend;
        dead_band_idle_ = dead_band_idle;
        timeout_        = idle_timeout;
        last_time_      = dsy_system_getnow();

        for(uint32_t i = 0; i < kNumPots; i++)
        {
            last_value_[i]      = 0.0;
            timeout_counter_[i] = 0;
        }
    }

    /** Checks the value of each pot and generates messages for the UIEventQueue.
	 *  Call this at regular intervals.
	 */
    void process()
    {
        const auto now       = dsy_system_getnow();
        const auto time_diff = now - last_time_;
        last_time_           = now;
        for(uint32_t i = 0; i < kNumPots; i++)
            processPot(i, backend_->getPotValue(i), time_diff);
    }

    /** Returns true, if the requested pot is currently being moved.
	 *  @param pot_id	The unique ID of the potentiometer (< kNumPots)
	 */
    bool isMoving(uint16_t pot_id) const
    {
        if(pot_id >= kNumPots)
            return false;
        else
            return timeout_counter_[pot_id] < timeout_;
    }

    /** For a given potentiometer, this will return the last value that was
	 *  posted to the UiEventQueue.
	 *  @param pot_id^	The unique ID of the potentiometer (< kNumPots)
	 */
    float getCurrentPotValue(uint16_t pot_id) const
    {
        if(pot_id >= kNumPots)
            return -1.0f;
        else
            return last_value_[pot_id];
    }

    /** Returns the BackendType that is used by the monitor. */
    BackendType& getBackend() { return backend_; }

    /** Returns the number of pots that are monitored by this class. */
    uint16_t getNumPotsMonitored() const { return kNumPots; }

  private:
    /** Process a potentiometer and detect movements - or
	 *  flag the pot as "idle" when no movement is detected for
	 *  a longer period of time.
	 *  @param id		The unique ID of the potentiometer (< kNumPots)
	 *  @param value	The new value in the range 0..1
	 */
    void processPot(uint16_t id, float value, uint32_t time_diff)
    {
        // currently moving?
        if(timeout_counter_[id] < timeout_)
        {
            // check if pot has left the deadband. If so, add a new message
            // to the queue.
            float delta = last_value_[id] - value;
            if((delta > dead_band_) || (delta < dead_band__))
            {
                last_value_[id] = value;
                queue_->addPotMoved(id, value);
                timeout_counter_[id] = 0;
            }
            // no movement, increment timeout counter
            else
            {
                timeout_counter_[id] += time_diff;
                // post activity changed event after timeout expired.
                if(timeout_counter_[id] >= timeout_)
                {
                    queue_->addPotActivityChanged(id, false);
                }
            }
        }
        // not moving right now
        else
        {
            // check if pot has left the idle deadband. If so, add a new message
            // to the queue and restart the timeout
            float delta = last_value_[id] - value;
            if((delta > dead_band_idle_) || (delta < -dead_band_idle_))
            {
                last_value_[id] = value;
                queue_->addPotActivityChanged(id, true);
                queue_->addPotMoved(id, value);
                timeout_counter_[id] = 0;
            }
        }
    }

    PotMonitor(const PotMonitor&) = delete;
    PotMonitor& operator=(const PotMonitor&) = delete;

    UiEventQueue* queue_;
    BackendType*  backend_;
    float         dead_band_;
    float         dead_band_idle_;
    uint32_t      timeout_;
    float         last_value_[kNumPots];
    uint32_t      timeout_counter_[kNumPots];
    uint32_t      last_time_;
};
} // namespace daisy