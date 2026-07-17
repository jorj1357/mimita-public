#!/bin/sh
set -e

# Source the env
. /root/mimita-coordinator/env.sh

# Replace placeholder with actual secret in turnserver.conf
sed -i "s/TEMP_PLACEHOLDER/$MIMITA_TURN_SECRET/g" /etc/turnserver.conf
echo "=== turnserver.conf updated ==="

# Restart services
pm2 restart /root/mimita-coordinator/server.js --name mimita-coordinator --update-env
pm2 save
systemctl restart coturn
echo "=== Services restarted ==="

sleep 2

# Verification
echo "=== PM2 status ==="
pm2 status 2>/dev/null | head -10
echo "=== Coturn port ==="
ss -tulpn | grep 3478 || echo "coturn not listening"
echo "=== Coordinator port ==="
ss -tulpn | grep 3001 || echo "coordinator not listening"

# Test endpoints
echo ""
echo "=== Test: ICE host ==="
RESULT=$(curl -s -X POST http://localhost:3001/api/coordinator/ice/host -H 'Content-Type: application/json' -d '{"host_session_id":"test1","ice_description":"a=ice-ufrag:xxx a=ice-pwd:yyy"}')
echo "$RESULT"
ROOM=$(echo "$RESULT" | python3 -c "import sys,json; print(json.load(sys.stdin).get('room_code',''))" 2>/dev/null)
echo "Room: $ROOM"

echo ""
echo "=== Test: ICE lookup (non-mutating) ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/lookup -H 'Content-Type: application/json' -d "{\"code\":\"$ROOM\"}"
echo ""

echo ""
echo "=== Test: probe rejection ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join -H 'Content-Type: application/json' -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"prober\",\"ice_description\":\"probe\"}"
echo ""

echo ""
echo "=== Test: valid begin-join ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join -H 'Content-Type: application/json' -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"client1\",\"ice_description\":\"a=ice-ufrag:aaa a=ice-pwd:bbb\"}"
echo ""

echo ""
echo "=== Test: second client ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join -H 'Content-Type: application/json' -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"client2\",\"ice_description\":\"a=ice-ufrag:ccc a=ice-pwd:ddd\"}"
echo ""

echo ""
echo "=== Test: host-poll (should see 2 pending) ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/host-poll -H 'Content-Type: application/json' -d "{\"room_code\":\"$ROOM\",\"host_session_id\":\"test1\"}"
echo ""

echo ""
echo "=== Test: TURN credentials ==="
curl -s -X POST http://localhost:3001/api/coordinator/turn-credentials -H 'Content-Type: application/json' -d '{}'
echo ""
