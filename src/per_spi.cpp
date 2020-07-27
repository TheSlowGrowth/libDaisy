#include "per_spi.h"
#include "util_hal_map.h"

// TODO
// - fix up rest of lib so that we can add a spi_handle map to the hal map
// - Add configuration for standard spi stuff.


namespace daisy
{
/** Private implementation for SpiHandle */
class SpiHandle::Impl
{
  public:
    SpiHandle::Result        init(const SpiHandle::Config& config);
    const SpiHandle::Config& GetConfig() const { return config_; }

    SpiHandle::Result transferBlocking(dsy_gpio_pin ss_pin,
                                       uint8_t*     tx_data,
                                       uint8_t*     rx_data,
                                       uint16_t     size,
                                       uint32_t     timeout);
    SpiHandle::Result transferDma(dsy_gpio_pin                   ss_pin,
                                  uint8_t*                       tx_data,
                                  uint8_t*                       rx_data,
                                  uint16_t                       size,
                                  SpiHandle::CallbackFunctionPtr callback,
                                  void* callback_context);

    // =========================================================
    // scheduling and global functions
    struct DmaJob
    {
        dsy_gpio_pin                   ss_pin           = {DSY_GPIOX, 0};
        uint8_t*                       tx_data          = nullptr;
        uint8_t*                       rx_data          = nullptr;
        uint16_t                       size             = 0;
        SpiHandle::CallbackFunctionPtr callback         = nullptr;
        void*                          callback_context = nullptr;

        bool isValidJob() const { return ss_pin.port != DSY_GPIOX; }
        void invalidate() { ss_pin = {DSY_GPIOX, 0}; }
    };
    static void globalInit();
    static bool isDmaActive();
    static bool IsDmaTransferQueuedFor(size_t spi_peripheral_idx);
    static void QueueDmaTransfer(size_t spi_peripheral_idx, const DmaJob& job);
    static void DmaTransferFinished(SPI_HandleTypeDef* hal_spi_handle,
                                    SpiHandle::Result  result);

    static constexpr uint8_t              kNumSpiWithDma = 3;
    static volatile int8_t                dma_active_peripheral_;
    static DmaJob                         queued_dma_transfers_[kNumSpiWithDma];
    static SpiHandle::CallbackFunctionPtr next_callback_;
    static void*                          next_callback_context_;

    // =========================================================
    // pivate functions and member variables
    SpiHandle::Config config_;
    DMA_HandleTypeDef spi_dma_tx_handle_;
    DMA_HandleTypeDef spi_dma_rx_handle_;
    SPI_HandleTypeDef spi_hal_handle_;

    SpiHandle::Result startDmaTransfer(dsy_gpio_pin                   ss_pin,
                                       uint8_t*                       tx_data,
                                       uint8_t*                       rx_data,
                                       uint16_t                       size,
                                       SpiHandle::CallbackFunctionPtr callback,
                                       void* callback_context);

