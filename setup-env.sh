#!/bin/sh
SECRET=$(openssl rand -base64 32)
echo "MIMITA_TURN_SECRET=$SECRET" > /root/mimita-coordinator/env.sh
chmod 600 /root/mimita-coordinator/env.sh
echo "OK secret=$SECRET"
