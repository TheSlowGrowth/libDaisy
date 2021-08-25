#pragma once

#include "daisy_core.h"
#include "per/gpio.h"
#include "per/spi.h"

namespace daisy
{
/** @addtogroup shiftregister
    @{
    */

/**
   @brief Device driver for 8-bit shift register chains consisting of multiple
   74HC595 8-bit serial-to-parallel and 74HC165 8-bit parallel-to-serial shift 
   registers connected to the same SPI bus.
   @author jelliesen
*/
template <uint8_t num_595, uint8_t num_165, typename SpiHandleType = SpiHandle>
class ShiftRegisterChain
{
  public:
    struct Config
    {
        SpiHandle::Config::Peripheral spi;
        struct PinConfig
        {
            dsy_gpio_pin sclk;
            dsy_gpio_pin miso;
            dsy_gpio_pin mosi;
            dsy_gpio_pin nss;
        } pin_config;
    };

    void Init(const Config& config)
    {
        // Init SPI
        SpiHandleType::Config spi_cfg;
        spi_cfg.pin_config = config.pin_config;
        spi_cfg.periph     = config.spi;
        spi_cfg.mode       = SpiHandleType::Config::Mode::MASTER;
        if constexpr(getDirection() == Direction::fullDuplex)
            spi_cfg.direction = SpiHandleType::Config::Direction::TWO_LINES;
        else if constexpr(getDirection() == Direction::rxOnly)
            spi_cfg.direction
                = SpiHandleType::Config::Direction::TWO_LINES_RX_ONLY;
        else
            spi_cfg.direction
                = SpiHandleType::Config::Direction::TWO_LINES_TX_ONLY;
        spi_cfg.datasize = 8;
        spi_cfg.clock_polarity
            = SpiHandleType::Config::ClockPolarity::LOW; // TODO: Check
        spi_cfg.clock_phase
            = SpiHandleType::Config::ClockPhase::TWO_EDGE; // TODO: Check
        spi_cfg.nss = SpiHandleType::Config::NSS::SOFT;
        spi_cfg.baud_prescaler
            = SpiHandleType::Config::BaudPrescaler::PS_64; // TODO: Check
        spi_handle_.Init(spi_cfg);

        // init NSS pin
        nss_gpio_.mode = DSY_GPIO_MODE_OUTPUT_PP;
        nss_gpio_.pin  = config.nss;
        nss_gpio_.pull = DSY_GPIO_NOPULL;
        dsy_gpio_init(&nss_gpio_);
        dsy_gpio_write(&nss_gpio_, 1);

        // Clear Buffers
        for(uint8_t i = 0; i < chain_length_; i++)
            tx_buffer_[i] = rx_buffer_[i] = 0x00;
    }

    static constexpr uint8_t  GetNum595() { return num_595; }
    static constexpr uint16_t GetNumOutputs() { return num_595 * 8; }
    static constexpr uint8_t  GetNum165() { return num_165; }
    static constexpr uint16_t GetNumInputs() { return num_165 * 8; }

    void SetOutput(uint16_t idx, bool value)
    {
        const auto bitIdx  = idx & 0x07;
        const auto byteIdx = idx >> 3;
        if(value)
            tx_buffer_[byteIdx] |= 1 << bitIdx;
        else
            tx_buffer_[byteIdx] &= ~(1 << bitIdx);
    };

    bool GetInput(uint16_t idx)
    {
        const auto bitIdx  = idx & 0x07;
        const auto byteIdx = idx >> 3;
        return rx_buffer_[byteIdx] & (1 << bitIdx);
    };

    void TransmitBlocking()
    {
        dsy_gpio_write(&nss_gpio_, 0);
        if constexpr(getDirection() == Direction::fullDuplex)
            spi_handle_.BlockingTransfer(
                tx_buffer_, rx_buffer_, chain_length_, 0);
        else if constexpr(getDirection() == Direction::rxOnly)
            spi_handle_.BlockingReceive(rx_buffer_, chain_length_, 0);
        else
            spi_handle_.BlockingTransmit(tx_buffer_, chain_length_, 0);
        dsy_gpio_write(&nss_gpio_, 1);
    }

  private:
    static constexpr uint8_t chain_length_ = std::max(num_595, num_165);

    enum class Direction
    {
        fullDuplex,
        rxOnly,
        txOnly
    };
    static constexpr getDirection()
    {
        if(num_595 > 0 && num_165 > 0)
            return Direction::fullDuplex;
        else if(num_165 > 0)
            return Direction::rxOnly;
        else
            return Direction::txOnly;
    }

    Config::PinConfig pin_config_;
    uint8_t tx_buffer_[getDirection() != Direction::rxOnly ? chain_length_ : 0];
    uint8_t rx_buffer_[getDirection() != Direction::txOnly ? chain_length_ : 0];
    SpiHandleType spi_handle_;
    dsy_gpio      nss_gpio_;
};

template <uint8_t num_595>
using ShiftRegisterChain595 = ShiftRegisterChain<num_595, 0, SpiHandle>;

template <uint8_t num_165>
using ShiftRegisterChain165 = ShiftRegisterChain<0, num_165, SpiHandle>;

} // namespace daisy