#!/bin/bash
set -e

# Source the TURN secret
source /root/mimita-coordinator/env.sh

# Replace placeholder in turnserver conf
sed -i "s/TEMP_PLACEHOLDER/$MIMITA_TURN_SECRET/g" /etc/turnserver.conf

# Modify server.js to load env.sh if it exists
if ! grep -q "env.sh" /root/mimita-coordinator/server.js; then
    sed -i '1i require("fs").existsSync("/root/mimita-coordinator/env.sh") && require("dotenv").config({path:"/root/mimita-coordinator/env.sh"});' /root/mimita-coordinator/server.js
fi

# Alternative: manually set in PM2
pm2 stop mimita-coordinator 2>/dev/null || true
pm2 delete mimita-coordinator 2>/dev/null || true
MIMITA_TURN_SECRET="$MIMITA_TURN_SECRET" pm2 start /root/mimita-coordinator/server.js --name mimita-coordinator --update-env
pm2 save

# Restart coturn
systemctl restart coturn

echo "=== Services restarted ==="
sleep 2

# Verify
echo "=== PM2 status ==="
pm2 status mimita-coordinator
echo "=== Coturn status ==="
systemctl status coturn 2>&1 | head -5
echo "=== Listening ports ==="
ss -tulpn | grep -E '3478|3001'

# Test coordinator ICE host endpoint
echo "=== Testing ICE host ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/host \
  -H 'Content-Type: application/json' \
  -d '{"host_session_id":"test","ice_description":"test sdp a=ice-ufrag:xxx a=ice-pwd:yyy"}'
echo ""

# Test lookup
echo "=== Testing lookup ==="
HOST_RESULT=$(curl -s -X POST http://localhost:3001/api/coordinator/ice/host \
  -H 'Content-Type: application/json' \
  -d '{"host_session_id":"test2","ice_description":"test sdp a=ice-ufrag:xxx a=ice-pwd:yyy"}')
ROOM_CODE=$(echo "$HOST_RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('room_code',''))" 2>/dev/null || echo "")
echo "Room code: $ROOM_CODE"
if [ -n "$ROOM_CODE" ]; then
    curl -s -X POST http://localhost:3001/api/coordinator/ice/lookup \
      -H 'Content-Type: application/json' \
      -d "{\"code\":\"$ROOM_CODE\"}"
    echo ""
    # Test probe rejection
    echo "=== Testing invalid SDP rejection ==="
    curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join \
      -H 'Content-Type: application/json' \
      -d "{\"room_code\":\"$ROOM_CODE\",\"client_session_id\":\"probe_client\",\"ice_description\":\"probe\"}"
    echo ""
    # Test valid begin-join
    echo "=== Testing valid begin-join ==="
    curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join \
      -H 'Content-Type: application/json' \
      -d "{\"room_code\":\"$ROOM_CODE\",\"client_session_id\":\"real_client\",\"ice_description\":\"a=ice-ufrag:xxx\na=ice-pwd:yyy\"}"
    echo ""
fi

echo "=== Turn credentials test ==="
curl -s -X POST http://localhost:3001/api/coordinator/turn-credentials \
  -H 'Content-Type: application/json' \
  -d '{}'
echo ""
