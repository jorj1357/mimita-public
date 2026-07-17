#!/bin/sh
set -e

echo "=== 1. ICE Host ==="
RESULT=$(curl -s -X POST http://localhost:3001/api/coordinator/ice/host \
  -H 'Content-Type: application/json' \
  -d '{"host_session_id":"test_host","ice_description":"a=ice-ufrag:abc a=ice-pwd:def"}')
echo "$RESULT"
ROOM=$(echo "$RESULT" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("room_code",""))' 2>/dev/null)
echo "Room: $ROOM"

echo ""
echo "=== 2. ICE Lookup ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/lookup \
  -H 'Content-Type: application/json' \
  -d "{\"code\":\"$ROOM\"}"
echo ""

echo ""
echo "=== 3. Invalid SDP probe rejection ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"prober\",\"ice_description\":\"probe\"}"
echo ""

echo ""
echo "=== 4. Client 1 valid join ==="
JOIN1=$(curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"client1\",\"ice_description\":\"a=ice-ufrag:aaa a=ice-pwd:bbb\"}")
echo "$JOIN1"
REQ1=$(echo "$JOIN1" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("request_id",""))' 2>/dev/null)
echo "Req1: $REQ1"

echo ""
echo "=== 5. Client 2 valid join ==="
JOIN2=$(curl -s -X POST http://localhost:3001/api/coordinator/ice/begin-join \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"client_session_id\":\"client2\",\"ice_description\":\"a=ice-ufrag:ccc a=ice-pwd:ddd\"}")
echo "$JOIN2"
REQ2=$(echo "$JOIN2" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("request_id",""))' 2>/dev/null)
echo "Req2: $REQ2"

echo ""
echo "=== 6. Host poll (should see 1 pending) ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/host-poll \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"host_session_id\":\"test_host\"}"
echo ""

echo ""
echo "=== 7. Host answers request 1 ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/host-answer \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"host_session_id\":\"test_host\",\"request_id\":\"$REQ1\",\"host_peer_sdp\":\"a=ice-ufrag:host1 a=ice-pwd:hostkey1\"}"
echo ""

echo ""
echo "=== 8. Client 1 polls ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/client-poll \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"request_id\":\"$REQ1\"}"
echo ""

echo ""
echo "=== 9. Request complete ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/request-complete \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"request_id\":\"$REQ1\"}"
echo ""

echo ""
echo "=== 10. Host poll again (sees client 2 now) ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/host-poll \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"host_session_id\":\"test_host\"}"
echo ""

echo ""
echo "=== 11. Validate join token ==="
JTOKEN=$(echo "$JOIN1" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("join_token",""))' 2>/dev/null)
curl -s -X POST http://localhost:3001/api/coordinator/ice/validate-join \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"join_token\":\"$JTOKEN\"}"
echo ""

echo ""
echo "=== 12. Idempotent re-validation ==="
curl -s -X POST http://localhost:3001/api/coordinator/ice/validate-join \
  -H 'Content-Type: application/json' \
  -d "{\"room_code\":\"$ROOM\",\"join_token\":\"$JTOKEN\"}"
echo ""

echo ""
echo "=== ALL TESTS COMPLETE ==="
