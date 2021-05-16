#pragma once

#include "AbstractMenu.h"

namespace daisy
{
/** @brief A menu page for small screens
 *  @author jelliesen
 *  @addtogroup ui
 * 
 *  This class builds upon the menu logic of AbstractMenu and adds 
 *  drawing routines that are suitable for small screens.
 * 
 *  By default, it will paint to the canvas returned by 
 *  `UI::GetPrimaryOneBitGraphicsDisplayId()`. It can also be 
 *  configured to paint to a different canvas.
 * 
 *  Each item will occupy the entire display.
 *  FullScreenItemMenu uses the LookAndFeel system to draw draw the
 *  items. This means that you can create your own graphics design 
 *  by creating your own LookAndFeel based on the OneBitGraphicsLookAndFeel
 *  class and apply that either globally (UI::SetOneBitGraphicsLookAndFeel())
 *  or to this page only (UiPage::SetOneBitGraphicsLookAndFeel()).
 */
class FullScreenItemMenu : public AbstractMenu
{
  public:
    /** Call this to initialize the menu. It's okay to re-initialize a 
     *  FullScreenItemMenu multiple times, even while it's displayed on the UI.
     * @param orientation       Controls which pair of arrow buttons are used for 
     *                          selection / editing
     * @param items             An array of ItemConfig that determine which items are 
     *                          available in the menu.
     * @param numItems          The number of items in the `items` array.
     * @param allowEntering     Globally controls if the Ok button can enter items 
     *                          for editing. If you have a physical controls that can edit 
     *                          selected items directly (value slider, a second arrow button 
     *                          pair, value encoder) you can set this to false, otherwise you
     *                          set it to true so that the controls used for selecting items 
     *                          can now also be used to edit the values.
     */
    void Init(AbstractMenu::Orientation       orientation,
              const AbstractMenu::ItemConfig* items,
              uint16_t                        numItems,
              bool                            allowEntering);

    /** Call this to change which canvas this menu will draw to. The canvas
     *  must be a `OneBitGraphicsDisplay`, e.g. the `OledDisplay` class.
     *  If `canvasId == UI::invalidCanvasId` then this menu will draw to the
     *  canvas returned by `UI::GetPrimaryOneBitGraphicsDisplayId()`. This
     *  is also the default behaviour.
     */
    void SetOneBitGraphicsDisplayToDrawTo(uint16_t canvasId);

    // inherited from UiPage
    void Draw(const UiCanvasDescriptor& canvas) override;

  private:

    uint16_t canvasIdToDrawTo_ = UI::invalidCanvasId;
};
} // namespace daisy
