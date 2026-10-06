// web_server.c - HTTP server para la Tela 1 (palco) servida por el P4
// Sirve WEB_INDEX_HTML (auto-contenido) en http://192.168.4.1/
#include "esp_http_server.h"
#include "esp_log.h"
#include "web_html.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "web_server";

static esp_err_t index_get_handler(httpd_req_t *req)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "text/html; charset=utf-8");
    httpd_resp_set_type(req, buf);
    httpd_resp_send(req, WEB_INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t not_found_handler(httpd_req_t *req)
{
    httpd_resp_send_404(req);
    return ESP_OK;
}

/* Servidor de assets da versao web (arquivos sob URI /assets/ na SPIFFS) */
static esp_err_t asset_get_handler(httpd_req_t *req)
{
    char path[80];
    const char *uri = req->uri;
    size_t ulen = strlen(uri);
    if (ulen > sizeof(path) - 8) {   /* "/spiffs" + NUL = 8 */
        httpd_resp_send_404(req);
        return ESP_OK;
    }
    memcpy(path, "/spiffs", 7);
    memcpy(path + 7, uri, ulen + 1);   /* copia junto o NUL */

    httpd_resp_set_type(req, "application/octet-stream");
    if (strstr(req->uri, ".png"))  httpd_resp_set_type(req, "image/png");
    else if (strstr(req->uri, ".jpg") || strstr(req->uri, ".jpeg")) httpd_resp_set_type(req, "image/jpeg");
    else if (strstr(req->uri, ".mp4")) httpd_resp_set_type(req, "video/mp4");

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "asset nao encontrado: %s", path);
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    char chunk[4096];
    size_t n;
    esp_err_t ret = ESP_OK;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) { ret = ESP_FAIL; break; }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);
    return ret;
}

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Iniciando HTTP server...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 12288;   /* 12KB: handler de assets usa chunk 4KB na pilha (6KB estourava) */
    /* coringa no fim da URI: roteia /assets/NOME ao handler de assets */
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao iniciar HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &uri_get);

    /* mismo HTML también en /index.html y por si acaso /palco */
    httpd_uri_t uri_index = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &uri_index);

    httpd_uri_t uri_palco = {
        .uri = "/palco",
        .method = HTTP_GET,
        .handler = index_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &uri_palco);

    /* Assets da versao web (fundo/amps/pedais/video) a partir da SPIFFS */
    httpd_uri_t uri_assets = {
        .uri = "/assets/*",
        .method = HTTP_GET,
        .handler = asset_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &uri_assets);

    /* 404 para todo lo demás */
    httpd_uri_t uri_404 = {
        .uri = "*",
        .method = HTTP_GET,
        .handler = not_found_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(server, &uri_404);

    ESP_LOGI(TAG, "HTTP server OK en http://192.168.4.1/");
    return ESP_OK;
}