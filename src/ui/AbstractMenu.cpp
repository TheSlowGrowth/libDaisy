#include "AbstractMenu.h"
#include "util/FixedCapStr.h"

namespace daisy
{
void AbstractMenu::SelectItem(uint16_t itemIdx)
{
    if(itemIdx >= numItems_)
        return;
    selectedItemIdx_ = itemIdx;
    isEntered_       = false;
}

// inherited from UiPage
bool AbstractMenu::OnOkayButton(uint8_t numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;

    if(allowEntering_ && CanItemBeModified(selectedItemIdx_))
    {
        isEntered_ = !isEntered_;
    }
    else
    {
        isEntered_ = false;
        OnItemEnter(selectedItemIdx_);
    }
    return true;
}

bool AbstractMenu::OnCancelButton(uint8_t numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;
    Close();
    return true;
}

bool AbstractMenu::OnArrowButton(ArrowButtonType arrowType,
                                 uint8_t         numberOfPresses)
{
    if(numberOfPresses < 1)
        return true;

    if(orientation_ == Orientation::leftRightSelectUpDownModify)
    {
        if(arrowType == ArrowButtonType::down)
            ModifyItemValue(selectedItemIdx_, -1, 0, isFuncButtonDown_);
        else if(arrowType == ArrowButtonType::up)
            ModifyItemValue(selectedItemIdx_, 1, 0, isFuncButtonDown_);
        else if(isEntered_)
        {
            if(arrowType == ArrowButtonType::left)
                ModifyItemValue(selectedItemIdx_, -1, 0, isFuncButtonDown_);
            else if(arrowType == ArrowButtonType::right)
                ModifyItemValue(selectedItemIdx_, 1, 0, isFuncButtonDown_);
        }
        else
        {
            if((arrowType == ArrowButtonType::left) && (selectedItemIdx_ > 0))
                selectedItemIdx_--;
            else if((arrowType == ArrowButtonType::right)
                    && (selectedItemIdx_ < numItems_ - 1))
                selectedItemIdx_++;
        }
    }
    else
    // orientation_ == Orientation::upDownSelectLeftRightModify
    {
        if(arrowType == ArrowButtonType::left)
            ModifyItemValue(selectedItemIdx_, -1, 0, isFuncButtonDown_);
        else if(arrowType == ArrowButtonType::right)
            ModifyItemValue(selectedItemIdx_, 1, 0, isFuncButtonDown_);
        else if(isEntered_)
        {
            if(arrowType == ArrowButtonType::down)
                ModifyItemValue(selectedItemIdx_, -1, 0, isFuncButtonDown_);
            else if(arrowType == ArrowButtonType::up)
                ModifyItemValue(selectedItemIdx_, 1, 0, isFuncButtonDown_);
        }
        else
        {
            if((arrowType == ArrowButtonType::up) && (selectedItemIdx_ > 0))
                selectedItemIdx_--;
            else if((arrowType == ArrowButtonType::down)
                    && (selectedItemIdx_ < numItems_ - 1))
                selectedItemIdx_++;
        }
    }
    return true;
}

bool AbstractMenu::OnFunctionButton(uint8_t numberOfPresses)
{
    isFuncButtonDown_ = numberOfPresses > 0;
    return true;
}

bool AbstractMenu::OnEncoderTurned(uint16_t encoderID,
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
                ModifyItemValue(selectedItemIdx_,
                                turns,
                                stepsPerRevolution,
                                isFuncButtonDown_);
            else
            // scroll through menu
            {
                int16_t result = selectedItemIdx_ + turns;
                selectedItemIdx_
                    = (result < 0)
                          ? 0
                          : ((result >= numItems_) ? numItems_ - 1 : result);
            }
        }
        // edit with the value encoder
        if(encoderID == ui->GetSpecialControlIds().valueEncoderId)
            ModifyItemValue(
                selectedItemIdx_, turns, stepsPerRevolution, isFuncButtonDown_);
    }
    return true;
}

bool AbstractMenu::OnPotMoved(uint16_t potID, float newPosition)
{
    if(auto* ui = GetParentUI())
    {
        // edit with the value slider
        if(potID == ui->GetSpecialControlIds().valuePotId)
            if(isEntered_)
                ModifyItemValue(
                    selectedItemIdx_, newPosition, isFuncButtonDown_);
    }
    return true;
}

void AbstractMenu::OnShow()
{
    isEntered_        = false;
    isFuncButtonDown_ = false;
}

void AbstractMenu::Init(Orientation       orientation,
                        const ItemConfig* items,
                        uint16_t          numItems,
                        bool              allowEntering)
{
    orientation_   = orientation;
    items_         = items;
    numItems_      = numItems;
    allowEntering_ = allowEntering;

    selectedItemIdx_  = 0;
    isEntered_        = false;
    isFuncButtonDown_ = false;
}

bool AbstractMenu::CanItemBeModified(uint16_t itemIdx)
{
    if(itemIdx >= numItems_)
        return false;

    const auto& item = items_[itemIdx];
    const auto  type = item.type;
    switch(type)
    {
        case ItemType::callbackFunctionItem:
        case ItemType::checkboxItem:
        case ItemType::closeMenuItem:
        case ItemType::openSubMenuItem: return false;
        case ItemType::valueItem: return true;
        case ItemType::customItem:
            return item.asCustomItem.itemObject->CanBeModified();
        default: return false;
    }
}
void AbstractMenu::ModifyItemValue(uint16_t itemIdx,
                                   int16_t  increments,
                                   uint16_t stepsPerRevolution,
                                   bool     isFunctionButtonPressed)
{
    if(itemIdx >= numItems_)
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
        case ItemType::valueItem:
            item.asMappedValueItem.valueToModify->Step(increments,
                                                       isFunctionButtonPressed);
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->ModifyValue(
                increments, stepsPerRevolution, isFunctionButtonPressed);
            break;
    }
}
void AbstractMenu::ModifyItemValue(uint16_t itemIdx,
                                   float    valueSliderPosition0To1,
                                   bool     isFunctionButtonPressed)
{
    if(itemIdx >= numItems_)
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
        case ItemType::valueItem:
            item.asMappedValueItem.valueToModify->SetFrom0to1(
                valueSliderPosition0To1);
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->ModifyValue(valueSliderPosition0To1,
                                                      isFunctionButtonPressed);
            break;
    }
}

void AbstractMenu::OnItemEnter(uint16_t itemIdx)
{
    if(itemIdx >= numItems_)
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
        case ItemType::valueItem:
            // no "on enter" action
            break;
        case ItemType::customItem:
            item.asCustomItem.itemObject->OnEnter();
            break;
    }
}

} // namespace daisy