    void initPins();
    void deinitPins();
};

// ======================================================================
// Error handler
// ======================================================================

static void Error_Handler()
{
    asm("bkpt 255");
    while(1) {}
}

// ================================================================
// Global references for the availabel SPIHandle::Impls
// ================================================================

static SpiHandle::Impl spi_handles[3];

// ================================================================
// Scheduling and global functions
// ================================================================

void SpiHandle::Impl::globalInit()
{
    // init the scheduler queue
    dma_active_peripheral_ = -1;
    for(int per = 0; per < kNumSpiWithDma; per++)
        queued_dma_transfers_[per] = SpiHandle::Impl::DmaJob();
}

bool SpiHandle::Impl::isDmaActive()
{
    return dma_active_peripheral_ >= 0;
}

bool SpiHandle::Impl::IsDmaTransferQueuedFor(size_t spi_peripheral_idx)
{
    return queued_dma_transfers_[spi_peripheral_idx].isValidJob();
}

void SpiHandle::Impl::QueueDmaTransfer(size_t spi_peripheral_idx,
                                       const SpiHandle::Impl::DmaJob& job)
{
    // wait for any previous job on this peripheral to finish
    // and the queue position to bevome free
    while(IsDmaTransferQueuedFor(spi_peripheral_idx)) {};

    // queue the job
    // TODO: Add ScopedIrqBlocker here
    queued_dma_transfers_[spi_peripheral_idx] = job;
}

void SpiHandle::Impl::DmaTransferFinished(SPI_HandleTypeDef* hal_spi_handle,
                                          SpiHandle::Result  result)
{
    // TODO: Add ScopedIrqBlocker

    // on an error, reinit the peripheral to clear any flags
    if(result != SpiHandle::Result::OK)
        HAL_SPI_Init(hal_spi_handle);

    dma_active_peripheral_ = -1;

    if(next_callback_ != nullptr)
    {
        // the callback may setup another transmission, hence we shouldn't reset this to
        // nullptr after the callback - it might overwrite the new transmission.
        auto callback  = next_callback_;
        next_callback_ = nullptr;
        // make the callback
        callback(next_callback_context_, result);
    }

    // the callback could have started a new transmission right away...
    if(isDmaActive())
        return;

    // dma is still idle. Check if another i2c peripheral waits for a job.
    for(int per = 0; per < kNumSpiWithDma; per++)
        if(IsDmaTransferQueuedFor(per))
        {
            if(spi_handles[per].startDmaTransfer(
                   queued_dma_transfers_[per].ss_pin,
                   queued_dma_transfers_[per].tx_data,
                   queued_dma_transfers_[per].rx_data,
                   queued_dma_transfers_[per].size,
                   queued_dma_transfers_[per].callback,
                   queued_dma_transfers_[per].callback_context)
               == SpiHandle::Result::OK)
            {
                // remove the job from the queue
                queued_dma_transfers_[per].invalidate();
                return;
            }
        }
}

// ================================================================
// Spi functions
// ================================================================

SpiHandle::Result SpiHandle::Impl::init(const SpiHandle::Config& config)
{
    const int spiIdx = int(config.periph);

    if(spiIdx >= 4)
        return SpiHandle::Result::ERR;

    config_ = config;
    constexpr SPI_TypeDef* instances[3]
        = {SPI1, SPI3, SPI6}; // map HAL instances
    spi_hal_handle_.Instance = instances[spiIdx];

    // Set Generic Parameters
    spi_hal_handle_.Init.Mode              = SPI_MODE_MASTER;
    spi_hal_handle_.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
    // direction
    if((config.pin_config.miso.port != DSY_GPIOX)
       && (config.pin_config.mosi.port != DSY_GPIOX))
        spi_hal_handle_.Init.Direction = SPI_DIRECTION_2LINES;
    else if(config.pin_config.miso.port != DSY_GPIOX)
        spi_hal_handle_.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
    else if(config.pin_config.mosi.port != DSY_GPIOX)
        spi_hal_handle_.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
    // clock phase
    if(config.clock_phase == SpiHandle::Config::ClockPhase::FIRST_EDGE)
        spi_hal_handle_.Init.CLKPhase = SPI_PHASE_1EDGE;
    else
        spi_hal_handle_.Init.CLKPhase = SPI_PHASE_2EDGE;
    // clock polarity
    if(config.clock_polarity == SpiHandle::Config::ClockPolarity::LOW_WHEN_IDLE)
        spi_hal_handle_.Init.CLKPolarity = SPI_POLARITY_LOW;
    else
        spi_hal_handle_.Init.CLKPolarity = SPI_POLARITY_HIGH;
    spi_hal_handle_.Init.DataSize       = SPI_DATASIZE_8BIT;
    spi_hal_handle_.Init.FirstBit       = SPI_FIRSTBIT_MSB;
    spi_hal_handle_.Init.TIMode         = SPI_TIMODE_DISABLE;
    spi_hal_handle_.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    spi_hal_handle_.Init.CRCPolynomial  = 7;
    spi_hal_handle_.Init.CRCLength      = SPI_CRC_LENGTH_8BIT;
    spi_hal_handle_.Init.NSS            = SPI_NSS_SOFT;
    spi_hal_handle_.Init.NSSPMode       = SPI_NSS_PULSE_DISABLE;
    spi_hal_handle_.Init.MasterKeepIOState
        = SPI_MASTER_KEEP_IO_STATE_ENABLE; /* Recommanded setting to avoid glitches */

    if(HAL_SPI_Init(&spi_hal_handle_) != HAL_OK)
        return SpiHandle::Result::ERR;

    return SpiHandle::Result::OK;
}

I2CHandle::Result I2CHandle::Impl::TransmitBlocking(uint16_t address,
                                                    uint8_t* data,
                                                    uint16_t size,
                                                    uint32_t timeout)
{
    // wait for previous transfer to be finished
    while(HAL_I2C_GetState(&i2c_hal_handle_) != HAL_I2C_STATE_READY) {};

    if(HAL_I2C_Master_Transmit(
           &i2c_hal_handle_, address << 1, data, size, timeout)
       != HAL_OK)
        return I2CHandle::Result::ERR;
    return I2CHandle::Result::OK;
}

I2CHandle::Result
I2CHandle::Impl::TransmitDma(uint16_t                       address,
                             uint8_t*                       data,
                             uint16_t                       size,
                             I2CHandle::CallbackFunctionPtr callback,
                             void*                          callback_context)
{
    // I2C4 has no DMA yet.
    if(config_.periph == I2CHandle::Config::Peripheral::I2C_4)
        return I2CHandle::Result::ERR;

    const int i2cIdx = int(config_.periph);

    // if dma is currently running - queue a job
    if(isDmaActive())
    {
        DmaJob job;
        job.slave_address    = address;
        job.data             = data;
        job.size             = size;
        job.callback         = callback;
        job.callback_context = callback_context;
        // queue a job (blocks until the queue position is free)
        QueueDmaTransfer(i2cIdx, job);
        // TODO: the user can't tell if he got returned "OK"
        // because the transfer was executed or because it was queued...
        // should we change that?
        return I2CHandle::Result::OK;
    }
    else
        // start transmission right away
        return startDmaTransfer(
            address, data, size, callback, callback_context);
}

I2CHandle::Result
I2CHandle::Impl::startDmaTransfer(uint16_t                       address,
                                  uint8_t*                       data,
                                  uint16_t                       size,
                                  I2CHandle::CallbackFunctionPtr callback,
                                  void* callback_context)
{
    // this could be called from both the scheduler ISR
    // and from user code via dsy_i2c_transmit_dma()
    // TODO: Add some sort of locking mechanism.

    // wait for previous transfer to be finished
    while(HAL_I2C_GetState(&i2c_hal_handle_) != HAL_I2C_STATE_READY) {};

    // reinit the DMA
    i2c_dma_tx_handle_.Instance = DMA1_Stream6;
    switch(config_.periph)
    {
        case I2CHandle::Config::Peripheral::I2C_1:
            i2c_dma_tx_handle_.Init.Request = DMA_REQUEST_I2C1_TX;
            break;
        case I2CHandle::Config::Peripheral::I2C_2:
            i2c_dma_tx_handle_.Init.Request = DMA_REQUEST_I2C2_TX;
            break;
        case I2CHandle::Config::Peripheral::I2C_3:
            i2c_dma_tx_handle_.Init.Request = DMA_REQUEST_I2C3_TX;
            break;
        // I2C4 has no DMA yet. TODO
        default: return I2CHandle::Result::ERR;
    }
    i2c_dma_tx_handle_.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    i2c_dma_tx_handle_.Init.PeriphInc           = DMA_PINC_DISABLE;
    i2c_dma_tx_handle_.Init.MemInc              = DMA_MINC_ENABLE;
    i2c_dma_tx_handle_.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    i2c_dma_tx_handle_.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    i2c_dma_tx_handle_.Init.Mode                = DMA_NORMAL;
    i2c_dma_tx_handle_.Init.Priority            = DMA_PRIORITY_LOW;
    i2c_dma_tx_handle_.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    i2c_dma_tx_handle_.Init.MemBurst            = DMA_MBURST_SINGLE;
    i2c_dma_tx_handle_.Init.PeriphBurst         = DMA_PBURST_SINGLE;

    if(HAL_DMA_Init(&i2c_dma_tx_handle_) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_LINKDMA(&i2c_hal_handle_, hdmatx, i2c_dma_tx_handle_);

    // start the transfer
    // TODO: Add ScopedIrqBlocker
    dma_active_peripheral_ = int(config_.periph);
    next_callback_         = callback;
    next_callback_context_ = callback_context;
    if(HAL_I2C_Master_Transmit_DMA(&i2c_hal_handle_, address << 1, data, size)
       != HAL_OK)
    {
        dma_active_peripheral_ = -1;
        next_callback_         = NULL;
        next_callback_context_ = NULL;
        return I2CHandle::Result::ERR;
    }
    return I2CHandle::Result::OK;
}

void I2CHandle::Impl::initPins()
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_TypeDef*    port;

    GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    switch(config_.periph)
    {
        case I2CHandle::Config::Peripheral::I2C_1:
            GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
            break;
        case I2CHandle::Config::Peripheral::I2C_2:
            GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
            break;
        case I2CHandle::Config::Peripheral::I2C_3:
            GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
            break;
        case I2CHandle::Config::Peripheral::I2C_4:
            GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
            break;
        default: break;
    }

    port                = dsy_hal_map_get_port(&config_.pin_config.scl);
    GPIO_InitStruct.Pin = dsy_hal_map_get_pin(&config_.pin_config.scl);
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    port                = dsy_hal_map_get_port(&config_.pin_config.sda);
    GPIO_InitStruct.Pin = dsy_hal_map_get_pin(&config_.pin_config.sda);
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

void I2CHandle::Impl::deinitPins()
{
    GPIO_TypeDef* port;
    uint16_t      pin;
    port = dsy_hal_map_get_port(&config_.pin_config.scl);
    pin  = dsy_hal_map_get_pin(&config_.pin_config.scl);
    HAL_GPIO_DeInit(port, pin);
    port = dsy_hal_map_get_port(&config_.pin_config.sda);
    pin  = dsy_hal_map_get_pin(&config_.pin_config.sda);
    HAL_GPIO_DeInit(port, pin);
}

volatile int8_t         I2CHandle::Impl::dma_active_peripheral_;
I2CHandle::Impl::DmaJob I2CHandle::Impl::queued_dma_transfers_[kNumI2CWithDma];
I2CHandle::CallbackFunctionPtr I2CHandle::Impl::next_callback_;
void*                          I2CHandle::Impl::next_callback_context_;

} // namespace daisy

#if 0
static SPI_HandleTypeDef hspi1;

static void Error_Handler()
{
    asm("bkpt 255");
}

void SpiHandle::Init()
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES_TXONLY;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_HARD_OUTPUT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 0x0;
    hspi1.Init.NSSPMode          = SPI_NSS_PULSE_ENABLE;
    hspi1.Init.NSSPolarity       = SPI_NSS_POLARITY_LOW;
    hspi1.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
    hspi1.Init.TxCRCInitializationPattern
        = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.RxCRCInitializationPattern
        = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
    hspi1.Init.MasterSSIdleness        = SPI_MASTER_SS_IDLENESS_00CYCLE;
    hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    hspi1.Init.MasterReceiverAutoSusp  = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    hspi1.Init.MasterKeepIOState       = SPI_MASTER_KEEP_IO_STATE_DISABLE;
    hspi1.Init.IOSwap                  = SPI_IO_SWAP_DISABLE;
    if(HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}

void SpiHandle::BlockingTransmit(uint8_t* buff, size_t size)
{
    HAL_SPI_Transmit(&hspi1, buff, size, 100);
}

void HAL_SPI_MspInit(SPI_HandleTypeDef* spiHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(spiHandle->Instance == SPI1)
    {
        /* USER CODE BEGIN SPI1_MspInit 0 */

        /* USER CODE END SPI1_MspInit 0 */
        /* SPI1 clock enable */
        __HAL_RCC_SPI1_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        /**SPI1 GPIO Configuration    
    PB5     ------> SPI1_MOSI
    PB4 (NJTRST)     ------> SPI1_MISO
    PG11     ------> SPI1_SCK
    PG10     ------> SPI1_NSS 
    */
        //        GPIO_InitStruct.Pin       = GPIO_PIN_5 | GPIO_PIN_4;
        switch(spiHandle->Init.Direction)
        {
            case SPI_DIRECTION_2LINES_TXONLY:
                GPIO_InitStruct.Pin = GPIO_PIN_5;
                break;
            case SPI_DIRECTION_2LINES_RXONLY:
                GPIO_InitStruct.Pin = GPIO_PIN_4;
                break;
            default: GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5; break;
        }
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        // Sck and CS
        GPIO_InitStruct.Pin = GPIO_PIN_11;
        if(spiHandle->Init.NSS != SPI_NSS_SOFT)
        {
            GPIO_InitStruct.Pin |= GPIO_PIN_10;
        }
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        /* USER CODE BEGIN SPI1_MspInit 1 */

        /* USER CODE END SPI1_MspInit 1 */
    }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef* spiHandle)
{
    if(spiHandle->Instance == SPI1)
    {
        /* USER CODE BEGIN SPI1_MspDeInit 0 */

        /* USER CODE END SPI1_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_SPI1_CLK_DISABLE();

        /**SPI1 GPIO Configuration    
    PB5     ------> SPI1_MOSI
    PB4 (NJTRST)     ------> SPI1_MISO
    PG11     ------> SPI1_SCK
    PG10     ------> SPI1_NSS 
    */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_5 | GPIO_PIN_4);

        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_11 | GPIO_PIN_10);

        /* USER CODE BEGIN SPI1_MspDeInit 1 */

        /* USER CODE END SPI1_MspDeInit 1 */
    }
}
#endif
