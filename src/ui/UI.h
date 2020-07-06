#pragma once
#include "UiEventQueue.h"
#include "../displays/Display.h"
#include "../util_stack.h"

namespace daisy
{
class UI;

/** The type of arrow button. */
enum class ArrowButtonType
{
    /** The left arrow button. */
    LEFT = 0,
    /** The right arrow button. */
    RIGHT,
    /** The up arrow button. */
    UP,
    /** The down arrow button. */
    DOWN
};

/** Base class for a page that can be displayed on the UI. */
class UiPage
{
  public:
    UiPage() : parent_(nullptr) {}

    virtual ~UiPage();

    /** Returns true, if the page fills the entire display. A display can
	 *  be individual leds, text displays, alphanumeric displays, graphics
	 *  displays, etc. The UI class will use this to determine if underlying pages
	 *  must be drawn before this page.
	 */
    virtual bool isOpaque(Display* display) { return true; }

    /** Returns true if the page is currently active on a UI - it may not be visible, though. */
    bool isActive() { return parent_ != nullptr; }

    /** Closes the current page. This calls the parent UI and asks it to remove this page
	 *  from the page stack.
	 */
    void close();

    /** Called when the okay button is pressed or released.
	 *  @param number_of_presses 	Holds the number of successive button presses.
	 *  						It will be 1 on the first call and increasing by 1
	 *  						with each successive call. A button down event is
	 *  						signaled by numberOfButtonPresses == 0.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onOkayButton(uint8_t number_of_presses) { return true; }

    /** Called when the cancel button is pressed or released.
	 *  @param number_of_presses 	Holds the number of successive button presses.
	 *  						It will be 1 on the first call and increasing by 1
	 *  						with each successive call. A button down event is
	 *  						signaled by numberOfButtonPresses == 0.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onCancelButton(uint8_t number_of_presses) { return true; }

    /** Called when an arrow button is pressed or released.
	 *  @param arrorType 		The arrow button affected.
	 *  @param number_of_presses 	Holds the number of successive button presses.
	 *  						It will be 1 on the first call and increasing by 1
	 *  						with each successive call. A button down event is
	 *  						signaled by numberOfButtonPresses == 0.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onArrowButton(ArrowButtonType arrorType,
                               uint8_t         number_of_presses)
    {
        return true;
    }

    /** Called when the function button is pressed or released.
	 *  @param number_of_presses 	Holds the number of successive button presses.
	 *  						It will be 1 on the first call and increasing by 1
	 *  						with each successive call. A button down event is
	 *  						signaled by numberOfButtonPresses == 0.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onFunctionButton(uint8_t number_of_presses) { return true; }

    /** Called when any other button is pressed or released.
	 *  @param button_id 		The ID of the affected button.
	 *  @param number_of_presses 	Holds the number of successive button presses.
	 *  						It will be 1 on the first call and increasing by 1
	 *  						with each successive call. A button down event is
	 *  						signaled by numberOfButtonPresses == 0.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onButton(uint16_t button_id, uint8_t number_of_presses)
    {
        return true;
    }

    /** Called when an encoder is turned.
	 *  @param encoderID			The ID of the affected encoder.
	 *  @param turns		 		The number of increments, positive is clockwise.
	 *  @param stepsPerRevolution	The total number of increments per revolution on this encoder.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onEncoderTurned(uint16_t encoderID,
                                 int16_t  turns,
                                 uint16_t stepsPerRevolution)
    {
        return true;
    }

    /** Called when the user starts or stops turning an encoder.
	 *  @param encoderID			The ID of the affected encoder.
	 *  @param isCurrentlyActive	True, if the user currently moves this encoder.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onEncoderActivityChanged(uint16_t potID,
                                          bool     isCurrentlyActive)
    {
        return true;
    }

    /** Called when an encoder is turned.
	 *  @param potID				The ID of the affected potentiometer.
	 *  @param newPosition		 	The new position in the range 0 .. 1
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onPotMoved(uint16_t potID, float newPosition) { return true; }

    /** Called when the user starts or stops turning a potentiometer.
	 *  @param potID				The ID of the affected potentiometer.
	 *  @param isCurrentlyActive	True, if the user currently moves this potentiometer.
	 *  @returns	false, if you want the event to be passed on to the page below.
	 */
    virtual bool onPotActivityChanged(uint16_t potID, bool isCurrentlyActive)
    {
        return true;
    }

    /** Called when the page is added to the UI. */
    virtual void onShow(){};

    /** Called when the page is removed from the UI. */
    virtual void onHide(){};

    /** Called to make the UIPage repaint everything on a display. Use `Display::getDisplayType()`
	 *  to find out which type of display this is. The cast it to the corresponding type and do your
	 *  draw operations.
	 */
    virtual void draw(Display* display) = 0;

