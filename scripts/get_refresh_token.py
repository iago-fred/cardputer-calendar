#!/usr/bin/env python3
"""
Gera um refresh_token do Google Calendar para usar no Cardputer.

Uso:
    python3 get_refresh_token.py --client-id SEU_CLIENT_ID --client-secret SEU_CLIENT_SECRET

Você precisa:
1. Criar um projeto no Google Cloud Console
2. Ativar Google Calendar API
3. Criar OAuth 2.0 Client ID (Desktop App)
4. Adicionar http://localhost:8080 como redirect URI autorizada
"""

import argparse
import json
import socket
import sys
import urllib.request
import urllib.parse
from http.server import HTTPServer, BaseHTTPRequestHandler

SCOPES = "https://www.googleapis.com/auth/calendar"
AUTH_URL = "https://accounts.google.com/o/oauth2/auth"
TOKEN_URL = "https://oauth2.googleapis.com/token"
REDIRECT_URI = "http://localhost:8080"


def get_auth_url(client_id):
    params = urllib.parse.urlencode({
        "client_id": client_id,
        "redirect_uri": REDIRECT_URI,
        "response_type": "code",
        "scope": SCOPES,
        "access_type": "offline",
        "prompt": "consent",
    })
    return f"{AUTH_URL}?{params}"


class RedirectHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        query = urllib.parse.urlparse(self.path).query
        params = urllib.parse.parse_qs(query)
        self.server.auth_code = params.get("code", [None])[0]
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        if self.server.auth_code:
            self.wfile.write(b"<h1>✅ Autorizado!</h1><p>Volte para o terminal.</p>")
        else:
            self.wfile.write(b"<h1>❌ Erro na autorizacao</h1>")

    def log_message(self, format, *args):
        pass  # Silêncio


def exchange_code(client_id, client_secret, code):
    data = urllib.parse.urlencode({
        "code": code,
        "client_id": client_id,
        "client_secret": client_secret,
        "redirect_uri": REDIRECT_URI,
        "grant_type": "authorization_code",
    }).encode()
    req = urllib.request.Request(TOKEN_URL, data=data)
    resp = urllib.request.urlopen(req)
    return json.loads(resp.read())


def main():
    parser = argparse.ArgumentParser(description="Gera refresh_token do Google Calendar")
    parser.add_argument("--client-id", required=True, help="OAuth 2.0 Client ID")
    parser.add_argument("--client-secret", required=True, help="OAuth 2.0 Client Secret")
    args = parser.parse_args()

    auth_url = get_auth_url(args.client_id)
    print(f"\n🔗 Abra este link no navegador:\n\n{auth_url}\n")
    print("Faça login com a conta do Google Calendar desejada.")
    print("Autorize o acesso. O servidor aguardará em http://localhost:8080\n")

    server = HTTPServer(("localhost", 8080), RedirectHandler)
    server.auth_code = None
    server.timeout = 120
    server.handle_request()

    if not server.auth_code:
        print("❌ Nenhum código recebido. Certifique-se de que o redirect URI é http://localhost:8080")
        sys.exit(1)

    print("📡 Trocando código por tokens...")
    tokens = exchange_code(args.client_id, args.client_secret, server.auth_code)

    if "refresh_token" not in tokens:
        print("❌ Nenhum refresh_token retornado.")
        print("   Certifique-se de que 'access_type=offline' e 'prompt=consent' foram usados.")
        print(f"   Resposta: {json.dumps(tokens, indent=2)}")
        sys.exit(1)

    print("\n" + "=" * 60)
    print("✅ Refresh Token gerado com sucesso!")
    print("=" * 60)
    print("\nSalve no arquivo auth.json no cartão SD:\n")
    auth_json = {
        "client_id": args.client_id,
        "client_secret": args.client_secret,
        "refresh_token": tokens["refresh_token"],
    }
    print(json.dumps(auth_json, indent=2))

    print(f"\n🔐 Access Token (válido por {tokens.get('expires_in', 3600)}s):")
    print(f"    {tokens.get('access_token', 'N/A')[:50]}...\n")


if __name__ == "__main__":
    main()
