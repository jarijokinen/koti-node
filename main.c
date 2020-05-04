#include <app_timer.h>
#include <ble_advdata.h>
#include <bsp.h>
#include <nordic_common.h>
#include <nrf.h>
#include <nrf_drv_ppi.h>
#include <nrf_drv_saadc.h>
#include <nrf_drv_timer.h>
#include <nrf_log.h>
#include <nrf_log_ctrl.h>
#include <nrf_log_default_backends.h>
#include <nrf_pwr_mgmt.h>
#include <nrf_sdh.h>
#include <nrf_sdh_ble.h>
#include <nrf_soc.h>
#include <stdbool.h>
#include <stdint.h>


#define KOTI_BEACON_UUID 0x29, 0x39, 0x01
#define KOTI_COMPANY_IDENTIFIER 0x0059
#define KOTI_BLE_CONN_CFG_TAG 1
#define KOTI_ADV_INTERVAL MSEC_TO_UNITS(100, UNIT_0_625_MS)
#define KOTI_ADC_SAMPLES 5

#define DEAD_BEEF 0xdeadbeef

static const nrf_drv_timer_t m_timer = NRF_DRV_TIMER_INSTANCE(3);
static nrf_saadc_value_t m_buffer_pool[2][KOTI_ADC_SAMPLES];
static nrf_ppi_channel_t m_ppi_channel;
static uint32_t m_adc_evt_counter;
static uint16_t m_light_value;

static ble_gap_adv_params_t m_adv_params;
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

static ble_gap_adv_data_t m_adv_data = {
  .adv_data = {
    .p_data = m_enc_advdata,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
  },
  .scan_rsp_data = {
    .p_data = NULL,
    .len = 0
  }
};

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
  app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void koti_adv_start(void)
{
  ret_code_t err_code;

  err_code = sd_ble_gap_adv_start(m_adv_handle, KOTI_BLE_CONN_CFG_TAG);
  APP_ERROR_CHECK(err_code);

  err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
  APP_ERROR_CHECK(err_code);
}

static void koti_adv_init(void)
{
  ret_code_t err_code;
  
  ble_advdata_manuf_data_t manuf_data;
  ble_advdata_t advdata;

  uint8_t payload[] = {
    KOTI_BEACON_UUID,
    ((m_light_value >> 8) & 0xff),
    (m_light_value & 0xff)
  };
  manuf_data.company_identifier = KOTI_COMPANY_IDENTIFIER;
  manuf_data.data.p_data = (uint8_t *)payload;
  manuf_data.data.size = sizeof(payload);

  memset(&advdata, 0, sizeof(advdata));
  advdata.name_type = BLE_ADVDATA_NO_NAME;
  advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  advdata.p_manuf_specific_data = &manuf_data;

  memset(&m_adv_params, 0, sizeof(m_adv_params));
  m_adv_params.properties.type = 
    BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
  m_adv_params.p_peer_addr = NULL;
  m_adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
  m_adv_params.interval = KOTI_ADV_INTERVAL;
  m_adv_params.duration = 0;

  err_code = ble_advdata_encode(&advdata, m_adv_data.adv_data.p_data, 
    &m_adv_data.adv_data.len);
  APP_ERROR_CHECK(err_code);
}

static void koti_ble_stack_init(void)
{
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(KOTI_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);
}

static void koti_timer_handler(nrf_timer_event_t event_type, void *p_context)
{
}

static void koti_saadc_sampling_event_init(void)
{
  ret_code_t err_code;

  err_code = nrf_drv_ppi_init();
  APP_ERROR_CHECK(err_code);

  nrf_drv_timer_config_t timer_config = NRF_DRV_TIMER_DEFAULT_CONFIG;
  timer_config.frequency = NRF_TIMER_FREQ_31250Hz;

  err_code = nrf_drv_timer_init(&m_timer, &timer_config, koti_timer_handler);
  APP_ERROR_CHECK(err_code);

  uint32_t ticks = nrf_drv_timer_ms_to_ticks(&m_timer, 400);
  nrf_drv_timer_extended_compare(&m_timer, NRF_TIMER_CC_CHANNEL0, ticks, 
    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);
  nrf_drv_timer_enable(&m_timer);

  uint32_t timer_compare_event_addr = nrf_drv_timer_compare_event_address_get(
    &m_timer, NRF_TIMER_CC_CHANNEL0);
  uint32_t saadc_sample_task_addr = nrf_drv_saadc_sample_task_get();

  err_code = nrf_drv_ppi_channel_alloc(&m_ppi_channel);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_drv_ppi_channel_assign(m_ppi_channel, 
    timer_compare_event_addr, saadc_sample_task_addr);
  APP_ERROR_CHECK(err_code);
}

static void koti_saadc_sampling_event_enable(void)
{
  ret_code_t err_code = nrf_drv_ppi_channel_enable(m_ppi_channel);
  APP_ERROR_CHECK(err_code);
}

static void koti_saadc_callback(nrf_drv_saadc_evt_t const *p_event)
{
  if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
    ret_code_t err_code;

    err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 
      KOTI_ADC_SAMPLES);
    APP_ERROR_CHECK(err_code);

    for (int i = 0; i < KOTI_ADC_SAMPLES; i++) {
      m_light_value = p_event->data.done.p_buffer[i];
    }
    koti_adv_init();
    m_adc_evt_counter++;
  }
}

static void koti_saadc_init(void)
{
  ret_code_t err_code;

  nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
  saadc_config.resolution = NRF_SAADC_RESOLUTION_12BIT;

  nrf_saadc_channel_config_t channel_config = 
    NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
  channel_config.gain = NRF_SAADC_GAIN1_6;
  channel_config.reference = NRF_SAADC_REFERENCE_VDD4;

  err_code = nrf_drv_saadc_init(&saadc_config, koti_saadc_callback);
  APP_ERROR_CHECK(err_code);
  
  err_code = nrf_drv_saadc_channel_init(0, &channel_config);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[0], KOTI_ADC_SAMPLES);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_drv_saadc_buffer_convert(m_buffer_pool[1], KOTI_ADC_SAMPLES);
  APP_ERROR_CHECK(err_code);
}

int main(void)
{
  ret_code_t err_code;

  /** Initialize logging */
  err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();
    
  /** Initialize timers */
  err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);

  /** Initialize leds */
  err_code = bsp_init(BSP_INIT_LEDS, NULL);
  APP_ERROR_CHECK(err_code);

  /** Initialize power management */
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);

  /** Initialize BLE stack and advertising */
  koti_ble_stack_init();
  koti_adv_init();
  
  err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, 
    &m_adv_params);
  APP_ERROR_CHECK(err_code);
  
  /** Initialize SAADC */
  koti_saadc_sampling_event_init();
  koti_saadc_init();
  koti_saadc_sampling_event_enable();

  /** Start advertising */
  NRF_LOG_INFO("koti-node started");
  koti_adv_start();

  while (1) {
    err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
  }

  return 0;
}