    /** Returns a reference to the parent UI object, or nullptr if not added to any UI at the moment. */
    UI* getParentUI() { return parent_; }

  private:
    friend class UI;
    UI* parent_;
};


/** A generic UI system. A stack of pages is displayed on a user interface
 *  that consists of a number of abstract displays (they could be LEDs,
 *  graphics displays and text displays or anything else). User input
 *  is read from a UiEventQueue and dispatched to the UiPages.
 *  Events are first passed to the topmost page and - if they can't be
 *  processed there - subsequently forwarded to the pages below.
 *
 *  Pages are painted from the bottom up so that they can overlay each other.
 *  This class makes sure that each display is updated when required without
 *  knowing anything about the actual display.
 */
class UI
{
  public:
    UI();
    ~UI();

    /** Initializes the UI.
     *  @param input_queue          The UiEventQueue to read input events from.
     *  @param button_state_buffer  A buffer for keeping track of the state of individual buttons.
     *                              It is allocated externally to avoid dynamic allocation.
     *  @param num_buttons          The number of buttons == the size of the buttonStateBuffer.
     */
    void init(UiEventQueue& input_queue,
              uint8_t*      button_state_buffer,
              uint16_t      num_buttons);

    /** Sets the button ID to be used as the OK button. Pass `INVALID_BUTTON_ID` to indicate
	 *  that no such button exists. Default value: `invalidbutton_id`
	 */
    void setOkButtonId(uint16_t button_id);

    /** Sets the button ID to be used as the cancel button. Pass `kInvalidButtonId` to indicate
	 *  that no such button exists. Default value: `kInvalidButtonId`
	 */
    void setCancelButtonId(uint16_t button_id);

    /** Sets the button ID to be used as the function/shift button. Pass `kInvalidButtonId` to indicate
	 *  that no such button exists. Default value: `kInvalidButtonId`
	 */
    void setFunctionButtonId(uint16_t button_id);

    /** Sets the button ID to be used for one of the arrow keys. Pass `kInvalidButtonId` to indicate
	 *  that no such button exists. Default value: `kInvalidButtonId`
	 */
    void setArrowButtonId(ArrowButtonType type, uint16_t button_id);

    /** Sets the button IDs to be used for the arrow keys. Pass `kInvalidButtonId` to indicate
	 *  that no such button exists. Default value: `kInvalidButtonId`
	 */
    void setArrowButtonIds(uint16_t left_id,
                           uint16_t right_id,
                           uint16_t up_id,
                           uint16_t down_id);

    /** Adds a Display to this user interface. The display will be redrawn
	 *  and updated with the update rate specified in the Display object.
	 *  The object will not be owned and is expected to stay valid.
	 */
    void addDisplay(Display& display);

    /** Call this regularly to allow processing user input, redraw displays
	 *  and do other "housekeeping" work.
	 **/
    void process(uint32_t current_time_in_systicks);

    /** Call this to temporarily disable processing of user input, e.g.
	 *  while a project is loading. If queueEvents==true, all user input
	 *  that happens while muted will be queued up and processed when the
	 *  mute state is removed. If queueEvents==false, all user input that
	 *  happens while muted will be discarded.
	 */
    void mute(bool should_be_muted, bool queue_events = false);

    /** Adds a new UIPage on the top of the stack of UI pages without
	 *  taking ownership of the object. The new page is set to be visible.
	 */
    void openPage(UiPage& page);

    /** Called to close a page. */
    void closePage(UiPage& page);

    /** Returns true if a button is currently depressed */
    bool isButtonDown(uint16_t button_id)
    {
        return button_state_buffer_[button_id];
    }

    /** Returns true, if the function button is depressed */
    bool isFuncButtonDown() { return button_state_buffer_[func_bttn_id_]; }

  private:
    bool                             is_muted_;
    bool                             queue_events_;
    static constexpr int             kMaxNumPages    = 32;
    static constexpr int             kMaxNumDisplays = 8;
    Stack<UiPage*, kMaxNumPages>     pages_;
    Stack<Display*, kMaxNumDisplays> displays_;
    uint32_t      last_display_update_times_[kMaxNumDisplays];
    UiEventQueue* event_queue_;
    uint16_t      func_bttn_id_;
    uint16_t      ok_bttn_id_;
    uint16_t      cancel_bttn_id_;
    uint16_t      arrow_bttn_ids_[4];
    uint8_t*      button_state_buffer_;
    uint8_t       num_buttons_;

    // internal
    void removePage(UiPage* page);
    void addPage(UiPage* p);
    void processEvent(const UiEventQueue::Event& m);
    void redrawDisplay(uint8_t  display_index,
                       uint32_t current_time_in_systicks);
    void forwardToButtonHandler(uint16_t button_id, uint8_t number_of_presses);
    void rebuildPageVisibilities();
};
} // namespace daisy