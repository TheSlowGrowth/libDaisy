#include "UI.h"

namespace daisy
{

void UiPage::close()
{
    if(parent_ != nullptr)
        parent_->closePage(*this);
}


UI::UI() {}

void UI::init(UiEventQueue& input_queue,
              uint8_t*      button_state_buffer,
              uint16_t      num_buttons)
{
    is_muted_            = false;
    queue_events_        = false;
    event_queue_         = &input_queue;
    func_bttn_id_        = UiEventQueue::kInvalidButtonId;
    ok_bttn_id_          = UiEventQueue::kInvalidButtonId;
    cancel_bttn_id_     = UiEventQueue::kInvalidButtonId;
    arrow_bttn_ids_[0]   = UiEventQueue::kInvalidButtonId;
    arrow_bttn_ids_[1]   = UiEventQueue::kInvalidButtonId;
    arrow_bttn_ids_[2]   = UiEventQueue::kInvalidButtonId;
    arrow_bttn_ids_[3]   = UiEventQueue::kInvalidButtonId;
    button_state_buffer_ = button_state_buffer;
    num_buttons_         = num_buttons;

    for(int i = 0; i < num_buttons_; i++)
        button_state_buffer_[i] = 0;
    for(int i = 0; i < kMaxNumDisplays; i++)
        last_display_update_times_[i] = 0;
}

UI::~UI()
{
    while(pages_.getNumElements() > 0)
        removePage(pages_.popBack());
}

/** Sets the button ID to be used as the OK button. Pass `kInvalidButtonId` to indicate
 *  that no such button exists. Default value: `kInvalidButtonId`
 */
void UI::setOkButtonId(uint16_t button_id)
{
    if(button_id >= num_buttons_)
        button_id = UiEventQueue::kInvalidButtonId;
    ok_bttn_id_ = button_id;
}

/** Sets the button ID to be used as the cancel button. Pass `kInvalidButtonId` to indicate
 *  that no such button exists. Default value: `kInvalidButtonId`
 */
void UI::setCancelButtonId(uint16_t button_id)
{
    if(button_id >= num_buttons_)
        button_id = UiEventQueue::kInvalidButtonId;
    cancel_bttn_id_ = button_id;
}

/** Sets the button ID to be used as the function/shift button. Pass `kInvalidButtonId` to indicate
 *  that no such button exists. Default value: `kInvalidButtonId`
 */
void UI::setFunctionButtonId(uint16_t button_id)
{
    if(button_id >= num_buttons_)
        button_id = UiEventQueue::kInvalidButtonId;
    func_bttn_id_ = button_id;
}

/** Sets the button ID to be used for one of the arrow keys. Pass `kInvalidButtonId` to indicate
 *  that no such button exists. Default value: `kInvalidButtonId`
 */
void UI::setArrowButtonId(ArrowButtonType type, uint16_t button_id)
{
    if(button_id >= num_buttons_)
        button_id = UiEventQueue::kInvalidButtonId;
    arrow_bttn_ids_[int(type)] = button_id;
}

/** Sets the button IDs to be used for the arrow keys. Pass `kInvalidButtonId` to indicate
 *  that no such button exists. Default value: `kInvalidButtonId`
 */
void UI::setArrowButtonIds(uint16_t left_id,
                           uint16_t right_id,
                           uint16_t up_id,
                           uint16_t down_id)
{
    setArrowButtonId(ArrowButtonType::LEFT, left_id);
    setArrowButtonId(ArrowButtonType::RIGHT, right_id);
    setArrowButtonId(ArrowButtonType::UP, up_id);
    setArrowButtonId(ArrowButtonType::DOWN, down_id);
}

/** Adds a Display to this user interface. The display will be redrawn
 *  and updated with the update rate specified in the Display object.
 *  The object will not be owned and is expected to stay valid.
 */
void UI::addDisplay(Display& display)
{
    if(!displays_.isFull())
        displays_.pushBack(&display);
}

/** Call this regularly to allow processing user input, redraw displays
 *  and do other "housekeeping" work.
 **/
void UI::process(uint32_t current_time_in_systicks)
{
    // handle user input
    if(!is_muted_)
    {
        while(!event_queue_->isQueueEmpty())
        {
            UiEventQueue::Event e = event_queue_->getAndRemoveNextEvent();
            if(e.type != UiEventQueue::Event::EventType::INVALID)
                processEvent(e);
        }
    }
    else if(!queue_events_)
    {
        // drop events
        while(!event_queue_->isQueueEmpty())
            event_queue_->getAndRemoveNextEvent();
    }

    // redraw displays
    for(uint32_t i = 0; i < displays_.getNumElements(); i++)
    {
        const uint32_t time_diff
            = current_time_in_systicks - last_display_update_times_[i];
        if(time_diff > displays_[i]->getUpdateRateInSysticks())
            redrawDisplay(i, current_time_in_systicks);
    }
}

/** Call this to temporarily disable processing of user input, e.g.
 *  while a project is loading. If queueEvents==true, all user input
 *  that happens while muted will be queued up and processed when the
 *  mute state is removed. If queueEvents==false, all user input that
 *  happens while muted will be discarded.
 */
void UI::mute(bool should_be_muted, bool queue_events)
{
    is_muted_     = should_be_muted;
    queue_events_ = queue_events;
}

/** Adds a new UIPage on the top of the stack of UI pages without
 *  taking ownership of the object.
 */
void UI::openPage(UiPage& page)
{
    if(page.parent_ != nullptr)
        return;
    if(pages_.isFull())
        return;

    pages_.pushBack(&page);
    page.parent_ = this;
    page.onShow();
}

/** Called to close a page: */
void UI::closePage(UiPage& page)
{
    if(page.parent_ != this)
        return;

    // find page index
    int page_index = -1;
    for(uint32_t i = pages_.getNumElements() - 1; i >= 0; i--)
        if(pages_[i] == &page)
        {
            page_index = i;
            break;
        }
    if(page_index < 0)
        return;

    // this is not the topmost page - move pages down
    // so that the stack is free of "gaps"
    if(page_index < int(pages_.getNumElements()) - 1)
    {
        for(uint32_t i = page_index; i < pages_.getNumElements() - 1; i++)
            pages_[i] = pages_[i + 1];
    }

    // close the page
    removePage(&page);
}

void UI::removePage(UiPage* page)
{
    page->onHide();
    page->parent_ = nullptr;
}

void UI::addPage(UiPage* page)
{
    page->parent_ = this;
    page->onShow();
}

void UI::processEvent(const UiEventQueue::Event& e)
{
    switch(e.type)
    {
        case UiEventQueue::Event::EventType::BUTTON_PRESSED:
            if(e.as_button_pressed.id < num_buttons_)
                button_state_buffer_[e.as_button_pressed.id] = 1;
            forwardToButtonHandler(e.as_button_pressed.id,
                                   e.as_button_pressed.num_successive_presses);
            break;
        case UiEventQueue::Event::EventType::BUTTON_RELEASED:
            if(e.as_button_released.id < num_buttons_)
                button_state_buffer_[e.as_button_released.id] = 0;
            forwardToButtonHandler(e.as_button_released.id, 0);
            break;
        case UiEventQueue::Event::EventType::ENCODER_TURNED:
            for(int32_t i = pages_.getNumElements() - 1; i >= 0; i--)
            {
                const auto id            = e.as_encoder_turned.id;
                const auto increments    = e.as_encoder_turned.increments;
                const auto steps_per_rev = e.as_encoder_turned.steps_per_rev;
                if(pages_[i]->onEncoderTurned(id, increments, steps_per_rev))
                    break;
            }
            break;
        case UiEventQueue::Event::EventType::ENCODER_ACTIVITY_CHANGED:
            for(int32_t i = pages_.getNumElements() - 1; i >= 0; i--)
            {
                const auto id = e.as_encoder_activity_changed.id;
                const auto activity_type
                    = e.as_encoder_activity_changed.new_activity_type;
                const bool is_active
                    = activity_type
                      == UiEventQueue::Event::ActivityType::ACTIVE;
                if(pages_[i]->onEncoderActivityChanged(id, is_active))
                    break;
            }
            break;
        case UiEventQueue::Event::EventType::POT_MOVED:
            for(int32_t i = pages_.getNumElements() - 1; i >= 0; i--)
            {
                const auto id           = e.as_pot_moved.id;
                const auto new_position = e.as_pot_moved.new_position;
                if(pages_[i]->onPotMoved(id, new_position))
                    break;
            }
            break;
        case UiEventQueue::Event::EventType::POT_ACTIVITY_CHANGED:
            for(int32_t i = pages_.getNumElements() - 1; i >= 0; i--)
            {
                const auto id = e.as_pot_activity_changed.id;
                const auto activity_type
                    = e.as_pot_activity_changed.new_activity_type;
                const bool is_active
                    = activity_type
                      == UiEventQueue::Event::ActivityType::ACTIVE;
                if(pages_[i]->onPotActivityChanged(id, is_active))
                    break;
            }
            break;
        case UiEventQueue::Event::EventType::INVALID:
        default: break;
    }
}

void UI::redrawDisplay(uint8_t display_index, uint32_t current_time_in_systicks)
{
    Display* display = displays_[display_index];
    int      first_to_draw;
    for(first_to_draw = int(pages_.getNumElements()) - 1; first_to_draw >= 0;
        first_to_draw--)
    {
        if(pages_[first_to_draw]->isOpaque(display))
            break;
    }

    // all pages are transparent - start with the page on the bottom
    if(first_to_draw < 0)
        first_to_draw = 0;

    display->clear();

    for(uint32_t i = first_to_draw; i < pages_.getNumElements(); i++)
    {
        pages_[i]->draw(display);
    }

    display->swapBuffersAndTransmit();
    last_display_update_times_[display_index] = current_time_in_systicks;
}

void UI::forwardToButtonHandler(const uint16_t button_id,
                                const uint8_t  number_of_presses)
{
    if(button_id == ok_bttn_id_)
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onOkayButton(number_of_presses))
                break;
        }
    }
    else if(button_id == cancel_bttn_id_)
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onCancelButton(number_of_presses))
                break;
        }
    }
    else if(button_id == func_bttn_id_)
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onFunctionButton(number_of_presses))
                break;
        }
    }
    else if(button_id == arrow_bttn_ids_[int(ArrowButtonType::LEFT)])
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onArrowButton(ArrowButtonType::LEFT,
                                        number_of_presses))
                break;
        }
    }
    else if(button_id == arrow_bttn_ids_[int(ArrowButtonType::RIGHT)])
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onArrowButton(ArrowButtonType::RIGHT,
                                        number_of_presses))
                break;
        }
    }
    else if(button_id == arrow_bttn_ids_[int(ArrowButtonType::UP)])
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onArrowButton(ArrowButtonType::UP, number_of_presses))
                break;
        }
    }
    else if(button_id == arrow_bttn_ids_[int(ArrowButtonType::DOWN)])
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onArrowButton(ArrowButtonType::DOWN,
                                        number_of_presses))
                break;
        }
    }
    else
    {
        for(int i = int(pages_.getNumElements()) - 1; i >= 0; i--)
        {
            if(pages_[i]->onButton(button_id, number_of_presses))
                break;
        }
    }
}
} // namespace daisy