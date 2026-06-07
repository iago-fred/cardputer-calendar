# Estrutura do Cartão SD

O firmware espera estes arquivos na **raiz** do cartão microSD:

```
RAIZ DO SD
├── auth.json              ← (OBRIGATÓRIO) Credenciais OAuth2
├── wifi_config.json       ← Redes WiFi salvas (criado pelo firmware)
├── events_cache.json      ← Cache local de eventos (criado pelo firmware)
├── pending_changes.json   ← Alterações offline pendentes (criado pelo firmware)
└── last_sync.json         ← Controle de sincronização (criado pelo firmware)
```

---

## 📄 auth.json (OBRIGATÓRIO)

```json
{
  "client_id": "seu-google-client-id.apps.googleusercontent.com",
  "client_secret": "seu-google-client-secret",
  "refresh_token": "seu-refresh-token-aqui"
}
```

**Como gerar:**
1. Crie um projeto no [Google Cloud Console](https://console.cloud.google.com)
2. Ative a API do Google Calendar
3. Crie credenciais OAuth 2.0 (Desktop app)
4. Use o script `get_refresh_token.py` para obter o refresh token

---

## 📄 wifi_config.json (OPCIONAL — criado pelo firmware)

```json
{
  "networks": [
    { "ssid": "MinhaRede", "password": "senha123" },
    { "ssid": "OutraRede", "password": "outrasenha" }
  ]
}
```

Pode ser criado manualmente ou via interface do Cardputer.

---

## 📄 events_cache.json (GERADO AUTOMATICAMENTE)

```json
{
  "events": [
    {
      "id": "google-event-id",
      "summary": "Reunião",
      "description": "Descrição do evento",
      "start_date": "2026-06-07T10:00:00-03:00",
      "end_date": "2026-06-07T11:00:00-03:00",
      "is_all_day": false,
      "color_id": "2",
      "updated_ms": 1780848000000,
      "deleted": false,
      "dirty": false
    }
  ]
}
```

---

## 📄 pending_changes.json (GERADO AUTOMATICAMENTE)

```json
{
  "pending": [
    {
      "action": "create",
      "event": { "summary": "Novo evento", ... }
    }
  ]
}
```

---

## 📄 last_sync.json (GERADO AUTOMATICAMENTE)

```json
{
  "sync_token": "token-do-google",
  "last_sync_ms": 1780848000000
}
```
