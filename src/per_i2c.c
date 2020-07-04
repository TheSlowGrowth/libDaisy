#include "per_i2c.h"
#include "util_hal_map.h"

// ======================================================================
// global variables
// ======================================================================

// TODO: This is global, and the board gets set for each init.
// Its a bit redundant, but I'm just trying to validate some hardware
//   without breaking all the other boards.
static dsy_i2c_handle i2c_handles[4];
// I2C4 needs BDMA. TODO: Add this.
static DMA_HandleTypeDef i2c_dma_tx_handles[3];

// ======================================================================
// Error handler
// ======================================================================

static void Error_Handler()
{
    asm("bkpt 255");
    while(1) {}
}

// ======================================================================
// private types
// ======================================================================

typedef enum
{
    dsy_i2c_transfer_success,
    dsy_i2c_transfer_failure
} dsy_i2c_transfer_result;

// ======================================================================
// dma job scheduling (all i2c peripherals share a single DMA stream)
// ======================================================================

/** Scheduling for DMA the transfers, as all I2C peripherals share the same DMA. 
 */
typedef struct
{
    uint16_t                       slave_address;
    uint8_t*                       data;
    uint16_t                       size;
    dsy_i2c_transf_cplt_callback_t callback;
    void*                          callback_context;
} DmaTransferJob;

// I2C4 has no DMA assigned yet.
// TODO: change this to 4 once that's done.
static const uint8_t num_i2c_with_dma = 3;

/** < 0 if DMA is not yet active
  * 0..3 if DMA is currently busy with the corresponding i2c peripheral
  */
static volatile int8_t dma_active_peripheral;
/** dma transfers that are waiting to be executed; one for each i2c peripheral. */
static volatile DmaTransferJob queued_dma_transfers[4];
/** callback for the current transfer */
static dsy_i2c_transf_cplt_callback_t current_callback;
static void*                          current_callback_context;

/** Returns true, if the i2c dma is currently executing a job. */
static int is_dma_active()
{
    return dma_active_peripheral >= 0;
}

/** Returns true, if a transfer job is queued for the provided i2c peripheral. */
static int is_dma_transfer_queued(size_t i2c_peripheral_idx)
{
    return queued_dma_transfers[i2c_peripheral_idx].data != NULL;
}

/** Queues a DmaTransferJob to be executed later. 
 * TODO: If the same peripheral is used from multiple threads
 * of execution (main loop or ISRs) we should add some sort of syncronisation
 * to this function.
 */
static void queue_dma_transfer(size_t                i2c_peripheral_idx,
                               const DmaTransferJob* job)
{
    // wait for any previous job on this peripheral to finish
    // and the queue position to bevome free
    while(is_dma_transfer_queued(i2c_peripheral_idx)) {};

    // queue the job
    queued_dma_transfers[i2c_peripheral_idx] = *job;
}

