#include <gtest/gtest.h>
#include "ui/AbstractMenu.h"
#include <vector>

using namespace daisy;

/** Exposes some of the internals of AbstractMenu to make them 
 *  accessible from unit tests.
 */
class ExposedAbstractMenu : public AbstractMenu
{
  public:
    void Init(Orientation       orientation,
              const ItemConfig* items,
              uint16_t          numItems,
              bool              allowEntering)
    {
        AbstractMenu::Init(orientation, items, numItems, allowEntering);
    }

    void AddDummyItemsAndInit(Orientation orientation,
                              int         numItemsToAdd,
                              bool        allowEntering)
    {
        // add a bunch of "close" items. Why not?!
        itemConfigs_.resize(numItemsToAdd);
        for(auto&& item : itemConfigs_)
        {
            item.type = AbstractMenu::ItemType::closeMenuItem;
            item.text = "close";
        }
        AbstractMenu::Init(
            orientation, itemConfigs_.data(), numItemsToAdd, allowEntering);
    }

    AbstractMenu::Orientation GetOrientation() { return orientation_; }
    bool                      AllowsEntering() { return allowEntering_; }
    bool                      IsEntered() { return isEntered_; }
    bool IsFunctionButtonDown() { return AbstractMenu::IsFunctionButtonDown(); }

    void Draw(const UiCanvasDescriptor& /* canvas */) override {}

  private:
    std::vector<AbstractMenu::ItemConfig> itemConfigs_;
};

TEST(ui_AbstractMenu, a_stateAfterConstruction)
{
    ExposedAbstractMenu menu;

    EXPECT_EQ(menu.GetNumItems(), 0);
    EXPECT_EQ(menu.GetSelectedItemIdx(), -1);
}

TEST(ui_AbstractMenu, b_stateAfterInit)
{
    // initializes the menu with two items,
    // checks the state after initialisation.

    ExposedAbstractMenu menu;

    {
        // initialize the menu with 2 items
        const int kNumItems = 2;
        menu.AddDummyItemsAndInit(
            AbstractMenu::Orientation::leftRightSelectUpDownModify,
            kNumItems,
            true);

        EXPECT_EQ(menu.GetNumItems(), kNumItems);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);
        EXPECT_EQ(menu.GetOrientation(),
                  AbstractMenu::Orientation::leftRightSelectUpDownModify);
        EXPECT_EQ(menu.AllowsEntering(), true);
        EXPECT_EQ(menu.IsFunctionButtonDown(), false);
    }

    {
        // initialize the menu again, this time with 4 items and
        // different setings
        const int kNumItems = 4;
        menu.AddDummyItemsAndInit(
            AbstractMenu::Orientation::upDownSelectLeftRightModify,
            kNumItems,
            false);

        EXPECT_EQ(menu.GetNumItems(), kNumItems);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);
        EXPECT_EQ(menu.GetOrientation(),
                  AbstractMenu::Orientation::upDownSelectLeftRightModify);
        EXPECT_EQ(menu.AllowsEntering(), false);
        EXPECT_EQ(menu.IsFunctionButtonDown(), false);
    }
}


TEST(ui_AbstractMenu, c_selectWithButtons)
{
    // initializes the menu with some items,
    // selects them with the arrow buttons

    ExposedAbstractMenu menu;
    {
        // init with left & right as the select buttons
        menu.AddDummyItemsAndInit(
            AbstractMenu::Orientation::leftRightSelectUpDownModify, 4, true);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);

        // select an item manually
        menu.SelectItem(2);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 2);

        // press the right button
        menu.OnArrowButton(ArrowButtonType::right, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 3);
        // press the right button again
        menu.OnArrowButton(ArrowButtonType::right, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 3); // we already were at the last item!

        // select an item manually
        menu.SelectItem(1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);

        // press the left button
        menu.OnArrowButton(ArrowButtonType::left, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);
        // press the left button again
        menu.OnArrowButton(ArrowButtonType::left, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0); // we already were at the first item!

        // up and down buttons are not configured to change the selection!
        menu.SelectItem(1);
        menu.OnArrowButton(ArrowButtonType::up, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);
        menu.OnArrowButton(ArrowButtonType::down, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);
    }
    {
        // repeat the same test, this time with up & down buttons 
        menu.AddDummyItemsAndInit(
            AbstractMenu::Orientation::upDownSelectLeftRightModify, 4, true);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);

        // select an item manually
        menu.SelectItem(2);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 2);

        // press the down button
        menu.OnArrowButton(ArrowButtonType::down, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 3);
        // press the down button again
        menu.OnArrowButton(ArrowButtonType::down, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 3); // we already were at the last item!

        // select an item manually
        menu.SelectItem(1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);

        // press the up button
        menu.OnArrowButton(ArrowButtonType::up, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0);
        // press the up button again
        menu.OnArrowButton(ArrowButtonType::up, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 0); // we already were at the first item!

        // left and right buttons are not configured to change the selection!
        menu.SelectItem(1);
        menu.OnArrowButton(ArrowButtonType::left, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);
        menu.OnArrowButton(ArrowButtonType::right, 1);
        EXPECT_EQ(menu.GetSelectedItemIdx(), 1);
    }
}