# OldSkull — Plano de Execução Web→LVGL (Setembro 2026)

## 🎯 Objetivo Principal
Usar prototipagem web primeiro para iterar layouts rápidos em CSS/HTML, depois transferir para LVGL C. Economia de tokens via skills modulares (~500 tokens base vs ~9K completo).

---

## 📊 Estado Atual (Setembro 2026)

### ✅ IMPLEMENTADO E FUNCIONANDO
- Display ST7701 landscape 800×480 com rotação software
- Touch GT911 funcional com swap_xy/mirror_x configurado
- Carrossel de 3 amplificadores (Lonestar/Soldano/JP-2C) com swipe vertical
- 3 telas com navegação horizontal
- Video loop 121 frames @ 24fps
- Comunicação ESP-NOW TX→RX com RX controlando relés físicos
- Bancos de presets básicos (BANK 1-5)
- BSP layer com hardware abstraction parcial

### ❌ PENDENTES/CONCEITUais
- MIDI/Hotone Ampero II (integração bidirecional)
- Looper com ADC/DAC e PSRAM (requer ESP32-S3)
- Pedal de expressão via MIDI CC
- Modo Song (sequências automáticas sincronizadas)
- Modo Cena/Scene completa
- Modos Automático e Estudo
- Perfis de multi-resolução
- CORE completamente separado do hardware

### 🔧 REGRAS CRÍTICAS
- Não reescrever arquivos inteiros — patches cirúrgicos apenas
- WiFi não modificar sem ordem direta do usuário
- Validação no hardware antes de declarar pronto
- Backups antes de modificações importantes
- Git commit antes/depois de cada alteração significativa

---

## 🚀 FASES DE EXECUÇÃO

### FASE 1: PROTOTIPAGEM TELAS WEB→LVGL ⬅️ INICIAR AGORA

**Objetivo:** Iterar designs visuais rapidamente em browser antes de tocar em firmware C

#### Etapa 1A: HTML Prototype — Tela 1 (Palco)
- [ ] Criar `proto_tela1.html` auto-contido
- [ ] Paleta exata: `#F3C969`, `#C9922B`, `#8A6420`, `#120C06`, `#4CD964`, `#E04A3F`, `#FFA030`
- [ ] Fonte Cinzel (Google Fonts)
- [ ] Layout com Flexbox/Grid:
  - Pedais (8 unidades) na parte inferior
  - Amps carrossel central (frente+laterais)
  - LED indicators esq/dir
  - Barra de preset inferior com nome
  - Canal do amp (canto superior direito)
  - Status WiFi (canto superior esquerdo)
  - Menu buttons esquerda/direita

#### Etapa 1B: Tradução HTML→LVGL C
- [ ] Mapear cada elemento CSS ↔ chamada LVGL equivalente
- [ ] Aplicar patches cirúrgicos em `app_main.c` e `palco.c`
- [ ] Build → flash → validar visualmente

#### Etapa 1C: Refinamentos
- [ ] Testar hitboxes de botões (independentes do desenho)
- [ ] Proteger contra acionamento acidental (swipe ≠ clique definido)
- [ ] Otimizar performance de animações (CSS `ease-out` → `lv_anim_t`)

**Ganhos esperados:** Iteração 10x mais rápida (minutos vs recompilar), menos ciclos build-flash-teste

---

### FASE 2: TEla 2 redesign + Zonas de Toque Precisas

**Objetivo:** Melhorar usabilidade das telas de configuração

- [ ] HTML prototype das subtelas de configuração
- [ ] Grid de menus (6 cards: PEDALS, AMP, VIDEO, FUNDO, REDE, SISTEMA)
- [ ] Submenus animados (abrir/fechar suave)
- [ ] Calibração precisa de zones.h (coletar dados reais de toque)
- [ ] Touch calibration TDD: coletar x,y → gerar zones → integrar

---

### FASE 3: PRESETS COMPLETOS + MIDI HANDSHAKE

**Objetivo:** Sistema completo de presets com integração MIDI

- [ ] Estrutura de dados: amp, canal, pedais físicos, boost, preset Ampero, scene Ampero, comandos MIDI, parâmetros MIDI, BPM
- [ ] Interface visual para editar presets existentes
- [ ] Protocolo MIDI handshake básico (PC, CC)
- [ ] Preparar para bidirecionalidade quando disponível

**Não implementa agora:** ADC/DAC/Looper (requer ESP32-S3)

---

### FASE 4: ESP32-S3 INTEGRATION (Hardware Futuro)

**Objetivo:** Conectar segundo módulo ESP32-S3

- Processamento de áudio
- Looper engine (ADC 24bit 48kHz, DAC, PSRAM buffer)
- Controle MIDI avançado
- Overdub/crossfade/clique-free loops

**Requer:** Hardware físico conectado + testes práticos

---

### FASE 5+: SONG MODE, AUTOMAÇÃO, ESTUDO

**Objetivo:** Funcionalidades avançadas dependentes do core estar estável

- Sequências automáticas (Intro→Verse→Chorus→Solo→Final)
- Modo Song (músidas completas com seções)
- Modo Cena/Scene completa (estado total do equipamento)
- Sincronização BPM/combo/passos
- Modo estudo (repetição trechos, velocidade variável)

---

## 💾 ECONOMIA DE TOKENS CONFIGURADA

### Arquivos de Contexto Base
- `.hermes.md` na raiz do projeto: diagrama estrutura, módulos, regras acesso
- Carrega SOLO módulo relevante por tarefa (~500 tokens vs ~9K total)

### Skills Modulares Carregadas
| Skill | Quando Usar | Arquivos Relevantes |
|---|---|---|
| `oldskull-display` | bsp_display.c, rotação LCD | bsp_display.c, esp-bsp.h, display.h |
| `oldskull-touch` | debug touch, zonas | bsp_touch.c, touch.h |
| `oldskull-pedals` | amps, carrossel | app_main.c, palco.c, oldskull_pedal.c |
| `oldskull-wifi` | ESP-NOW TX→RX | esp_wifi_remote/, protocolo packet |
| `lvgl-prototipagem-web` | prototipar tela web→LVGL | HTML prototype → tradução manual LVGL C |

### Princípios de Economia
- NUNCA ler todos os arquivos de uma vez
- Carregar SOLO o módulo da tarefa atual
- Patches cirúrgicos (nunca reescrever inteiro)
- Prototipagem web antes de codificar C
- Backup antes de grandes alterações

---

## 🔄 FLUXO DE TRABALHO PADRÃO

```
User pede mudança UI
        ↓
Criar HTML prototype (browser_exec para visualizar)
        ↓
Validar visual no browser
        ↓
Traduzir CSS→LVGL C (tabela mapeada na skill lvgl-prototipagem-web)
        ↓
Aplicar patches cirúrgicos em files específicos
        ↓
Build → Flash → Testar hardware
        ↓
Backup + Git commit
```

---

## 📁 REFERÊNCIAS DE ESPECIFICAÇÕES

- Specs técnicos: `/home/e/.hermes/skills/software-development/embedded-ui-to-web/references/oldskull-specs.md`
- Arquitetura original: arquivo de escopo fornecido pelo usuário
- Regras mestre: skill `oldskull-regras-mestre`
- Configuração display: `oldskull-display` skill

---

*Plano criado em resposta à especificação mestra do projeto OldSkull.*
*Data: Setembro 2026*