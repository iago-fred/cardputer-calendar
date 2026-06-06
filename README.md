# Neon Calendar — Cardputer

Um calendário inteligente para o **M5Stack Cardputer** que sincroniza com o Google Calendar.

![Build](https://github.com/IagoFrederick/cardputer-calendar/actions/workflows/build.yml/badge.svg)

## ✨ Funcionalidades

- 📅 **Ver eventos** do dia com cores personalizadas
- ➕ **Criar eventos** diretamente pelo Cardputer
- ✏️ **Editar eventos** existentes
- ❌ **Excluir eventos**
- 💾 **Offline first** — cache completo no cartão SD
- 🔄 **Sync bidirecional** — alterações offline são sincronizadas ao conectar
- 📶 **WiFi Manager** — escaneia, salva e gerencia redes pelo SD
- 🔐 **OAuth2 direto** — sem backend intermediário, refresh_token no SD

## 📦 Estrutura do Cartão SD

```
📁 SD CARD ROOT/
├── auth.json            ← Credenciais Google OAuth2
├── wifi_config.json     ← Redes WiFi salvas
├── events_cache.json    ← Cache offline de eventos
├── pending_changes.json ← Alterações pendentes de sync
└── last_sync.json       ← Token de sincronização
```

### `auth.json` — Gerar Refresh Token

1. Vá para [Google Cloud Console](https://console.cloud.google.com/)
2. Crie um projeto → **APIs & Services** → **Credentials**
3. Crie um **OAuth 2.0 Client ID** (Desktop App)
4. Ative a **Google Calendar API**
5. Gere um refresh_token usando o script `scripts/get_refresh_token.py`

```json
{
  "client_id": "xxxxx.apps.googleusercontent.com",
  "client_secret": "GOCSPX-xxxxx",
  "refresh_token": "1//0g8t..."
}
```

### `wifi_config.json`

```json
{
  "networks": [
    { "ssid": "MinhaRede", "password": "minha_senha" }
  ]
}
```

## 🔧 Compilando

### Com PlatformIO (recomendado)

1. Instale [VS Code](https://code.visualstudio.com/) + [PlatformIO](https://platformio.org/)
2. Clone o repo: `git clone https://github.com/IagoFrederick/cardputer-calendar.git`
3. Abra a pasta no PlatformIO
4. Compile: `pio run`
5. Upload: `pio run --target upload`

### CLI

```bash
pip install platformio
pio run
pio run --target upload
```

## ⌨️ Navegação

| Tecla | Ação |
|-------|------|
| `SET` | Confirmar / Abrir evento |
| `ESC` | Voltar |
| `▲ / ▼` | Navegar na lista |
| `DEL` | Excluir evento |
| `Fn + W` | Abrir WiFi scan |

## 🗺️ Roadmap

- [x] CRUD de eventos
- [x] Offline cache
- [x] Sync bidirecional
- [x] WiFi manager
- [ ] Vista semanal
- [ ] Notificações com buzzer
- [ ] Tempo / clima na tela inicial

---

Feito com 💙 por Neon 👻
