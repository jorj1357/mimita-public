# Mimita.fun Website

React/Vite frontend with an Express, PostgreSQL, and SMTP account service.

## Local Setup

Requirements:

- Node.js 20+
- PostgreSQL 15+

```powershell
cd C:\important\mimita-priv-v8\website
Copy-Item .env.example .env
npm install
npm run migrate
```

Run the API and frontend in separate terminals:

```powershell
npm run server
npm run dev
```

Vite proxies `/api` to `http://localhost:3001`.

## PostgreSQL Setup

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y postgresql postgresql-contrib
sudo systemctl enable --now postgresql
sudo -u postgres psql
```

In `psql`:

```sql
CREATE USER mimita_app WITH PASSWORD 'replace_with_a_strong_password';
CREATE DATABASE mimita OWNER mimita_app;
\q
```

Set `DATABASE_URL` in `.env`, then run:

```bash
npm ci
npm run migrate
```

The migration creates:

- `users`
- `sessions`
- `password_change_codes`
- `newsletter`

The source schema is
`server/migrations/001_auth.sql`. The application migration command is
idempotent and can be run during every deployment.

## Environment

Start with `.env.example`.

Auth:

- `SESSION_SECRET`: long random secret used when hashing opaque session and
  verification tokens.
- `SESSION_COOKIE_NAME`: defaults to `mimita_session`.
- `SESSION_DAYS`: session lifetime, defaults to 30.
- `APP_ORIGIN`: allowed browser origin.
- `NODE_ENV=production`: enables secure cookies and requires
  `SESSION_SECRET`.

Database:

- `DATABASE_URL`: preferred PostgreSQL connection string.
- `DB_SSL=true`: use for a managed PostgreSQL service that requires TLS.
- `DB_USER`, `DB_HOST`, `DB_NAME`, `DB_PASSWORD`, `DB_PORT`: alternative to
  `DATABASE_URL`.

Mail:

- `SMTP_HOST`
- `SMTP_PORT`
- `SMTP_SECURE`
- `SMTP_USER`
- `SMTP_PASS`
- `MAIL_FROM`

Generate a session secret:

```bash
node -e "console.log(require('crypto').randomBytes(48).toString('hex'))"
```

## VPS Deployment

Example Ubuntu deployment using Nginx and systemd:

```bash
sudo apt install -y nginx nodejs npm postgresql
sudo mkdir -p /var/www/mimita
sudo chown "$USER":"$USER" /var/www/mimita
cd /var/www/mimita
git clone YOUR_REPOSITORY_URL .
cd website
npm ci
npm run migrate
npm run build
```

Create `/etc/systemd/system/mimita-api.service`:

```ini
[Unit]
Description=Mimita website API
After=network.target postgresql.service

[Service]
Type=simple
User=www-data
WorkingDirectory=/var/www/mimita/website
EnvironmentFile=/var/www/mimita/website/.env
ExecStart=/usr/bin/node server/server.js
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Enable the API:

```bash
sudo chown -R www-data:www-data /var/www/mimita/website
sudo systemctl daemon-reload
sudo systemctl enable --now mimita-api
sudo systemctl status mimita-api
```

Nginx server block:

```nginx
server {
    listen 80;
    server_name mimita.fun www.mimita.fun;

    root /var/www/mimita/website/dist;
    index index.html;

    location /api/ {
        proxy_pass http://127.0.0.1:3001;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

Enable HTTPS after DNS points to the VPS:

```bash
sudo ln -s /etc/nginx/sites-available/mimita.fun \
  /etc/nginx/sites-enabled/mimita.fun
sudo nginx -t
sudo systemctl reload nginx
sudo apt install -y certbot python3-certbot-nginx
sudo certbot --nginx -d mimita.fun -d www.mimita.fun
```

Production `.env` must include:

```dotenv
NODE_ENV=production
PORT=3001
APP_ORIGIN=https://mimita.fun
DATABASE_URL=postgresql://mimita_app:password@127.0.0.1:5432/mimita
SESSION_SECRET=long_random_value
SMTP_HOST=smtp.provider.example
SMTP_PORT=587
SMTP_SECURE=false
SMTP_USER=hello@mimita.fun
SMTP_PASS=mail_password
MAIL_FROM="Mimita <hello@mimita.fun>"
```

## Auth Routes

- `POST /api/auth/signup`
- `POST /api/auth/signin`
- `POST /api/auth/signout`
- `GET /api/auth/me`
- `POST /api/auth/password-change/request`
- `POST /api/auth/password-change/verify`
- `POST /api/auth/password-change/finalize`
- `PATCH /api/account/profile`
- `PATCH /api/account/notification-preferences`
- `DELETE /api/account`
- `GET /api/users/:username`

Passwords use Node's `scrypt` with random salts. Session and verification
tokens are random, stored only as hashes, and have explicit expiration.
All SQL values use PostgreSQL parameters.

## Validation

```bash
npm test
npm run lint
npm run build
```
