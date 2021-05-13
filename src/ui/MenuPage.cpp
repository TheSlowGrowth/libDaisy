#include "MenuPage.h"
#include "util/FixedCapStr.h"

namespace daisy
{
void MenuPage::SetOrientation(MenuPageOrientation orientation)
{
    orientation_ = orientation;
}

void MenuPage::AddItem(const MenuPage::ItemConfig& itemToAdd)
{
    items_.PushBack(itemToAdd);
}

void MenuPage::RemoveAllItems()
{
    items_.Clear();
    currentSelection_ = 0;
    isEntered_        = false;
}

// inherited from UiPage
bool MenuPage::OnOkayButton(uint8_t numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;

    if(CanItemBeModified(currentSelection_))
    {
        isEntered_ = !isEntered_;
    }
    else
    {
        isEntered_ = false;
        OnItemEnter(currentSelection_);
    }
    return true;
}

bool MenuPage::OnCancelButton(uint8_t numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;
    Close();
    return true;
}

bool MenuPage::OnArrowButton(ArrowButtonType arrowType, uint8_t numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;

    if(orientation_ == MenuPageOrientation::leftRightSelectUpDownModify)
    {
        if(arrowType == ArrowButtonType::down)
            ModifyItemValue(currentSelection_, -1, 0, isFuncButtonDown_);
        else if(arrowType == ArrowButtonType::up)
            ModifyItemValue(currentSelection_, 1, 0, isFuncButtonDown_);
        else if(isEntered_)
        {
            if(arrowType == ArrowButtonType::left)
                ModifyItemValue(currentSelection_, -1, 0, isFuncButtonDown_);
            else if(arrowType == ArrowButtonType::right)
                ModifyItemValue(currentSelection_, 1, 0, isFuncButtonDown_);
        }
        else
        {
            if((arrowType == ArrowButtonType::left) && (currentSelection_ > 0))
                currentSelection_--;
            else if((arrowType == ArrowButtonType::right)
                    && (currentSelection_ < items_.GetNumElements() - 1))
                currentSelection_++;
        }
    }
    else
    // orientation_ == Orientation::upDownSelectLeftRightModify
    {
        if(arrowType == ArrowButtonType::left)
            ModifyItemValue(currentSelection_, -1, 0, isFuncButtonDown_);
        else if(arrowType == ArrowButtonType::right)
            ModifyItemValue(currentSelection_, 1, 0, isFuncButtonDown_);
        else if(isEntered_)
        {
            if(arrowType == ArrowButtonType::down)
                ModifyItemValue(currentSelection_, -1, 0, isFuncButtonDown_);
            else if(arrowType == ArrowButtonType::up)
                ModifyItemValue(currentSelection_, 1, 0, isFuncButtonDown_);
        }
        else
        {
            if((arrowType == ArrowButtonType::up) && (currentSelection_ > 0))
                currentSelection_--;
            else if((arrowType == ArrowButtonType::down)
                    && (currentSelection_ < items_.GetNumElements() - 1))
                currentSelection_++;
        }
    }
    return true;
}

bool MenuPage::OnFunctionButton(uint8_t numberOfPresses)
{
    isFuncButtonDown_ = numberOfPresses > 0;
    return true;
}

bool MenuPage::OnEncoderTurned(uint16_t encoderID,
                               int16_t  turns,
                               uint16_t stepsPerRevolution)
{
    if(auto* ui = GetParentUI())
    {
        // scroll or edit with the menu encoder
        if(encoderID == ui->GetSpecialControlIds().menuEncoderId)
        {
            // edit value
            if(isEntered_)
                ModifyItemValue(currentSelection_,
                                turns,
                                stepsPerRevolution,
                                isFuncButtonDown_);
            else
            // scroll through menu
            {
                int16_t result = currentSelection_ + turns;
                currentSelection_
                    = (result < 0)
                          ? 0
                          : ((result >= uint16_t(items_.GetNumElements()))
                                 ? uint16_t(items_.GetNumElements()) - 1
                                 : result);
            }
        }
        // edit with the value encoder
        if(encoderID == ui->GetSpecialControlIds().valueEncoderId)
            ModifyItemValue(currentSelection_,
                            turns,
                            stepsPerRevolution,
                            isFuncButtonDown_);
    }
    return true;
}

bool MenuPage::OnPotMoved(uint16_t potID, float newPosition)
{
    if(auto* ui = GetParentUI())
    {
        // edit with the value slider
        if(potID == ui->GetSpecialControlIds().valuePotId)
            if(isEntered_)
                ModifyItemValue(
                    currentSelection_, newPosition, isFuncButtonDown_);
    }
    return true;
}

void MenuPage::OnShow()
{
    currentSelection_ = 0;
    isEntered_        = false;
    isFuncButtonDown_ = false;
}

void MenuPage::Draw(const UiCanvasDescriptor& canvas)
{
    // no items?!
    if(currentSelection_ >= items_.GetNumElements())
        return;

    if(auto* ui = GetParentUI())
        if(ui->GetPrimaryOneBitGraphicsDisplayId() == canvas.id_)
        {
            OneBitGraphicsDisplay& display
                = *(OneBitGraphicsDisplay*)(canvas.handle_);
            DrawItem(currentSelection_, display, isEntered_);
        }
}

void MenuPage::DrawItem(uint16_t               itemIdx,
                        OneBitGraphicsDisplay& display,
                        bool                   isEntered)
{
    if(itemIdx >= items_.GetNumElements())
        return;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    const auto& laf  = GetParentUI()->GetLookAndFeel();
    switch(type)
    {
        case ItemType::callbackFunctionItem:
            laf.DrawMenuPageTextItem(display,
                                     orientation_,
                                     itemIdx,
                                     items_.GetNumElements(),
                                     item.text,
                                     true);
            break;
        case ItemType::checkboxItem:
            laf.DrawMenuPageCheckboxItem(display,
                                         orientation_,
                                         itemIdx,
                                         items_.GetNumElements(),
                                         item.text,
                                         *item.asCheckboxItem.valueToModify);
            break;
        case ItemType::mappedValueItem:
            laf.DrawMenuPageValueItem(display,
                                      orientation_,
                                      itemIdx,
                                      items_.GetNumElements(),
                                      item.text,
                                      item.asMappedValueItem.valueToModify,
                                      isEntered);
            break;
        case ItemType::openSubMenuItem:
            laf.DrawMenuPageOpenSubMenuItem(display,
                                            orientation_,
                                            itemIdx,
                                            items_.GetNumElements(),
                                            item.text);
            break;
        case ItemType::closeMenuItem:
            laf.DrawMenuPageCloseMenuItem(display,
                                          orientation_,
                                          itemIdx,
                                          items_.GetNumElements(),
                                          item.text);
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->Draw(
                display, itemIdx, items_.GetNumElements(), isEntered);
            break;
    }
}

bool MenuPage::CanItemBeModified(uint16_t itemIdx)
{
    if(itemIdx >= items_.GetNumElements())
        return false;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    switch(type)
    {
        case ItemType::callbackFunctionItem:
        case ItemType::checkboxItem:
        case ItemType::closeMenuItem:
        case ItemType::openSubMenuItem: return false;
        case ItemType::mappedValueItem: return true;
        case ItemType::customItem:
            return item.asCustomItem.itemObject->CanBeModified();
        default: return false;
    }
}
void MenuPage::ModifyItemValue(uint16_t itemIdx,
                               int16_t  increments,
                               uint16_t stepsPerRevolution,
                               bool     isFunctionButtonPressed)
{
    if(itemIdx >= items_.GetNumElements())
        return;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    switch(type)
    {
        case ItemType::callbackFunctionItem: break;
        case ItemType::checkboxItem:
            *item.asCheckboxItem.valueToModify = (increments > 0);
            break;
        case ItemType::closeMenuItem: break;
        case ItemType::openSubMenuItem: break;
        case ItemType::mappedValueItem:
            item.asMappedValueItem.valueToModify->Step(increments,
                                                       isFunctionButtonPressed);
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->ModifyValue(
                increments, stepsPerRevolution, isFunctionButtonPressed);
            break;
    }
}
void MenuPage::ModifyItemValue(uint16_t itemIdx,
                               float    valueSliderPosition0To1,
                               bool     isFunctionButtonPressed)
{
    if(itemIdx >= items_.GetNumElements())
        return;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    switch(type)
    {
        case ItemType::callbackFunctionItem: break;
        case ItemType::checkboxItem:
            *item.asCheckboxItem.valueToModify
                = (valueSliderPosition0To1 > 0.5f);
            break;
        case ItemType::closeMenuItem: break;
        case ItemType::openSubMenuItem: break;
        case ItemType::mappedValueItem:
            item.asMappedValueItem.valueToModify->SetFrom0to1(valueSliderPosition0To1);
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->ModifyValue(valueSliderPosition0To1,
                                                      isFunctionButtonPressed);
            break;
    }
}

void MenuPage::OnItemEnter(uint16_t itemIdx)
{
    if(itemIdx >= items_.GetNumElements())
        return;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    switch(type)
    {
        case ItemType::callbackFunctionItem:
            item.asCallbackFunctionItem.callbackFunction(
                item.asCallbackFunctionItem.context);
            break;
        case ItemType::checkboxItem:
            *item.asCheckboxItem.valueToModify
                = !*item.asCheckboxItem.valueToModify;
            break;
        case ItemType::closeMenuItem: Close(); break;
        case ItemType::openSubMenuItem:
            if(auto* ui = GetParentUI())
                ui->OpenPage(*item.asOpenSubMenuItem.pageToOpen);
            break;
        case ItemType::mappedValueItem:
            // no "on enter" action
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->OnEnter();
            break;
    }
}

// ======================================================================================
// Look And Feel Implementation
// ======================================================================================

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawMenuPageTextItem(
    OneBitGraphicsDisplay& display,
    MenuPageOrientation    menuOrientation,
    int                    currentIndex,
    int                    numItemsTotal,
    const char*            text,
    bool                   hasAction) const
{
    auto remainingBounds = display.GetBounds();
    auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight_);
    DrawTopRow(display,
               menuOrientation,
               currentIndex,
               numItemsTotal,
               text,
               topRowRect,
               true);
}

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawMenuPageCheckboxItem(
    OneBitGraphicsDisplay& display,
    MenuPageOrientation    menuOrientation,
    int                    currentIndex,
    int                    numItemsTotal,
    const char*            name,
    bool                   isChecked) const
{
    auto remainingBounds = display.GetBounds();
    auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight_);
    DrawTopRow(display,
               menuOrientation,
               currentIndex,
               numItemsTotal,
               name,
               topRowRect,
               true);

    // draw the checkbox
    auto checkboxBounds = remainingBounds.WithSizeKeepingCenter(12, 12);
    display.DrawRect(checkboxBounds, true, false);
    if(isChecked)
        display.DrawRect(checkboxBounds.Reduced(3), true, true);
}

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::
    DrawMenuPageValueItem(OneBitGraphicsDisplay& display,
                                           MenuPageOrientation menuOrientation,
                                           int                 currentIndex,
                                           int                 numItemsTotal,
                                           const char*         name,
                                           const MappedValue*  value,
                                           bool                isEditing) const
{
    auto remainingBounds = display.GetBounds();
    auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight_);
    DrawTopRow(display,
               menuOrientation,
               currentIndex,
               numItemsTotal,
               name,
               topRowRect,
               !isEditing);

    // draw the value
    FixedCapStr<20> valueStr;
    value->AppentToString(valueStr);
    DrawValueText(display, valueStr, remainingBounds, isEditing);
}

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::
    DrawMenuPageOpenSubMenuItem(OneBitGraphicsDisplay& display,
                                MenuPageOrientation    menuOrientation,
                                int                    currentIndex,
                                int                    numItemsTotal,
                                const char*            text) const
{
    auto remainingBounds = display.GetBounds();
    auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight_);
    DrawTopRow(display,
               menuOrientation,
               currentIndex,
               numItemsTotal,
               text,
               topRowRect,
               true);

    DrawValueText(display, "...", remainingBounds, false);
}

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawMenuPageCloseMenuItem(
    OneBitGraphicsDisplay& display,
    MenuPageOrientation    menuOrientation,
    int                    currentIndex,
    int                    numItemsTotal,
    const char*            text) const
{
    auto remainingBounds = display.GetBounds();
    auto topRowRect      = remainingBounds.RemoveFromTop(topRowHeight_);
    DrawTopRow(display,
               menuOrientation,
               currentIndex,
               numItemsTotal,
               text,
               topRowRect,
               true);

    DrawValueText(display, "...", remainingBounds, false);
}


