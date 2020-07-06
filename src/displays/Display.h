#pragma once
#include <stdint.h>

namespace daisy
{
/** Abstract base class for displays used in the UI system. Child classes
 *  provide functionality for LEDs, character displays and graphic displays.
 *  A pointer to this object is passed to UIPages and other classes in the
 *  UI system to make them draw their content. As there are several type of
 *  displays that can't easily be abstracted into a single base class, there
 *  must be another way of letting downstream classes know what type of
 *  display this is. This is done with the Type enum, that tells
 *  what child class is to be expected
 */
class Display
{
  public:
    /** The child class type that an object of type `Display` can be
	 *  casted to.
	 */
    enum class Type
    {
        /** Used to identify an unknown or invalid display. */
        INVALID = 0,
        /** Special/unspecified display. No default class corresponds to this. */
        OTHER = 1,
        /** Generic LEDs on the user interface. These can be LEDs of buttons,
		 *  indicator lights, RGB LEDs, etc. Corresponding child class: LEDDisp.
		 */
        LED = 2,
        /** Character based displays like character LCDs and alphanumeric LED
		 *  displays. Corresponding child class: CharacterDisp.
		 */
        CHARACTER = 3,
        /** Monochrome graphics displays that have binary (on/off) pixels.
		 *  Corresponding child class: GraphicsDisp1.
		 */
        GRAPHICS_1BIT = 4,
        /** Monochrome graphics displays that have 4bit (16 level) pixels.
		 *  Corresponding child class: GraphicsDisp4.
		 */
        GRAPHICS_4BIT = 5,
        /** Monochrome graphics displays that have 8bit (256 level) pixels.
		 *  Corresponding child class: GraphicsDisp8.
		 */
        GRAPHICS_8Bit = 7
    };

  protected:
    /** Creates a representation of an abstract display.
	 *
	 *  \param type  	The type of display. If you set type to any value other than Type::OTHER, 
	 * 					the object must be of the corresponding type.
	 *  \param ID  		A unique identification number used to tell apart Displays.
	 *  \param update_rate_in_systicks specifies the desired update rate of the display in systicks
	 */
    Display(Type type, uint8_t ID, uint32_t update_rate_in_systicks)
    : type_(type), update_rate_(update_rate_in_systicks), ID_(ID)
    {
    }

  public:
    virtual ~Display(){};

    /** Clears the display. */
    virtual void clear() = 0;

    /** Returns the type of display associated with this Display object.
	 *  When drawing to the Display, you can request the type via this function, then cast
	 *  the `Display` object to the corresponding type and perform your draw operations.
	 */
    Type getDisplayType() { return type_; }

    /** Returns the requested update rate of this display, in systicks. */
    uint32_t getUpdateRateInSysticks() { return update_rate_; }

    /** Returns the unique ID number of this display. This can be used to tell displays apart
	 *  if there are multiple displays of the same type.
	 */
    uint8_t getID() { return ID_; }

    /** Swaps the display buffers and starts transmitting the display data to the hardware. */
    virtual void swapBuffersAndTransmit() = 0;

  private:
    const Type     type_;
    const uint32_t update_rate_;
    const uint8_t  ID_;
};
} // namespace daisy