/** starts a dma transfer */
static dsy_i2c_result
start_dma_transfer(dsy_i2c_handle*                dsy_hi2c,
                   uint16_t                       address,
                   uint8_t*                       data,
                   uint16_t                       size,
                   dsy_i2c_transf_cplt_callback_t callback,
                   void*                          callback_context)
{
    // this could be called from both the scheduler ISR
    // and from user code via dsy_i2c_transmit_dma()
    // TODO: Add some sort of locking mechanism.

    const int          i2cIdx   = dsy_hi2c->config.periph;
    I2C_HandleTypeDef* hal_hi2c = (I2C_HandleTypeDef*)dsy_hi2c->hal_hi2c;
    // wait for previous transfer to be finished
    while(HAL_I2C_GetState(hal_hi2c) != HAL_I2C_STATE_READY) {};

    // reinit the DMA
    i2c_dma_tx_handles[i2cIdx].Instance = DMA1_Stream6;
    switch(i2cIdx)
    {
        case DSY_I2C_PERIPH_1:
            i2c_dma_tx_handles[i2cIdx].Init.Request = DMA_REQUEST_I2C1_TX;
            break;
        case DSY_I2C_PERIPH_2:
            i2c_dma_tx_handles[i2cIdx].Init.Request = DMA_REQUEST_I2C2_TX;
            break;
        case DSY_I2C_PERIPH_3:
            i2c_dma_tx_handles[i2cIdx].Init.Request = DMA_REQUEST_I2C3_TX;
            break;
        // I2C4 has no DMA yet. TODO
        default: return DSY_I2C_RES_ERR;
    }
    i2c_dma_tx_handles[i2cIdx].Init.Direction           = DMA_MEMORY_TO_PERIPH;
    i2c_dma_tx_handles[i2cIdx].Init.PeriphInc           = DMA_PINC_DISABLE;
    i2c_dma_tx_handles[i2cIdx].Init.MemInc              = DMA_MINC_ENABLE;
    i2c_dma_tx_handles[i2cIdx].Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    i2c_dma_tx_handles[i2cIdx].Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    i2c_dma_tx_handles[i2cIdx].Init.Mode                = DMA_NORMAL;
    i2c_dma_tx_handles[i2cIdx].Init.Priority            = DMA_PRIORITY_LOW;
    i2c_dma_tx_handles[i2cIdx].Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    i2c_dma_tx_handles[i2cIdx].Init.MemBurst            = DMA_MBURST_SINGLE;
    i2c_dma_tx_handles[i2cIdx].Init.PeriphBurst         = DMA_PBURST_SINGLE;

    if(HAL_DMA_Init(&i2c_dma_tx_handles[i2cIdx]) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_LINKDMA(hal_hi2c, hdmatx, i2c_dma_tx_handles[i2cIdx]);


    // start the transfer
    dma_active_peripheral    = i2cIdx;
    current_callback         = callback;
    current_callback_context = callback_context;
    if(HAL_I2C_Master_Transmit_DMA(hal_hi2c, address << 1, data, size)
       != HAL_OK)
    {
        dma_active_peripheral    = -1;
        current_callback         = NULL;
        current_callback_context = NULL;
        return DSY_I2C_RES_ERR;
    }
    return DSY_I2C_RES_OK;
}

/** dma scheduler */
static void dma_transfer_finished(I2C_HandleTypeDef*      i2c_handle,
                                  dsy_i2c_transfer_result result)
{
    dma_active_peripheral = -1;

    // on an error, reinit the peripheral to clear any flags
    if(result == dsy_i2c_transfer_failure)
        HAL_I2C_Init(i2c_handle);

    if(current_callback != NULL)
    {
        // the callback may setup another transmission, hence we shouldn't reset this to NULL
        // after the callback - it might overwrite the new transmission.
        dsy_i2c_transf_cplt_callback_t callback = current_callback;
        current_callback                        = NULL;
        // make the callback
        callback(current_callback_context,
                 (result == dsy_i2c_transfer_success) ? DSY_I2C_RES_OK
                                                      : DSY_I2C_RES_ERR);
    }

    // the callback could have started a new transmission right away...
    if(is_dma_active())
        return;

    // dma is still idle. Check if another i2c peripheral waits for a job.
    for(int per = 0; per < num_i2c_with_dma; per++)
        if(is_dma_transfer_queued(per))
        {
            if(start_dma_transfer(&i2c_handles[per],
                                  queued_dma_transfers[per].slave_address,
                                  queued_dma_transfers[per].data,
                                  queued_dma_transfers[per].size,
                                  queued_dma_transfers[per].callback,
                                  queued_dma_transfers[per].callback_context)
               == DSY_I2C_RES_OK)
            {
                // remove the job from the queue
                queued_dma_transfers[per].data = NULL;
                return;
            }
        }
}

// ======================================================================
// i2c driver functions
// ======================================================================

void dsy_i2c_global_init()
{
    dma_active_peripheral = -1;
    // init the scheduler queue
    for(int per = 0; per < num_i2c_with_dma; per++)
        queued_dma_transfers[per].data = NULL;
}

dsy_i2c_result dsy_i2c_init(dsy_i2c_handle* dsy_hi2c)
{
    I2C_HandleTypeDef* hal_hi2c = dsy_hal_map_get_i2c(&dsy_hi2c->config);
    dsy_hi2c->hal_hi2c          = (void*)hal_hi2c;
    switch(dsy_hi2c->config.periph)
    {
        default:
        case DSY_I2C_PERIPH_1:
            i2c_handles[0]     = *dsy_hi2c;
            hal_hi2c->Instance = I2C1;
            break;
        case DSY_I2C_PERIPH_2:
            i2c_handles[1]     = *dsy_hi2c;
            hal_hi2c->Instance = I2C2;
            break;
        case DSY_I2C_PERIPH_3:
            i2c_handles[2]     = *dsy_hi2c;
            hal_hi2c->Instance = I2C3;
            break;
        case DSY_I2C_PERIPH_4:
            i2c_handles[3]     = *dsy_hi2c;
            hal_hi2c->Instance = I2C4;
            break;
    }
    // Set Generic Parameters
    // Configure Speed
    // TODO: make this dependent on the current I2C Clock speed set in sys
    switch(dsy_hi2c->config.speed)
    {
        case DSY_I2C_SPEED_100KHZ: hal_hi2c->Init.Timing = 0x30E0628A; break;
        case DSY_I2C_SPEED_400KHZ: hal_hi2c->Init.Timing = 0x20D01132; break;
        case DSY_I2C_SPEED_1MHZ: hal_hi2c->Init.Timing = 0x1080091A; break;
        default: break;
    }
    //	hal_hi2c->Init.Timing = 0x00C0EAFF;
    hal_hi2c->Init.OwnAddress1      = 0;
    hal_hi2c->Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hal_hi2c->Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hal_hi2c->Init.OwnAddress2      = 0;
    hal_hi2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hal_hi2c->Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hal_hi2c->Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if(HAL_I2C_Init(hal_hi2c) != HAL_OK)
        return DSY_I2C_RES_ERR;
    if(HAL_I2CEx_ConfigAnalogFilter(hal_hi2c, I2C_ANALOGFILTER_ENABLE)
       != HAL_OK)
        return DSY_I2C_RES_ERR;
    if(HAL_I2CEx_ConfigDigitalFilter(hal_hi2c, 0) != HAL_OK)
        return DSY_I2C_RES_ERR;

    return DSY_I2C_RES_OK;
}

static void init_i2c_pins(dsy_i2c_handle* hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_TypeDef*    port;

    GPIO_InitStruct.Mode  = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    switch(hi2c->config.periph)
    {
        case DSY_I2C_PERIPH_1: GPIO_InitStruct.Alternate = GPIO_AF4_I2C1; break;
        case DSY_I2C_PERIPH_2: GPIO_InitStruct.Alternate = GPIO_AF4_I2C2; break;
        case DSY_I2C_PERIPH_3: GPIO_InitStruct.Alternate = GPIO_AF4_I2C3; break;
        case DSY_I2C_PERIPH_4: GPIO_InitStruct.Alternate = GPIO_AF4_I2C4; break;
        default: break;
    }

    port                = dsy_hal_map_get_port(&hi2c->config.pin_config.scl);
    GPIO_InitStruct.Pin = dsy_hal_map_get_pin(&hi2c->config.pin_config.scl);
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    port                = dsy_hal_map_get_port(&hi2c->config.pin_config.sda);
    GPIO_InitStruct.Pin = dsy_hal_map_get_pin(&hi2c->config.pin_config.sda);
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

static void deinit_i2c_pins(dsy_i2c_handle* hi2c)
{
    GPIO_TypeDef* port;
    uint16_t      pin;
    port = dsy_hal_map_get_port(&hi2c->config.pin_config.scl);
    pin  = dsy_hal_map_get_pin(&hi2c->config.pin_config.scl);
    HAL_GPIO_DeInit(port, pin);
    port = dsy_hal_map_get_port(&hi2c->config.pin_config.sda);
    pin  = dsy_hal_map_get_pin(&hi2c->config.pin_config.sda);
    HAL_GPIO_DeInit(port, pin);
}

dsy_i2c_result dsy_i2c_transmit_blocking(dsy_i2c_handle* dsy_hi2c,
                                         uint16_t        address,
                                         uint8_t*        pData,
                                         uint16_t        size,
                                         uint32_t        timeout)
{
    I2C_HandleTypeDef* hal_hi2c = (I2C_HandleTypeDef*)dsy_hi2c->hal_hi2c;

    // wait for previous transfer to be finished
    while(HAL_I2C_GetState(hal_hi2c) != HAL_I2C_STATE_READY) {};

    if(HAL_I2C_Master_Transmit(hal_hi2c, address << 1, pData, size, timeout)
       != HAL_OK)
        return DSY_I2C_RES_ERR;
    return DSY_I2C_RES_OK;
}

dsy_i2c_result dsy_i2c_transmit_dma(dsy_i2c_handle*                dsy_hi2c,
                                    uint16_t                       address,
                                    uint8_t*                       data,
                                    uint16_t                       size,
                                    dsy_i2c_transf_cplt_callback_t callback,
                                    void* callback_context)
{
    // I2C4 has no DMA yet.
    if(dsy_hi2c->config.periph == DSY_I2C_PERIPH_4)
        return DSY_I2C_RES_ERR;

    // if dma is currently running - queue a job
    if(is_dma_active())
    {
        DmaTransferJob job;
        job.slave_address    = address;
        job.data             = data;
        job.size             = size;
        job.callback         = callback;
        job.callback_context = callback_context;
        // queue a job (blocks until the queue position is free)
        queue_dma_transfer(dsy_hi2c->config.periph, &job);
        // TODO: the user can't tell if he got returned "OK"
        // because the transfer was executed or because it was queued...
        // should we change that?
        return DSY_I2C_RES_OK;
    }
    else
        // start transmission right away
        return start_dma_transfer(
            dsy_hi2c, address, data, size, callback, callback_context);
}

dsy_i2c_result dsy_i2c_ready(dsy_i2c_handle* dsy_hi2c)
{
    I2C_HandleTypeDef* hal_hi2c = (I2C_HandleTypeDef*)dsy_hi2c->hal_hi2c;
    if(HAL_I2C_GetState(hal_hi2c) != HAL_I2C_STATE_READY)
        return DSY_I2C_RES_ERR;
    return DSY_I2C_RES_OK;
}

// ======================================================================
// HAL service functions
// ======================================================================

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2c_handle)
{
    if(i2c_handle->Instance == I2C1)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        init_i2c_pins(&i2c_handles[0]);
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    }
    else if(i2c_handle->Instance == I2C2)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        init_i2c_pins(&i2c_handles[1]);
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
    }
    else if(i2c_handle->Instance == I2C3)
    {
        // Enable RCC GPIO CLK for necessary ports.
        init_i2c_pins(&i2c_handles[2]);
        __HAL_RCC_I2C3_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        HAL_NVIC_SetPriority(I2C3_EV_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(I2C3_EV_IRQn);
    }
    else if(i2c_handle->Instance == I2C4)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        init_i2c_pins(&i2c_handles[3]);
        __HAL_RCC_I2C4_CLK_ENABLE();

        // I2C4 needs BMDA
        // TODO
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2c_handle)
{
    if(i2c_handle->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_DISABLE();
        deinit_i2c_pins(&i2c_handles[0]);
    }
    else if(i2c_handle->Instance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_DISABLE();
        deinit_i2c_pins(&i2c_handles[1]);
    }
    else if(i2c_handle->Instance == I2C3)
    {
        // Enable RCC GPIO CLK for necessary ports.
        __HAL_RCC_I2C3_CLK_DISABLE();
        deinit_i2c_pins(&i2c_handles[2]);
    }
    else if(i2c_handle->Instance == I2C4)
    {
        __HAL_RCC_I2C4_CLK_DISABLE();
        deinit_i2c_pins(&i2c_handles[3]);
    }
}

// ======================================================================
// ISRs and event handlers
// ======================================================================

void DMA1_Stream6_IRQHandler(void)
{
    // TODO: multiplex this to handle all I2C peripherals
    if(dma_active_peripheral >= 0)
        HAL_DMA_IRQHandler(&i2c_dma_tx_handles[dma_active_peripheral]);
}

void I2C1_EV_IRQHandler()
{
    HAL_I2C_EV_IRQHandler(i2c_handles[0].hal_hi2c);
}

void I2C2_EV_IRQHandler()
{
    HAL_I2C_EV_IRQHandler(i2c_handles[1].hal_hi2c);
}

void I2C3_EV_IRQHandler()
{
    HAL_I2C_EV_IRQHandler(i2c_handles[2].hal_hi2c);
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef* i2c_handle)
{
    dma_transfer_finished(i2c_handle, dsy_i2c_transfer_success);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef* i2c_handle)
{
    dma_transfer_finished(i2c_handle, dsy_i2c_transfer_failure);
}
