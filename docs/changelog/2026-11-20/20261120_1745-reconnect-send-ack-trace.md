# Reconnect send and acknowledgment trace

- Task: Continue the end-to-end MiMITA connection reliability issue (`issue-cf9cd77eb056`).
- Source review: `sendReconnectRequest` returns success when the selected local send API accepts the request; the client logs `send_ok` and expects `network.server.reconnect.received`. The server handler logs receipt before token lookup, then logs lookup acceptance/rejection, sends `ReconnectAccept`, and emits acceptance. These are distinct transitions.
- Runtime evidence: In gen404 run `20261120_152743`, repeated client `attempt_sent` events had no matching server `reconnect_request_received` / `network.server.reconnect.received` in the observed window. The server continued snapshot sends to the peer before its peer list later emptied. This narrows the missing transition to delivery/receive/decoder before the handler, subject to journal coverage and clock alignment; it does not establish the cause.
- Shared the source/runtime evidence with the active issue owner and appended it to the canonical issue discussion. No source code or live runs were changed.
- Validation: read-only source and journal review. No build or runtime proof performed in this activation.
- Outstanding: trace transport send/delivery/receive and correlate request attempts to server receipt, lookup, accept send, client accept, and resumed snapshots; continue the lifecycle matrix and Payload visual proof under the umbrella issue.

## Transport API boundary follow-up

- Further source trace: CAP_TRANSPORT send falls through `HostRuntime::transportSendThunk` to `HostTransportAdapter::sendToServer`, then the active `IGameTransport::send`. For ICE this reaches `IceTransport::send` and `IceAgent::send`, whose success means `juice_send` returned `JUICE_ERR_SUCCESS`; it does not confirm peer delivery or server receipt.
- Shared this boundary and the recommended correlated stages (server ingress/decoder, handler receive, token lookup, reconnect-accept send, client accept receive, resumed snapshots) with the active issue owner and appended it to `issue-cf9cd77eb056`.
- No runtime, build, or source change was made.