void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawTopRow(
    OneBitGraphicsDisplay& display,
    MenuPageOrientation    menuOrientation,
    int                    currentIndex,
    int                    numItemsTotal,
    const char*            text,
    Rectangle              rect,
    bool                   isSelected) const
{
    const bool hasPrev = currentIndex > 0;
    const bool hasNext = currentIndex < numItemsTotal - 1;
    // draw the arrows
    if(menuOrientation == MenuPageOrientation::leftRightSelectUpDownModify)
        rect = DrawLRArrowsAndGetRemRect(
            display, rect, hasPrev, hasNext, isSelected);
    else
        rect = DrawUDArrowsAndGetRemRect(
            display, rect, hasPrev, hasNext, isSelected);
    display.WriteStringAligned(
        text, Font_11x18, rect, Alignment::centered, true);
}

Rectangle
MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawUDArrowsAndGetRemRect(
    OneBitGraphicsDisplay& display,
    Rectangle              topRowRect,
    bool                   upAvailable,
    bool                   downAvailable,
    bool                   selected) const
{
    auto upArrowRect = topRowRect.RemoveFromLeft(9).WithSizeKeepingCenter(9, 5);
    auto downArrowRect
        = topRowRect.RemoveFromRight(9).WithSizeKeepingCenter(9, 5);

    if(upAvailable)
    {
        for(int16_t y = upArrowRect.GetBottom() - 1; y >= upArrowRect.GetY();
            y--)
        {
            if(selected)
                display.DrawLine(
                    upArrowRect.GetX(), y, upArrowRect.GetRight(), y, true);
            else
            {
                display.DrawPixel(upArrowRect.GetX(), y, true);
                display.DrawPixel(upArrowRect.GetRight(), y, true);
            }
            upArrowRect = upArrowRect.Reduced(1, 0);
            if(upArrowRect.IsEmpty())
                break;
        }
    }
    if(downAvailable)
    {
        for(int16_t y = downArrowRect.GetY(); y < upArrowRect.GetBottom(); y++)
        {
            if(selected)
                display.DrawLine(
                    downArrowRect.GetX(), y, downArrowRect.GetRight(), y, true);
            else
            {
                display.DrawPixel(downArrowRect.GetX(), y, true);
                display.DrawPixel(downArrowRect.GetRight(), y, true);
            }
            downArrowRect = downArrowRect.Reduced(1, 0);
            if(downArrowRect.IsEmpty())
                break;
        }
    }

    return topRowRect;
}

