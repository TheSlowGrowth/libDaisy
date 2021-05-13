#pragma once

#include "hid/disp/display.h"
#include "util/MappedValue.h"
#include "UI.h"

namespace daisy
{
/** @brief Base class for complex menus.
 *  @author jelliesen
 *  @addtogroup ui
 * 
 * This is the base class for any form of UiPage that displays a menu with multiple items.
 * It handles all the logic behind a menu (selecting items, editing items, etc.) but doesn't
 * implement any form of drawing.
 * Child classes can inherit from this and implement the drawing routines by overriding
 * UiPage::Draw().
 */
class AbstractMenu : public UiPage
{
  public:
    /** Controls which buttons are used to navigate back and forth between the menu 
     *  items (selection buttons) and which buttons can be used to modify their value 
     *  directly without pressing the enter button first (modify buttons; these don't
     *  have to be available).
     *  @see AbstractMenuPage
     */
    enum class Orientation
    {
        /** left/right buttons => selection buttons, up/down => value buttons */
        leftRightSelectUpDownModify,
        /** up/down buttons => selection buttons, left/right => value buttons */
        upDownSelectLeftRightModify,
    };

    /** The types of entries that can be added to the menu. */
    enum class ItemType
    {
        /** Displays a text and calls a callback function when activated with the enter button */
        callbackFunctionItem,
        /** Displays a name and a checkbox. When selected, the modify keys will allow
         *  to change the value directly. Pressing the enter button toggles the value. */
        checkboxItem,
        /** Displays a name and a value (with unit) from a MappedValue. When selected, the modify keys will allow
         *  to change the value directly. Pressing the enter button allows to change the value with
         *  the selection buttons as well. */
        valueItem,
        /** Displays a name and opens another UiPage when selected. */
        openSubMenuItem,
        /** Displays a text and closes the menu page when selected. This is useful when no cancel 
         *  button is available to close a menu and return to the page below. */
        closeMenuItem,
        /** A custom item. @see CustomItem */
        customItem,
    };

    /** Base class for a custom menu item */
    class CustomItem
    {
      public:
        virtual ~CustomItem() {}

        /** Draws the item to a OneBitGraphicsDisplay.
         * @param display       The display to draw to
         * @param currentIndex  The index in the menu
         * @param numItemsTotal The total number of items in the menu
         * @param isEntered     True if the enter button was pressed and the value is being edited directly.
         */
        virtual void Draw(OneBitGraphicsDisplay& display,
                          int                    currentIndex,
                          int                    numItemsTotal,
                          bool                   isEntered)
            = 0;

        /** Returns true, if this item can be modified with the modify buttons, 
         *  an encoder or the value potentiometer */
        virtual bool CanBeModified() { return false; }

        /** Called when the encoder of the buttons are used to modify the value. */
        virtual void ModifyValue(int16_t  increments,
                                 uint16_t stepsPerRevolution,
                                 bool     isFunctionButtonPressed)
        {
            (void)(increments);              // silence unused variable warnings
            (void)(stepsPerRevolution);      // silence unused variable warnings
            (void)(isFunctionButtonPressed); // silence unused variable warnings
        };

        /** Called when the value slider is used to modify the value. */
        virtual void ModifyValue(float valueSliderPosition0To1,
                                 bool  isFunctionButtonPressed)
        {
            (void)(valueSliderPosition0To1); // silence unused variable warnings
            (void)(isFunctionButtonPressed); // silence unused variable warnings
        };

        /** Called when the enter button is pressed (and the CanBeModified() returns false). */
        virtual void OnEnter(){};
    };

    struct ItemConfig
    {
        /** The type of item */
        ItemType type = ItemType::closeMenuItem;
        /** The name/text to display */
        const char* text = "";

        /** additional properties that depend on the value of `type` */
        union
        {
            /** Properties for type == ItemType::callbackFunctionItem */
            struct
            {
                void (*callbackFunction)(void* context);
                void* context;
            } asCallbackFunctionItem;

            /** Properties for type == ItemType::checkboxItem */
            struct
            {
                /** The variable to modify. */
                bool* valueToModify;
            } asCheckboxItem;

            /** Properties for type == ItemType::valueItem */
            struct
            {
                /** The variable to modify. */
                MappedValue* valueToModify;
            } asMappedValueItem;

            /** Properties for type == ItemType::openSubMenuItem */
            struct
            {
                /** The UiPage to open when the enter button is pressed.
                 *  The object must stay alive longer than the MenuPage, 
                 *  e.g. as a global variable. */
                UiPage* pageToOpen;
            } asOpenSubMenuItem;

            /** Properties for type == ItemType::customItem */
            struct
            {
                /** The CustomItem to display. The object provided here must 
                 *  stay alive longer than the MenuPage, e.g. as a global variable. */
                CustomItem* itemObject;
            } asCustomItem;
        };
    };

    AbstractMenu() = default;
    virtual ~AbstractMenu() override {}

    uint16_t          GetNumItems() const { return numItems_; }
    const ItemConfig& GetItem(uint16_t itemIdx) const
    {
        return items_[itemIdx];
    }
    void    SelectItem(uint16_t itemIdx);
    int16_t GetSelectedItemIdx() const { return selectedItemIdx_; }

    // inherited from UiPage
    bool OnOkayButton(uint8_t numberOfPresses) override;
    bool OnCancelButton(uint8_t numberOfPresses) override;
    bool OnArrowButton(ArrowButtonType arrowType,
                       uint8_t         numberOfPresses) override;
    bool OnFunctionButton(uint8_t numberOfPresses) override;
    bool OnEncoderTurned(uint16_t encoderID,
                         int16_t  turns,
                         uint16_t stepsPerRevolution) override;
    bool OnPotMoved(uint16_t potID, float newPosition) override;
    void OnShow() override;

  protected:
    /** Call this from your child class to initialize the menu */
    void Init(Orientation       orientation,
              const ItemConfig* items,
              uint16_t          numItems,
              bool              allowEntering);

    /** Returns the state of the function button. */
    bool IsFunctionButtonDown() const { return isFuncButtonDown_; }

    /** The orientation of the menu. This is used to determine 
     *  which function the arrow keys will be assigned to. */
    Orientation orientation_ = Orientation::upDownSelectLeftRightModify;
    /** A list of items to include in the menu. */
    const ItemConfig* items_ = nullptr;
    /** The number of items in `items_` */
    uint16_t numItems_ = 0;
    /** The currently selected item index */
    int16_t selectedItemIdx_ = -1;
    /** If true, the menu allows "entering" an item to modify 
     *  its value with the encoder / selection buttons. */
    bool allowEntering_ = true;
    /** If true, the currently selected item index is 
     *  "entered" so that it can be edited with the encoder/
     *  selection buttons. */
    bool isEntered_ = false;

  private:
    AbstractMenu(const AbstractMenu& other) = delete;
    AbstractMenu& operator=(const AbstractMenu& other) = delete;

    bool CanItemBeModified(uint16_t itemIdx);
    void ModifyItemValue(uint16_t itemIdx,
                         int16_t  increments,
                         uint16_t stepsPerRevolution,
                         bool     isFunctionButtonPressed);
    void ModifyItemValue(uint16_t itemIdx,
                         float    valueSliderPosition0To1,
                         bool     isFunctionButtonPressed);
    void OnItemEnter(uint16_t itemIdx);

    bool isFuncButtonDown_ = false;
};


} // namespace daisy