#pragma once

#include "hid/disp/display.h"
#include "util/MappedValue.h"

namespace daisy
{
/** Controls which buttons are used to navigate back and forth between the menu 
 *  items (selection buttons) and which buttons can be used to modify their value 
 *  directly without pressing the enter button first (modify buttons; these don't
 *  have to be available).
 *  @see MenuPage
 */
enum class MenuPageOrientation
{
    /** left/right buttons => selection buttons, up/down => value buttons */
    leftRightSelectUpDownModify,
    /** up/down buttons => selection buttons, left/right => value buttons */
    upDownSelectLeftRightModify,
};

namespace MenuPageLaF
{
    /** Implements drawing routines. */
    class OneBitGraphicsLookAndFeelFunctions
    {
      public:
        virtual ~OneBitGraphicsLookAndFeelFunctions() {}

        virtual void DrawMenuPageTextItem(OneBitGraphicsDisplay& display,
                                          MenuPageOrientation menuOrientation,
                                          int                 currentIndex,
                                          int                 numItemsTotal,
                                          const char*         text,
                                          bool                hasAction) const;
        virtual void
                     DrawMenuPageCheckboxItem(OneBitGraphicsDisplay& display,
                                              MenuPageOrientation    menuOrientation,
                                              int                    currentIndex,
                                              int                    numItemsTotal,
                                              const char*            name,
                                              bool                   isChecked) const;
        virtual void DrawMenuPageValueItem(OneBitGraphicsDisplay& display,
                                           MenuPageOrientation menuOrientation,
                                           int                 currentIndex,
                                           int                 numItemsTotal,
                                           const char*         name,
                                           const MappedValue*  value,
                                           bool                isEditing) const;
        virtual void
        DrawMenuPageOpenSubMenuItem(OneBitGraphicsDisplay& display,
                                    MenuPageOrientation    menuOrientation,
                                    int                    currentIndex,
                                    int                    numItemsTotal,
                                    const char*            text) const;
        virtual void
        DrawMenuPageCloseMenuItem(OneBitGraphicsDisplay& display,
                                  MenuPageOrientation    menuOrientation,
                                  int                    currentIndex,
                                  int                    numItemsTotal,
                                  const char*            text) const;

      private:
        Rectangle DrawUDArrowsAndGetRemRect(OneBitGraphicsDisplay& display,
                                            Rectangle              topRowRect,
                                            bool                   upAvailable,
                                            bool downAvailable,
                                            bool selected) const;
        Rectangle DrawLRArrowsAndGetRemRect(OneBitGraphicsDisplay& display,
                                            Rectangle              topRowRect,
                                            bool leftAvailable,
                                            bool rightAvailable,
                                            bool selected) const;

        void DrawTopRow(OneBitGraphicsDisplay& display,
                        MenuPageOrientation    menuOrientation,
                        int                    currentIndex,
                        int                    numItemsTotal,
                        const char*            text,
                        Rectangle              rect,
                        bool                   isSelected) const;
        void DrawNameText(OneBitGraphicsDisplay& display,
                          const char*            text,
                          Rectangle              rect) const;
        void DrawValueText(OneBitGraphicsDisplay& display,
                           const char*            text,
                           Rectangle              rect,
                           bool                   isBeingEdited) const;

        static constexpr int16_t topRowHeight_ = 32;
    };
} // namespace MenuPageLaF
} // namespace daisy

#include "UI.h"

namespace daisy
{
/** @brief A MenuPage for complex menus
 *  @author jelliesen
 *  @addtogroup ui
 * 
 * This is a UiPage that displays a menu with multiple items.
 * Each item can control a value (bool int, float, string list)
 */
class MenuPage : public UiPage
{
  public:
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
        mappedValueItem,
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
                                 bool     isFunctionButtonPressed){};

        /** Called when the value slider is used to modify the value. */
        virtual void ModifyValue(float valueSliderPosition0To1,
                                 bool  isFunctionButtonPressed){};

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

            /** Properties for type == ItemType::intValueItem */
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

    virtual ~MenuPage() override {}

    void SetOrientation(MenuPageOrientation orientation);
    void AddItem(const ItemConfig& itemToAdd);
    void RemoveAllItems();

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
    void Draw(const UiCanvasDescriptor& canvas) override;

  private:
    void DrawItem(uint16_t idx, OneBitGraphicsDisplay& disp, bool isEntered);
    bool CanItemBeModified(uint16_t itemIdx);
    void ModifyItemValue(uint16_t itemIdx,
                         int16_t  increments,
                         uint16_t stepsPerRevolution,
                         bool     isFunctionButtonPressed);
    void ModifyItemValue(uint16_t itemIdx,
                         float    valueSliderPosition0To1,
                         bool     isFunctionButtonPressed);
    void OnItemEnter(uint16_t itemIdx);

    MenuPageOrientation orientation_
        = MenuPageOrientation::upDownSelectLeftRightModify;
    static constexpr int             kMaxNumItems_ = 32;
    Stack<ItemConfig, kMaxNumItems_> items_;
    uint16_t                         currentSelection_;
    bool                             isEntered_;
    bool                             isFuncButtonDown_;
};


} // namespace daisy