Rectangle
MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawLRArrowsAndGetRemRect(
    OneBitGraphicsDisplay& display,
    Rectangle              topRowRect,
    bool                   leftAvailable,
    bool                   rightAvailable,
    bool                   selected) const
{
    auto leftArrowRect
        = topRowRect.RemoveFromLeft(9).WithSizeKeepingCenter(5, 9);
    auto rightArrowRect
        = topRowRect.RemoveFromRight(9).WithSizeKeepingCenter(5, 9);

    if(leftAvailable)
    {
        for(int16_t x = leftArrowRect.GetRight() - 1; x >= leftArrowRect.GetX();
            x--)
        {
            if(selected)
                display.DrawLine(x,
                                 leftArrowRect.GetY(),
                                 x,
                                 leftArrowRect.GetBottom(),
                                 true);
            else
            {
                display.DrawPixel(x, leftArrowRect.GetY(), true);
                display.DrawPixel(x, leftArrowRect.GetBottom(), true);
            }
            leftArrowRect = leftArrowRect.Reduced(0, 1);
            if(leftArrowRect.IsEmpty())
                break;
        }
    }
    if(rightAvailable)
    {
        for(int16_t x = rightArrowRect.GetX(); x < rightArrowRect.GetRight();
            x++)
        {
            if(selected)
                display.DrawLine(x,
                                 rightArrowRect.GetY(),
                                 x,
                                 rightArrowRect.GetBottom(),
                                 true);
            else
            {
                display.DrawPixel(x, rightArrowRect.GetY(), true);
                display.DrawPixel(x, rightArrowRect.GetBottom(), true);
            }
            rightArrowRect = rightArrowRect.Reduced(0, 1);
            if(rightArrowRect.IsEmpty())
                break;
        }
    }

    return topRowRect;
}

void MenuPageLaF::OneBitGraphicsLookAndFeelFunctions::DrawValueText(
    OneBitGraphicsDisplay& display,
    const char*            text,
    Rectangle              rect,
    bool                   isBeingEdited) const
{
    auto drawnRect = display.WriteStringAligned(
        text, Font_11x18, rect, Alignment::centered, true);
    if(isBeingEdited)
    {
        const auto y = drawnRect.GetBottom() + 2;
        display.DrawLine(drawnRect.GetX(), y, drawnRect.GetRight(), y, true);
    }
}

} // namespace daisy