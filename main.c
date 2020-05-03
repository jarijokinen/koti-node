#include <app_timer.h>
#include <ble_advdata.h>
#include <bsp.h>
#include <nordic_common.h>
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

#define DEAD_BEEF 0xdeadbeef

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

static void koti_adv_init(void) {
  ret_code_t err_code;
  
  ble_advdata_manuf_data_t manuf_data;
  ble_advdata_t advdata;

  /* TODO: replace hard-coded light value by a value from adc */
  uint16_t light = 0x1234; /* value between 0 - 4095 (0x0000 - 0x0fff) */
  uint8_t flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

  uint8_t payload[] = {
    KOTI_BEACON_UUID,
    ((light >> 8) & 0xff),
    (light & 0xff)
  };
  manuf_data.company_identifier = KOTI_COMPANY_IDENTIFIER;
  manuf_data.data.p_data = (uint8_t *)payload;
  manuf_data.data.size = sizeof(payload);

  memset(&advdata, 0, sizeof(advdata));
  advdata.name_type = BLE_ADVDATA_NO_NAME;
  advdata.flags = flags;
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

  err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, 
    &m_adv_params);
  APP_ERROR_CHECK(err_code);
}

static void koti_ble_stack_init(void) {
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(KOTI_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_sdh_ble_enable(&ram_start);
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

  /** Start advertising */
  NRF_LOG_INFO("koti-node started");
  koti_adv_start();

  while (1) {
    if (!NRF_LOG_PROCESS()) {
      nrf_pwr_mgmt_run();
    }
  }

  return 0;
}
