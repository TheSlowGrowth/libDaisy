#pragma once
#include <stdint.h>
#include "../util_ringbuffer.h"
#include "../util_scopedirqblocker.h"

namespace daisy
{
/** A queue that holds user interface events such as button presses or encoder turns.
 *  Events can be added to the queue from ISRs and other time critical 
 *  routines and later retrieved from the main loop.
 *  Buttons, encoders and potentiometers are referred to by unique IDs.
 *  There is support for activity detection and successive button presses.
 */
class UiEventQueue
{
  public:
    /** A button ID used to indicate an invalid or non existing button. */
    static constexpr uint16_t kInvalidButtonId = UINT16_MAX;

    /** An encoder ID used to indicate an invalid or non existing encoder. */
    static constexpr uint16_t kInvalidEncoderId = UINT16_MAX;

    /** A potentiometer ID used to indicate an invalid or non existing potentiometer. */
    static constexpr uint16_t kInvalidPotId = UINT16_MAX;

    /** An event */
    struct __attribute__((packed)) Event
    {
        /** The type of event */
        enum class EventType
        {
            /** An invalid event. Returned to indicate that no events are left in the queue. */
            INVALID,
            /** A button was pressed. */
            BUTTON_PRESSED,
            /** A button was released. */
            BUTTON_RELEASED,
            /** An encoder was turned. */
            ENCODER_TURNED,
            /** The user has started or stopped turning an encoder. */
            ENCODER_ACTIVITY_CHANGED,
            /** A potentiometer was moved. */
            POT_MOVED,
            /** The user has started or stopped moving a potentiometer. */
            POT_ACTIVITY_CHANGED,
        };

        /** Used to indicate if a control is currently being used. */
        enum class ActivityType
        {
            /** The control is not in use at the moment. */
            INACTIVE,
            /** The control is actively used at the moment. */
            ACTIVE
        };

        /** The type of event that this Event object represents. */
        EventType
            type : 16; // default size could be more than 16 bits - not required

        union
        {
            struct __attribute__((packed))
            {
                /** The unique ID of the button that was pressed. */
                uint16_t id;
                /** The number of successive button presses (e.g. double click) */
                uint16_t num_successive_presses;
            } as_button_pressed;
            struct __attribute__((packed))
            {
                /** The unique ID of the button that was released. */
                uint16_t id;
            } as_button_released;
            struct __attribute__((packed))
            {
                /** The unique ID of the encoder that was turned. */
                uint16_t id;
                /** The number of increments detected. */
                int16_t increments;
                /** The total number of increments per revolution. */
                uint16_t steps_per_rev;
            } as_encoder_turned;
            struct __attribute__((packed))
            {
                /** The unique ID of the encoder that is affected. */
                uint16_t id;
                /** The new activity type. */
                ActivityType new_activity_type : 1;
            } as_encoder_activity_changed;
            struct __attribute__((packed))
            {
                /** The unique ID of the pot that was moved. */
                uint16_t id;
                /** The new position of the pot. */
                float new_position;
            } as_pot_moved;
            struct __attribute__((packed))
            {
                /** The unique ID of the pot that is affected. */
                uint16_t id;
                /** The new activity type. */
                ActivityType new_activity_type : 1;
            } as_pot_activity_changed;
        };
    };

    UiEventQueue() {}
    ~UiEventQueue() {}

    /** Adds a Event::EventType::BUTTON_PRESSED event to the queue. */
    void addButtonPressed(uint16_t button_id, uint16_t num_successive_presses)
    {
        Event e;
        e.type                 = Event::EventType::BUTTON_PRESSED;
        e.as_button_pressed.id = button_id;
        e.as_button_pressed.num_successive_presses = num_successive_presses;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(e);
    }

    /** Adds a Event::EventType::BUTTON_RELEASED event to the queue. */
    void addButtonReleased(uint16_t button_id)
    {
        Event m;
        m.type                  = Event::EventType::BUTTON_RELEASED;
        m.as_button_released.id = button_id;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(m);
    }

    /** Adds a Event::EventType::ENCODER_TURNED event to the queue. */
    void addEncoderTurned(uint16_t encoder_id,
                          int16_t  increments,
                          uint16_t steps_per_pev)
    {
        Event e;
        e.type                            = Event::EventType::ENCODER_TURNED;
        e.as_encoder_turned.id            = encoder_id;
        e.as_encoder_turned.increments    = increments;
        e.as_encoder_turned.steps_per_rev = steps_per_pev;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(e);
    }

    /** Adds a Event::EventType::encoderActivityChanged event to the queue. */
    void addEncoderActivityChanged(uint16_t encoder_id, bool is_active)
    {
        Event e;
        e.type = Event::EventType::ENCODER_ACTIVITY_CHANGED;
        e.as_encoder_activity_changed.id = encoder_id;
        e.as_encoder_activity_changed.new_activity_type
            = is_active ? Event::ActivityType::ACTIVE
                        : Event::ActivityType::INACTIVE;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(e);
    }

    /** Adds a Event::EventType::POT_MOVED event to the queue. */
    void addPotMoved(uint16_t pot_id, float new_position)
    {
        Event e;
        e.type                      = Event::EventType::POT_MOVED;
        e.as_pot_moved.id           = pot_id;
        e.as_pot_moved.new_position = new_position;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(e);
    }

    /** Adds a Event::EventType::POT_ACTIVITY_CHANGED event to the queue. */
    void addPotActivityChanged(uint16_t pot_id, bool is_active)
    {
        Event e;
        e.type                       = Event::EventType::POT_ACTIVITY_CHANGED;
        e.as_pot_activity_changed.id = pot_id;
        e.as_pot_activity_changed.new_activity_type
            = is_active ? Event::ActivityType::ACTIVE
                        : Event::ActivityType::INACTIVE;
        ScopedIrqBlocker sIrqBl;
        events_.Overwrite(e);
    }

    /** Removes and returns an event from the queue. */
    Event getAndRemoveNextEvent()
    {
        ScopedIrqBlocker sIrqBl;
        if(events_.isEmpty())
        {
            Event m;
            m.type = Event::EventType::INVALID;
            return m;
        }
        else
        {
            return events_.ImmediateRead();
        }
    }

    /** Returns true, if the queue is empty. */
    bool isQueueEmpty()
    {
        ScopedIrqBlocker sIrqBl;
        return events_.isEmpty();
    }

  private:
    RingBuffer<Event, 256> events_;
};

} // namespace daisy
