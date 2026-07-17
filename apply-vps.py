#!/usr/bin/env python3
import os, sys

# Read the env
env_file = "/root/mimita-coordinator/env.sh"
secret = None
with open(env_file) as f:
    for line in f:
        if line.startswith("MIMITA_TURN_SECRET="):
            secret = line.strip().split("=", 1)[1]
            break

if not secret:
    print("ERROR: MIMITA_TURN_SECRET not found")
    sys.exit(1)

print(f"Using secret (length {len(secret)})")

# Update turnserver.conf
with open("/etc/turnserver.conf") as f:
    content = f.read()
content = content.replace("TEMP_PLACEHOLDER", secret)
with open("/etc/turnserver.conf", "w") as f:
    f.write(content)
print("=== turnserver.conf updated ===")

# Restart services
os.system("pm2 restart /root/mimita-coordinator/server.js --name mimita-coordinator --update-env")
os.system("pm2 save")
os.system("systemctl restart coturn")
print("=== Services restarted ===")
