#pragma once
#ifndef DSY_SPI_H
#define DSY_SPI_H

#include "daisy_core.h"

namespace daisy
{
/** @addtogroup serial
@{
*/

/** Handler for serial peripheral interface */
class SpiHandle
{
  public:
    /** Contains settings for initialising an SPI interface. */
    struct Config
    {
        /** SPI peripheral enum */
        enum class Peripheral
        {
            SPI_1, /**< SPI peripheral 1 */
            SPI_3, /**< SPI peripheral 3 */
            SPI_6, /**< SPI peripheral 6 */
        };

        /** The clock speed. */
        enum class Speed 
        {

        };

        /** The clock signal polarity when idle */
        enum class ClockPolarity
        {
            HIGH_WHEN_IDLE, /**< & */
            LOW_WHEN_IDLE   /**< & */
        };

        /** The clock phase */
        enum class ClockPhase
        {
            FIRST_EDGE, /**< & */
            SECOND_EDGE /**< & */
        };

        Peripheral periph; /**< & */
        struct
        {
            dsy_gpio_pin mosi = {DSY_GPIOX, 0}; /**< & */
            dsy_gpio_pin miso = {DSY_GPIOX, 0}; /**< & */
            dsy_gpio_pin sck  = {DSY_GPIOX, 0}; /**< & */
        } pin_config;                           /**< & */
        Speed         speed;
        ClockPolarity clock_polarity;           /**< & */
        ClockPhase    clock_phase;              /**< & */
    };

    /** Return values for SPI functions. */
    enum class Result
    {
        OK, /**< & */
        ERR /**< & */
    };

    SpiHandle() : pimpl_(nullptr) {}
    SpiHandle(const SpiHandle& other) = default;
    SpiHandle& operator=(const SpiHandle& other) = default;

    /** Initializes an I2C peripheral. */
    Result init(const Config& config);

    /** Returns the current config. */
    const Config& GetConfig() const;

    /** Transmits and receives data and blocks until the transmission is complete.
     *  Use this for smaller transmissions of a few bytes.
     * 
     *  \param ss_pin       The SlaveSelect pin to use for this transmission.
     *  \param tx_data      A pointer to transmit buffer or nullptr if no data should be sent.
     *  \param rx_data      A pointer to receive buffer or nullptr if no data should be received.
     *  \param size         The size of the data to be sent, in bytes.
     *  \param timeout      A timeout.
     */
    Result transferBlocking(dsy_gpio_pin ss_pin,
                            uint8_t*     tx_data,
                            uint8_t*     rx_data,
                            uint16_t     size,
                            uint32_t     timeout);

    /** A callback to be executed when a dma transfer is complete. */
    typedef void (*CallbackFunctionPtr)(void* context, Result result);

    /** Transfers data with a DMA and returns immediately. Use this for larger transmissions.
     *  The pointer to the send and receive buffers must be located in the D2 memory domain 
     *  by adding the `DMA_BUFFER_MEM_SECTION` attribute like this:
     *      uint8_t DMA_BUFFER_MEM_SECTION my_buffer[100];
     *  If the send buffer cannot be placed in D2 memory for some reason, you MUST clear the 
     *  cachelines spanning the size of the transmit buffer before initiating the dma 
     *  transfer by calling `dsy_dma_clear_cache_for_buffer(buffer, size);`
     *  Similarly, if the receive buffer can't be placed in D2 memory for some reason,
     *  you must invalidate the cachelines spanning the size of the receive buffer after the
     *  dma transfer is complete by calling `dsy_dma_invalidate_cache_for_buffer(buffer, size);`
     *  
     *  A single DMA is shared across all SPI peripherals.
     *  If the DMA is busy with another transfer, the job will be queued and executed later.
     *  If there is a job waiting to be executed for this SPI peripheral, this function
     *  will block until the queue is free and the job can be queued.
     * 
     *  \param ss_pin       The SlaveSelect pin to use for this transmission.
     *  \param tx_data      A pointer to transmit buffer or nullptr if no data should be sent.
     *  \param rx_data      A pointer to receive buffer or nullptr if no data should be received.
     *  \param size         The size of the data to be sent, in bytes.
     *  \param callback     A callback to execute when the transfer finishes, or NULL.
     *  \param callback_context A pointer that will be passed back to you in the callback.      
     */
    Result transferDma(dsy_gpio_pin        ss_pin,
                       uint8_t*            tx_data,
                       uint8_t*            rx_data,
                       uint16_t            size,
                       CallbackFunctionPtr callback,
                       void*               callback_context);

    class Impl;

  private:
    Impl* pimpl_;
};


extern "C"
{
    /** internal. Used for global init. */
    void dsy_spi_global_init();
};

/** @} */
} // namespace daisy

#endif
