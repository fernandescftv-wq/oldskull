// esp_now_tx.c - envio de comandos al modulo RX (OldSkull) VIA C6 onboard
//
// El P4 NO soporta ESP-NOW; el radio WiFi lo maneja el ESP32-C6 (esclavo
// esp_hosted por SDIO). El canal peer_data del esp_hosted (Custom RPC)
// transporta el packet del protocolo RX hasta el C6, que reenvía el MISMO
// packet por ESP-NOW al RX (MAC 80:F3:DA:63:51:90). El RX queda intacto.
#include "eh_host_feat_peer_data.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "espnow_tx";

/* msg_id acordado com o bridge do C6 (main/main.c do projeto c6) */
#define MSG_ID_OLDSKULL 0x4F53 /* 'OS' */
#define MSG_ID_ACK      0x4B43 /* 'KC': C6 -> P4 status do ESP-NOW (1 byte) */

// Packet protocolo RX (8 bytes little-endian): {cmd,bank,fxMask,ch,seq}
static uint32_t seq = 0;
static volatile int ack_status = -1;   /* -1=sem resposta, 0=OK, 1=FALHOU */

/* callback do C6: status do ESP-NOW (0=OK, 1=FALHOU) */
static void on_ack(uint32_t msg_id, const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    if (msg_id == MSG_ID_ACK) {
        ack_status = (len == 1) ? data[0] : -2;
        if (ack_status == 0)      ESP_LOGI(TAG, "*** RX ACK: OK — RX RECEBEU ***");
        else if (ack_status == 1) ESP_LOGI(TAG, "*** RX ACK: FALHOU — RX nao respondeu ***");
        else if (ack_status == 2) ESP_LOGI(TAG, "*** ECO: esp_now aceito (aguardando RX) ***");
        else if (ack_status == 3) ESP_LOGE(TAG, "*** ECO: esp_now_send ERRO (peer/canal?) ***");
        else                      ESP_LOGW(TAG, "ACK estranho: id=%lu len=%d", (unsigned long)msg_id, (int)len);
    }
}

esp_err_t esp_now_tx_init(void)
{
    esp_err_t r = eh_host_feat_peer_data_init();
    if (r == ESP_OK) r = eh_host_peer_data_register(MSG_ID_ACK, on_ack, NULL);
    ESP_LOGI(TAG, "TX->RX via C6 (peer_data msg=0x%04X, ack=0x%04X) init=%d", MSG_ID_OLDSKULL, MSG_ID_ACK, r);
    return r;
}

/* envia un APPLY BANK al RX (P4 -> C6 por peer_data; el C6 emite ESP-NOW) */
esp_err_t esp_now_tx_aplicar(int banco, uint8_t fxMask, uint8_t ch)
{
    if (banco < 1 || banco > 5) banco = 1;

    uint8_t pk[8];
    pk[0] = (uint8_t)banco;        /* cmd = APPLY BANK n */
    pk[1] = (uint8_t)banco;
    pk[2] = fxMask;
    pk[3] = ch ? ch : 1;
    uint32_t s = ++seq;            /* little-endian */
    pk[4] = (uint8_t)(s & 0xFF);
    pk[5] = (uint8_t)((s >> 8) & 0xFF);
    pk[6] = (uint8_t)((s >> 16) & 0xFF);
    pk[7] = (uint8_t)((s >> 24) & 0xFF);

    esp_err_t ret = eh_host_peer_data_send(MSG_ID_OLDSKULL, pk, sizeof(pk));
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "send APPLY b%d mask=0x%02X: fallo (%s)", banco, fxMask, esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "-> C6->RX b%d mask=0x%02X ch%d seq%u via peer_data", banco, fxMask, pk[3], (unsigned)s);
    return ret;
}