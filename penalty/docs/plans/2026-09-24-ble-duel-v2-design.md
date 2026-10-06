<p align="right">
  <a href="2026-09-24-ble-duel-v2-design.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Penalty v2.0.0 Two-Device BLE Duel Design

Date: 2026-09-24 · Status: M3 device main path confirmed; M4 build/host tests passed, device tests pending · Target: v2.0.0 · Protocol: `penalty-ble/1.0`

## 1. Decision and boundaries

The user confirmed that “combo” was a typo and the requested feature is a connected BLE duel: two AI Passports running the same Penalty v2.0.0 firmware connect directly without a phone, Wi-Fi, or cloud. One player shoots while the other keeps and roles swap after each kick.

Use equal products with one authority per match. Either device can create or join. The creator is the BLE Peripheral, GATT Server, and authoritative Host for that match; the joiner is the Central, GATT Client, and Guest. Link roles never determine the first shooter. Once both reach the coin page, only the creator can press OK to trigger the toss; firmware still locks a random result, so the creator cannot choose the side or order.

The v2.0.0 multiplayer path must remain isolated from v1.5.0 single-device play:

- Shootout, Shoot Practice, and Keeper Practice continue to use the existing `penalty_model` with unchanged rules, probabilities, input timing, UI, and audio.
- NimBLE starts only after entering Multiplayer and is fully stopped on exit.
- UP long always retains first priority and returns to the factory Launcher. In-game OK long still returns to the Penalty cover, cover OK long remains consumed, and startup must not call `launcher_contract_mark_valid()`.
- M1–M3 are implemented, the M3 device main path is user-confirmed, and M4 extends the production UI to full matches and recovery. The isolated M2 hardware-diagnostic page remains available: from the cover, hold DOWN to open `BLE LINK TEST`. The diagnostic uses lobby `4072`: UP starts Create/Host, DOWN starts Join/Guest, both press OK only when the displayed six-digit values match, and the Guest then runs encrypted ping and snapshot checks automatically. Hold OK to stop BLE and return to the cover. This temporary page is not the production lobby or game flow. Do not change the release version or claim v2.0.0 is deliverable before full two-device acceptance is complete. Development approval does not authorize commit, push, publish, or flash.

### Current baseline evidence

- The authoritative checkout is `feature/penalty-ble-v2` at the committed M1 baseline `3ba1f63f7d74dfb12299f0b7846f163043617890`. This design does not read or modify work on `release/penalty-1.x`.
- The current `penalty_model` is single-device and player-centric. `aim()` locks the AI direction, green range, and result randomness at the start of a kick; `settle()`/`settle_match()` applies a result once; `penalty_model_resync()` resets only uncommitted input without rerolling hidden decisions. Those invariants are the basis for multiplayer exactly-once and reconnect behavior.
- At the v1.5.0 baseline, `sdkconfig.defaults` enabled NimBLE Peripheral/Broadcaster only and disabled Central/Observer. M2 enables both role pairs plus the GATT client and Secure Connections while retaining one connection and no bond persistence. The unrelated `demo_ble.c` remains outside the Penalty build.
- The v1.5.0 ESP-IDF 5.5.3 build is recorded PASS at 1,496,832 application bytes, but no v1.5.0 two-device, heap/stack, or device test exists. The approximately 249,200-byte free heap and 114,688-byte largest block are older v1.4.1 startup evidence and are not presented as a current v1.5.0 measurement.
- The existing `main/main.c` shell intercepts UP long before page input, calls `launcher_contract_return_to_factory()`, and logs every error. That ordering is a hard v2.0.0 contract.

## 2. Goals, non-goals, and default play

### Goals

1. Discover, confirm, securely connect, bind one match, play a complete shootout, and recover a short disconnection with two devices only.
2. Lock each input from its source-device timestamp so network latency cannot change an already captured direction or timing value.
3. Let the Host settle exactly once and distribute one result, score, and recoverable snapshot to both peers.
4. Make duplicates, reordering, loss, and brief disconnects converge or abort explicitly, never double-score, reroll, skip, or silently fork.
5. Keep memory bounded. Do not transmit animation frames or ball coordinates and do not allocate per packet.

### Non-goals

- No online matchmaking, accounts, leaderboard, spectators, more than two players, or cross-version replay.
- No malicious-firmware attestation or esports-grade anti-cheat. The model trusts official matching firmware while defending against accidental peers, casual eavesdropping, replay, and injection.
- No resume across either device reboot in v2.0.0.
- No Launcher, OTA, NVS erase, file, or non-Penalty command in the GATT surface.
- No Wi-Fi/BLE coexistence during the match.

### Default flow

1. Keep the five-row title. Selecting Shootout opens a second level with Single vs AI, Create Multiplayer, Join Multiplayer, and Back.
2. The creator advertises a time-limited connectable lobby and displays a four-digit lobby code. The joiner lists only compatible Penalty lobbies.
3. After the joiner chooses the same lobby, both devices display and confirm the same six-digit security comparison value.
4. Both enter the same coin page. The creator presses OK, then the Host randomly locks and publishes the coin result and order; the joiner cannot trigger or choose it.
5. Both players act simultaneously and blind: the shooter selects target/power while the keeper independently selects target/timing. Neither side sees the other choice or progress.
6. Both first send action commitments, reveal only after both commitments exist, and let the Host verify and resolve with pre-committed randomness. Both devices animate locally.
7. Roles swap after each kick. Each side takes at most five regular kicks, with v1.5.0 early finish and paired sudden death.

### Recommended second-level menu

```text
SHOOTOUT

> SINGLE VS AI
  CREATE MULTIPLAYER
  JOIN MULTIPLAYER
  BACK

UP/DOWN SELECT · OK CONFIRM
```

Single vs AI enters the unchanged v1.5.0 shootout with one extra confirmation. Create shows the four-digit lobby code; Join scans compatible codes. This avoids a sixth title row and keeps Settings/Help positions unchanged. The extra single-player confirmation remains a product decision.

## 3. Roles, identity, and match binding

Keep product identity, GAP role, GATT role, authority, and gameplay role separate:

| Dimension | Creator | Joiner | Meaning |
| --- | --- | --- | --- |
| Product | peer A | peer B | Same firmware and features |
| BLE GAP | Peripheral | Central | Advertising/scanning and connection only |
| GATT | Server | Client | Attribute transport only |
| Match authority | Host | Guest | Host owns state, randomness, settlement, and snapshots |
| Current kick | Shooter or Keeper | Keeper or Shooter | Swaps each kick, independent of BLE role |

Generate a random 64-bit `boot_instance_id` on each boot and a random 128-bit `match_id` when creating. Use `match_epoch` to invalidate old connections, record `peer_instance_id` to prevent takeover, and derive a short `session_tag` for regular packets. A BLE address is not a permanent player identity.

Binding begins after both users accept the secure comparison and lobby settings. It ends on match completion, cancellation, reconnect timeout, or reboot. Keep match state, resume material, and temporary bonding in RAM by default. Re-pairing explicitly abandons the old match, advances the epoch, clears temporary keys and caches, then returns to Create/Join. A third device can never silently replace a recoverable peer.

The creator can trigger but cannot select the random coin outcome. Host/Guest identity remains unchanged after reconnect.

## 4. Discovery and secure connection

- Assign a dedicated 128-bit Service UUID for `penalty-ble/1.x`. Put only flags, UUID, and essential data in the legacy advertisement; put lobby code, minor version, and capability bits in the scan response and stay within both 31-byte limits.
- Advertise at about 100 ms for the first 10 seconds, then about 500 ms. Stop after 60 seconds and offer retry. Active scanning lists only fresh, parseable compatible lobbies. RSSI may sort results but is not identity or distance proof.
- Stop accepting a second peer once security starts. Keep `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`.
- The four-digit lobby code prevents user selection mistakes; it is not authentication. Require LE Secure Connections, MITM protection, encryption, and Numeric Comparison using both screens and three-button confirmation.
- The six-digit comparison is not a second lobby password and is not typed. BLE generates it temporarily so users can verify that the two devices in front of them negotiated the same encrypted connection. The four-digit code answers “which lobby”; the six-digit value answers “was either endpoint replaced during pairing?” A mismatch is rejected.
- Allow the match Control Point only after the connection is encrypted and authenticated. Reject and disconnect on security failure, user rejection, timeout, or protocol incompatibility. Just Works or a fixed passkey is development-only.
- Do not persist a bond across reboot by default. If the chosen ESP-IDF 5.5.3 NimBLE storage path requires persistent bonding, separately review the NVS namespace, deletion lifecycle, and full-image reset behavior rather than reusing language-preference keys.

## 5. Session state and authoritative kick flow

Use a separate pure-C session state machine:

```text
OFF
  -> CREATE_ADVERTISING | JOIN_SCANNING
  -> CONNECTING -> SECURING -> BINDING -> LOBBY_READY
  -> COIN_READY -> COIN_TOSS -> ROUND_OPEN
  -> WAIT_ACTION_COMMITS -> WAIT_ACTION_REVEALS
  -> RESOLVING -> RESULT_COMMITTED
  -> HANDOFF | MATCH_SUMMARY
  -> PAUSED_RECONNECT -> MATCH_ABORTED | OFF
```

NimBLE callbacks only copy bounded events into a queue. One session task parses protocol and advances pure-C state. UI renders state through the existing locked LVGL path.

For each kick:

1. The Host creates `round_id`, locks two random result values and their commitment, then sends delayed `ROUND_OPEN`. It may not reroll after seeing input.
2. Shooter and Keeper act simultaneously with their existing six-target and meter controls. Neither UI exposes opponent target, progress, or completion time.
3. Each sends `ACTION_COMMIT = SHA-256(match_id, round_id, role, action fields, action_nonce)`. The Host must fix its local commitment before seeing the Guest reveal.
4. After both commitments, the Host sends `REVEAL_REQUEST`; both reveal action and nonce. A reveal that does not match its commitment causes protocol recovery, not settlement.
5. The Host validates both revealed actions and exactly-once state, then creates `result_id` and `ROUND_RESULT` once.
6. The committed result remains in the RAM snapshot. After delivery confirmation, `ANIMATION_START(delay_ms)` lets both devices animate and play audio locally. Frames are never sent over BLE.
7. Score changes only on first application of `result_id`; animation completion is presentation only.

The two v1.5.0 probability tables were designed separately against AI and cannot be silently combined. The confirmed PvP rule first checks shot-on-target probability (perfect 95%, green 80%, legal outside 35%; weak/high/timeout miss), then applies keeper save probability only for an on-target shot (exact target 90/65/25%, same column other row 35/20/5%, wrong column 0%). Use two pre-committed 0–99 values so both peers can recompute. This makes perfect shot versus perfect exact save about a 9.5% goal; the rule is approved for implementation but still requires device playtesting and may need a later balance revision.

With directional cues removed, multiplayer difficulty no longer controls the 900/600/350 ms cue duration. The recommended meaning is identical meter speed and green-window width on both devices. Any additional difficulty effect must be shown in the lobby and cannot give Host and Guest different windows.

Local actions become immutable before send, and remote hidden actions cannot enter either decision UI before both commitments. Commit/reveal is required in v2.0.0 so the creator/Host cannot wait for the Guest's raw choice before acting. It still does not resist modified malicious Host firmware.

## 6. GATT and application protocol

Use four characteristics:

| Characteristic | Direction | Property | Purpose |
| --- | --- | --- | --- |
| Protocol Info | Server to Client | Read | Protocol, firmware, capabilities, minimum compatibility |
| Control Point | Client to Server | Write with Response | Guest action, confirmation, snapshot request, exit |
| Event Stream | Server to Client | Indicate | Host event, result, error, snapshot fragment |
| Session Status | Server to Client | Read + Notify | Lightweight state, seq/epoch, recovery entry |

Critical state never uses Write Without Response. GATT acknowledgements prove transport, not business application, so retain `seq`, `ack_seq`, `action_id`, and `result_id`.

Firmware version is `2.0.0`; protocol version is independently `penalty-ble/1.0`; snapshot schema is `1`. Reject a different protocol major. With the same major, negotiate the common capability set and reject missing required bits. Initial required capabilities are `DUEL_V1`, `COMMIT_REVEAL_V1`, `SNAPSHOT_V1`, `RECONNECT_V1`, and `SC_NUMERIC_COMPARE`.

The logical envelope contains protocol version, message type/flags, session tag/epoch, sender and acknowledged sequence, round/phase, payload length, and payload. Handshake/snapshot carries full `match_id`; regular traffic uses `session_tag`. Request a 64–128-byte ATT MTU but remain correct at the default MTU 23 by using bounded fragments with message ID, index, count, and total length. Cap one logical message at 256 bytes and use compile-time buffer limits.

Reliability rules:

- Independent monotonic 32-bit `seq` in each direction and a match-unique `action_id` per user action.
- Duplicate or old seq replays the cached ACK/result without applying again.
- A future seq gap is not guessed; request authoritative state and do not apply it.
- Wrong epoch, round, phase, or role is rejected with current state.
- Retry an unacknowledged business message after 300 ms, at most three times, then request a snapshot. Continued failure pauses the match; timeout is never a goal.
- Drop only reconstructible status updates on queue pressure. Never overwrite an unconfirmed action, result, or exit.
- Log only sanitized type, length, seq, and error; never log comparison values, full match IDs, or keys.

The Host snapshot includes protocol/schema, full match/epoch, peer instances, phase, difficulty, order, round, attempts, score, roles, each side's commit/reveal completion, most recent committed result, seq/ack values, and match result. Until both reveals complete, it contains no hidden target, power, timing, or nonce. Guest applies the Host snapshot rather than merging local score; an unacknowledged action may be resent unchanged.

## 7. Timing and latency

- Judge input from the originating device monotonic timestamp, never receive time. Host phase timers use Host monotonic time; wall clocks and cross-device epochs are unnecessary.
- Request 30–45 ms interval and latency 0 during active input; request 60–90 ms and peripheral latency 3 while waiting. Negotiation failure must degrade gracefully and be logged.
- Device acceptance targets: P95 local lock to Host ACK at or below 150 ms; P95 final action lock to result animation starting on both devices at or below 300 ms; start skew at or below 100 ms in a stable same-room link. These are targets, not current facts.
- `ANIMATION_START` uses a relative 150–200 ms delay. The protocol never synchronizes every animation frame.

## 8. Loss, disconnect, and re-pairing

Freeze input on disconnect and never award a goal because a peer disappeared.

| Disconnect point | Recovery |
| --- | --- |
| Before either commitment | Resume the same round, discard uncommitted local choices, consume no kick |
| One commitment only | Preserve the hash without reveal; the other side may choose and commit after reconnect |
| Both committed, reveals incomplete | Preserve commitments and continue reveal; neither action may change |
| Both revealed, result not committed | Host settles once from the same actions/randomness |
| Result committed, animation incomplete | Snapshot carries the same result; show a stable result once without rescoring |
| Handoff or summary | Restore the same score and page; never auto-start the next kick |

Use a 30-second recovery window. The original Peripheral advertises only the same resumable session; the original Central scans only for the same peer/session. After encryption, exchange `RESUME(match_id, epoch, last_applied_seq, state_digest)`, advance the epoch, and apply a Host snapshot. Timeout defaults to an abandoned match with no winner. Reboot ends the match.

No-winner abandonment is recommended for the first release because BLE loss can come from range, battery, interference, or reboot, and the devices cannot reliably distinguish deliberate avoidance from failure. With no accounts, ranking, or season points, a false forfeit costs more than it adds. A later competitive mode can separately define immediate forfeit on explicit exit and forfeit after link-recovery timeout.

OK long cancels locally and best-effort sends `MATCH_END(USER_CANCELLED)` without blocking cover return. UP long remains shell-owned, precedes page input, and directly follows the Launcher contract without added UI text.

## 9. Code boundaries and lifecycle

Recommended files:

```text
main/penalty_model.[ch]            existing v1.5 single-player model
main/penalty_duel_model.[ch]       new pure-C duel and snapshot rules
main/penalty_ble_protocol.[ch]     new pure-C codec, framing, version, seq
main/penalty_ble_session.[ch]      new NimBLE/GATT discovery/security/reconnect
main/penalty_multiplayer.[ch]      new session/UI/input coordinator
main/penalty_app.c                  multiplayer lifecycle integration only
main/penalty_ui.c / penalty_i18n.c new bilingual lobby/recovery/error UI
tests/test_penalty_duel_model.c
tests/test_penalty_ble_protocol.c
tests/test_penalty_ble_session.c
tests/test_penalty_shell_contract.py
```

Do not ship or copy the `demo_ble.c` page; reuse only applicable initialization and bounded shutdown patterns. BLE stays in `main`, not BSP.

GAP/GATT callbacks do bounded validation and queue delivery only. A single session task owns handles, sequences, retransmission, snapshots, and NimBLE state. On multiplayer exit, first reject new events, stop scan/advertising/connection and NimBLE host with a bounded handshake, then delete queues and UI. A failed stop retains a retryable state. Single-player modes create no NimBLE task, transport queue, or security state.

## 10. Memory, Flash, and power budgets

The ESP32-C3 has no PSRAM. Display, the 24 KB LVGL pool, audio, NimBLE, stacks, and DMA compete for internal RAM. The v1.4.1 device once reported about 249,200 bytes free heap and a 114,688-byte largest block; v1.5.0 has no equivalent board measurement, so these values guide testing but do not prove v2 headroom.

Targets:

- Keep all application protocol queues, snapshots, retransmit caches, and fragments within about 8 KB, with no per-packet heap allocation.
- Keep one connection, one bond, and minimal CCCD/GATT procedures. Do not disable security merely to save roughly 2 KB.
- With BLE connected and multiplayer UI/audio idle, target at least 160 KB free heap and a 64 KB largest block; record delta against the same v1.5.0 page.
- Record task stack high-water with at least 768 bytes target headroom. Fifty enter/exits, fifty reconnects, and twenty matches must show no progressive leak, task leak, watchdog, or reboot.
- Keep the current 8 MB layout and Launcher app-slot contract. No new partition or NVS erase is required.

These are provisional engineering budgets and must be adjusted from actual build and device evidence.

Enable BLE only inside Multiplayer. Stop advertising/scanning on timeout or exit. Use low latency during input and reduced duty cycle while waiting. Preserve current 60-second backlight dimming. Do not enable automatic Light-sleep in the first v2.0.0 increment: the 50 ms LVGL timer, audio, BLE timing, tickless idle, wake inputs, and power locks need separate evidence. Do not promise battery life; measure 30 minutes of lobby wait and 60 minutes of continuous play, or mark power unverified.

## 11. Host tests and fault injection

Pure-C model tests cover both first-player orders, all regular and sudden-death boundaries, all 36 target pairs, timing/power boundaries, both random thresholds, exactly-once result application, 1,000 tied sudden-death pairs, bounded memory, and matching Host/Guest digests.

Protocol/session tests cover golden vectors, illegal length/enum/version, missing/duplicate/reordered fragments, sequence wrap boundaries, unknown messages, and loss/duplicate/reorder/queue-full/disconnect/reconnect/security rejection at every state. Run at least 10,000 randomized messages; the pair must converge or explicitly abort, never continue with divergent scores. Fuzz the fixed-buffer parser under ASan/UBSan.

All existing model, scene, audio, i18n, preference, and preview tests remain green. Shell contract tests continue to prove UP-long to Factory, OK-long to cover/home, and no startup mark-valid. Add a contract proving single-player entry does not initialize NimBLE and multiplayer exit leaves no live BLE task/callback. After implementation run `./tools/validate.sh --static`, `--firmware`, and the complete gate.

## 12. Two-device acceptance

With explicit flash approval and the identical candidate on both devices, verify:

1. A creates/B joins and the reverse; multiple lobbies, wrong selection, cancellation, comparison accept/reject, 60-second timeout, and incompatible protocol.
2. Creator-triggered random coin outcomes, all difficulties and targets, simultaneous blind choices, weak/high/timeout, exact/same-column/wrong-column saves, early finish, and sudden death.
3. Forced disconnect at round open, one commit, both commits, one reveal, both reveals, result commit, animation, handoff, and summary; no choice leak/change, reroll, or duplicate score during recovery.
4. Recovery timeout, peer reboot, third-device takeover attempt, and explicit re-pair; old-session traffic cannot enter a new match.
5. Log input-to-ACK, final-action-to-animation, start skew, RSSI, and disconnect reason. Test close range, 3–5 m line of sight, and body obstruction without claiming a range in advance.
6. Fifty multiplayer enter/exits, fifty reconnects, twenty complete matches, and at least 60 minutes continuous operation while recording heap, largest block, task high-water, queue peak, watchdog, and reset reason.
7. No visible audio/display/input/battery stutter. After BLE exit, complete each of the three v1.5.0 single-player modes unchanged.
8. UP long returns to Launcher from discovery, security, match, reconnect, and summary. OK long returns only to the Penalty cover, and startup does not mark valid.
9. An ordinary scanner cannot write match characteristics before authentication. Malformed, oversized, old-epoch, and wrong-role messages are rejected without crash or secret leakage.

## 13. Milestones

- **M0 — Product and protocol freeze:** Resolve section 14, freeze flow, duel table, GATT UUID/schema, timeout, and acceptance thresholds. Documentation only.
- **M1 — Pure-C duel and protocol:** Add duel model, snapshots, codec, sequence/idempotency, and failure simulation using test-first development. Preserve v1.5.0 tests.
- **M2 — Minimal secure two-device link:** Add Central/Observer, connectable Peripheral, GATT, Numeric Comparison, version negotiation, one connection, and clean stop. A hidden `BLE LINK TEST` page activates the link only after Create/Join selection and exposes the two-device encrypted ping/snapshot acceptance path without becoming production UI. Measure size/heap/stack delta and repeated start/stop on devices.
- **M3 — Lobby and one-kick loop:** Second-level menu, Create/Join, lobby code, security confirmation, creator-triggered random coin, difficulty, and one blind commit/reveal/result/animation with each side as shooter.
- **M4 — Full match and recovery:** Five rounds, early finish, sudden death, summary, duplicate/reorder/loss, 30-second reconnect, snapshot restore, exit, and re-pairing; pass randomized failure simulation.
- **M5 — Compatibility and complete gates:** Finish bilingual strings/glyphs/rendering, single-player and Launcher regression, resource tuning, static/firmware/full gates, and authoritative docs without repository changelog edits.
- **M6 — Two-device acceptance:** Flash only after approval, execute section 12, and mark Device tests PASS only after gameplay, recovery, Launcher, resource, and endurance acceptance. Commit, push, and release still require separate authorization.

## 14. Confirmed product decisions

| # | Decision | Confirmed value | Impact |
| --- | --- | --- | --- |
| 1 | Name | **Confirmed: Multiplayer, not combo** | Use one term in docs and UI |
| 2 | Coin trigger | **Confirmed: creator presses OK; outcome remains random** | BLE role does not determine first shooter |
| 3 | Kick interaction | **Confirmed: simultaneous blind choice, no directional cue** | Requires commit/reveal |
| 4 | Match format | **Five kicks each, early finish, paired sudden death** | Inherits the v1.5.0 format |
| 5 | PvP result table | **Shot-on-target probability, then save probability** | Still requires device playtesting for feel |
| 6 | Difficulty | **Host selects, Guest confirms, shared for the match** | Both peers use the same speed and green window |
| 7 | Pairing security | **4-digit lobby + 6-digit Numeric Comparison + SC/MITM** | Six digits are compared, not typed |
| 8 | Disconnect | **30 seconds, then no-winner abandonment** | No forfeit is awarded |
| 9 | Power-cycle recovery | **None in v2.0.0** | No NVS checkpoint |
| 10 | Peer retention | **Temporary per match** | Cleared when the session ends |
| 11 | Menu placement | **Shootout submenu: Single/Create/Join/Back** | Single-player gains one confirmation |
| 12 | Result after reconnect | **Show the committed result once without repeat score/audio** | Preserves exactly-once settlement |
| 13 | First release scope | **No account, ranking, spectator, cloud, or cross-version match** | Extensions require a later protocol version |

All decisions above are locked. M1 implements and tests the duel format, action and random commit/reveal, authoritative snapshots, seq/epoch idempotency, CRC framing, and fixed-buffer fragmentation. M2 implements a bounded NimBLE session with Central/Observer discovery, connectable Peripheral advertising, authenticated GATT, user-confirmed Numeric Comparison, explicit protocol/capability negotiation, encrypted fragmented ping/snapshot paths, single-connection enforcement, sanitized events, and retryable clean stop. The user reported `JOIN / PING PASS` and `SNAPSHOT PASS` on the two-device diagnostic, with MTU 128 and approximately 123 KiB free heap on Guest. This proves that exchange, not gameplay, long-running resource acceptance, or the 160 KiB design target. M3 also fixes the Host diagnostic's static waiting text by exposing received ping and sent snapshot events.

### 14.1 M3 implementation checkpoint

M3 implements the Shootout submenu (Single/Create/Join/Back), up to four compatible nearby lobbies, four-digit lobby codes, displayed six-digit security confirmation, Host-selected/Guest-confirmed difficulty, and creator-triggered random first shooter. Both players independently choose from six targets and time their action. Neither sees an opponent direction cue. After both commitments, Host requests reveals and sends a result; Guest verifies both action commitments, the random commitment, and recomputed settlement before applying any score. Both screens animate the settled ball path and play local cues. After both acknowledge the first result, the second kick swaps shooter. The test session ends after **two kicks, one per player**, without declaring a match winner.

The pure-C coordinator is `penalty_multiplayer_core`; `penalty_multiplayer` owns the worker, input queue, BLE integration, and rendered view. BLE uses a bounded logical-message queue, ACK/fragment handling, and a five-second outbound deadline. An experimental `M3_LOOP_V1` capability prevents matching an M3 lobby to an older diagnostic-only build. Production nonces use the ESP hardware random source; the deterministic RNG is a host-test fallback. Audio and networking remain outside LVGL callbacks. Multiplayer stop pauses the timer and waits for worker/BLE/audio shutdown before UI destruction. Single-player does not initialize NimBLE.

The historical M3 candidate aborts without a winner on connection failure and offers a new connection. It does **not** resume an interrupted round. M4 extends this candidate as described below. Clock-synchronized animation, resource/endurance acceptance, and the full device failure-injection matrix remain M5–M6 work. The approximately 180 ms relative animation delay is not measured cross-device synchronization.

For the M3 device check, use the same approved candidate on both devices. Open Shootout → Create on one and Shootout → Join on the other; select the matching lobby code. Compare the six-digit values on both screens, then press OK to accept or DOWN to reject. Host selects difficulty with UP/DOWN and confirms with OK; Guest confirms with OK. Host presses OK to toss the coin. Each player uses UP/DOWN to choose a target, OK to start the meter, and OK again to lock; a four-second meter timeout commits zero timing. After the first result, both press OK to swap roles. After the second result, hold OK to return to the cover. Repeat with the devices' creation roles reversed. Physical rendering, sounds, actual score convergence, both random first shooters, pairing rejection, clean exits, and runtime resources are still board checks.

The user confirmed the M3 two-device lobby/security/gameplay/role-swap main path as normal on 2026-10-04. This is user-observed main-path acceptance, not instrumented heap/stack, rejection, endurance, or recovery acceptance.

### 14.2 M4 implementation checkpoint

The current coordinator runs five kicks each, early finish, and paired sudden death until a winner exists. Both peers must acknowledge each nonfinal result before the next kick. The final page shows the local winner/loser, converged score, and regular/sudden-death stage; OK starts a new lobby. No difficulty, input timing, probability, or audio behavior changes in single-player.

M4 requires the experimental `M4_MATCH_V1`, duel, commit/reveal, and reconnect capabilities. It cannot match an M2 diagnostic-only or M3 two-kick lobby. Encrypted Protocol Info adds a two-byte Host-issued connection epoch after its existing 20 bytes. Epoch advances on each authenticated physical reconnection; seq restarts for that epoch, old-epoch frames are discarded, and late GATT procedure callbacks carry a connection-generation guard. Match ID, peer BLE identity, and peer boot instance remain pinned. Rebooted or third devices cannot take over this match.

Each direction retains one immutable unacknowledged logical message. Its application ACK is emitted only after the multiplayer core accepts it. ACK-only frames use seq zero and are not themselves acknowledged. Retry interval is 300 ms, with at most three retries. A retry keeps epoch/seq/payload unchanged and uses a fresh fragmentation ID; fragment completion is not application acceptance. Exhaustion or a transport gap terminates the physical connection and starts recovery. Physical GATT operations retain their five-second bound. BLE diagnostics retain their separate M2 codec and path.

On disconnect, input and settlement freeze for at most 30 seconds. Repeated disconnects do not extend the coordinator's original deadline. Uncommitted selection/charge resets to aim. Locked actions, nonces, commitments, and Host random material remain immutable. Original peers automatically reconnect using their authenticated, RAM-only per-session bond; no match checkpoint or bond is written to NVS. Exiting stops the session and removes only this peer's temporary store records. It does not erase NVS or unrelated records.

Recovery is Host SNAPSHOT → Guest STATE_ACK → Host RESUME. Guest checks bounds, match/round identity, counts, commitments, and any missed settled result before acknowledging. Neither side accepts new input before its resume state is established. An interrupted animation resumes at its committed result, without replaying score or audio. An elapsed recovery window or explicit cancellation ends with no winner; it never awards a forfeit. DOWN cancels from the pause screen; OK LONG/UP LONG keep their cover/Launcher precedence. The exit worker attempts a bounded cancellation notice before teardown. A completed, mutually acknowledged summary is retained if the link subsequently drops. No reboot recovery is implemented.

The recovery SNAPSHOT is a fixed 193-byte little-endian payload, schema 1 under `M4_MATCH_V1`:

| Offset | Content |
| --- | --- |
| 0–19 | Full match ID and round ID |
| 20–27 | Schema, normalized resume phase, difficulty, first/current shooter, flags, duel phase, match result |
| 28–51 | Settlement count, attempts, goals, sudden-death round |
| 52–59 | Both meters and commitment/reveal masks |
| 60–155 | Both action commitments and Host random commitment |
| 156–172 | Settled random reveal, outcome/flags, result ID |
| 173–192 | Both actions, only after settlement; all zero before settlement |

Host checks include 180 full matches across both first shooters and three difficulties, including early finish and sudden death. The encoded/fragmented fault simulation delivers 16,869 logical messages in 27,211 transmissions, injects 10,342 message/ACK losses and 27,223 duplicate arrivals, and rejects 22,046 old-epoch arrivals. It also covers lost first/next openings, locked/unlocked actions, missed results, repeated disconnects, 30-second timeout, cancellation/re-pairing, and malformed snapshots. Address/undefined-behavior sanitizers pass. Actual LVGL tests pass for both languages, all required glyphs, summary/recovery/no-contest pages, blind keeper, and 50 create/destroy cycles in the unchanged 24 KiB pool; single-player preview also passes. These tests do not exercise the physical NimBLE radio, bond reencryption, or device rendering.

After approval to install M4 on both devices, repeat the normal lobby/security flow and play through a full match and paired sudden death. Test brief radio interruption while aiming, after one lock, after both locks, and around the result; compare scores and verify that a locked action cannot change. Keep both devices powered: switching one off tests reboot rejection, not recovery. Test an interruption longer than 30 seconds, pause-screen cancellation, cover/Launcher exits, a fresh re-pair, and rejection of a different peer. Runtime heap/stack, physical rendering/audio, animation skew, 50 reconnects, and 20-match endurance remain device checks.

### 14.3 Controlled-disconnect development variant

On 2026-10-05 the user could not trigger a BLE disconnect at about 5 m through two walls. This is a non-triggered recovery test, not recovery acceptance or a measured RF range. The user approved a development-only controlled-disconnect variant; this approval does not authorize flashing or publication.

`CONFIG_PENALTY_DEV_BLE_FAULTS` defaults to disabled. Build the opt-in variant with `./tools/validate.sh --all --dev-ble-faults`; its overlay and separate delivery directory are described in the [build guide](../../../docs/development/engineering/build-and-test.md). Both normal and development builds retain the existing M4 protocol, RAM-only bond, original-peer/boot identity, and unchanged 30-second recovery deadline.

With an authenticated match on both powered devices, open the panel by holding DOWN while aiming, waiting for the other action, or on a nonfinal result. The panel says `DEV BLE TEST`: click OK for 10 seconds, click DOWN for 35 seconds, or press UP to close without disconnecting. DOWN short presses in these idle phases are applied on CLICK in this variant, so opening the panel never changes the target. Charge, coin, pairing, recovery, failed and completed phases cannot open it. Closing or leaving an eligible phase clears pending panel clicks. OK long still returns to the cover and UP long still follows the existing Launcher contract; they do not intentionally inject a disconnect.

The multiplayer worker queues a request; the BLE worker rechecks M4, pinned authenticated peer, live connection, current connection generation and unfinished match. It terminates the real GAP connection, rather than synthesizing a game-disconnect event. Only the actual disconnect callback starts the 10/35-second discovery hold. The initiating device suppresses rediscovery and queued advertisement/connection attempts; the peer follows its normal recovery path. Match IDs, commitments, nonces and random material are not edited. The 10-second hold then resumes ordinary authenticated discovery and SNAPSHOT/STATE_ACK/RESUME. In the 35-second test, the existing 30-second deadline ends the match with no winner before rediscovery can resume. Repeated disconnect callbacks cannot extend the hold. Failure, match end, stop and new session clear the hold; no delayed timer survives page teardown. No settings or bond data is erased or persisted by the hook.

Host validation covers allowed durations, the actual-disconnect clock, duplicate callbacks, lifecycle reset, both initiating roles, preservation of a locked action/random material/match identity at 10 seconds, and both peers' no-contest timeout at 35 seconds. Existing 180-match fault simulation and ASan/UBSan pass. Actual LVGL panel glyph/layout checks and 50 enter/exit cycles pass in the existing 24 KiB pool in both languages; the development labels use ASCII already covered by the selected fonts. These are host checks, not physical GAP/bond or RF-loss acceptance.

After separate app-only flash approvals, first test 10 seconds while one action is locked: keep both devices powered and close together; open the panel on one device, click OK, and verify a visible recovery phase, automatic return without a new coin toss, unchanged locked action and matching scores with no duplicate scoring. Repeat with the opposite device initiating. In a fresh match click DOWN for 35 seconds and check that both show no-contest after approximately 30 seconds, not a forfeit or resumed match. Then start a new lobby and test clean re-pairing and normal full-match behavior. Natural radio-loss/supervision timeout, recovery around every settlement boundary, physical UI/audio/input, runtime resources and endurance remain separate tests.

### 14.4 M5 compatibility and resource observability

M5 retains all three single-player modes, portrait controls, language-only persistence and the existing Launcher contract. No BSP, partition, dependency-lock, network protocol or match-rule change is required. Single-player entry does not start NimBLE. The preview wrapper runs both the single-player and multiplayer executables; firmware/full gates require their actual LVGL render checks and a size report before archiving. Multiplayer coverage includes both languages, actual selected-font glyphs, lobby/security/difficulty/coin, locked waiting, recovery, win/loss/no-contest summaries, blind keeper behavior, rounded corners, dirty/full redraw equivalence and 50 page lifecycles in the unchanged 24 KiB pool. Desktop framebuffers remain test-only.

BLE and multiplayer workers sample free heap/largest block once per second, outside button/GAP callbacks. BLE teardown records before/ready/after heap, sampled minima, BLE worker/host stack high-water values and observed internal/public/send queue peaks. Multiplayer teardown records entry/exit heap, sampled minima, boot-lifetime minimum heap, worker stack, observed input queue peak/drop count/max age and post-page LVGL free/largest block. ESP-IDF stack values are bytes, not words. Sampled minima/queue peaks can miss transient extrema; boot minimum does not reset per lobby, and an immediate teardown heap may precede idle-task reclamation. Compare stable warm-up measurements during M6, not single instantaneous values. Logs contain no match secrets or key material. Existing queue capacities, 6 KiB BLE/multiplayer worker stacks, 24 KiB LVGL pool and partial draw buffer are not reduced without physical headroom evidence. Compile-time and host tests bound fixed model/core/message state; they do not measure device heap. Physical resource/latency/security/endurance acceptance remains M6 work.

## 15. Current validation

```text
Build: PASS (ESP-IDF 5.5.3; complete gate; exact final candidate identity recorded below)
Host tests: PASS (complete gate; controlled 10s/35s policies and both initiating roles; 180 full matches and 27,112 encoded/fragmented fault-simulation transmissions; ASan/UBSan; bilingual actual LVGL panel glyph/layout checks and 50 enter/exit cycles in 24 KiB; single-player regression)
Device tests: both-device M5 development installation/verification/startup and user-reported final-outcome agreement with original and reversed Create/Join roles plus three-mode single-player controls/audio/OK-long cover return and controlled 10s automatic recovery initiated separately by each device without a new coin toss, altered locked action or duplicate scoring, plus reported 30s no-contest termination without forfeit or resuming the old match, plus post-timeout fresh-lobby pairing, zeroed score/no old locked action and per-kick/final-outcome agreement in a new full match PASS; earlier M4 development installation/verification/startup and user-observed 10s recovery/30s no-contest termination PASS; previous non-hook M4 full-match score/winner convergence PASS; remaining M6 acceptance NOT RUN
Device tests (resources): PASS for ten recorded teardowns on each device; second-device cycle 6 includes a BLE restart, not ten uninterrupted ready sessions; not complete resource-budget/endurance acceptance
Unverified: opposite-device long-disconnect initiation; settlement-boundary recovery; natural RF-loss/supervision timeout; all first-shooter/difficulty/early-finish/sudden-death variants; cancellation, instrumented bond reencryption, peer rejection, physical display, multiplayer input/audio and Launcher return; timing/animation skew; worst-case resources and connected-idle heap target; peer-exit/retry sequence; remaining M5–M6 resource, compatibility, failure and endurance acceptance
```

This design and the M1–M4 implementation are based on the v1.5.0 product behavior, the current `feature/penalty-ble-v2` branch, repository hardware/lifecycle guidance, and ESP-IDF 5.5.3 NimBLE connection/security capabilities. The working tree is an experimental milestone, not a published v2.0.0 release. Passing the build and host gate does not establish M4 device acceptance or full-match usability.

Final M3 candidate (2026-10-03): app **1,882,832 bytes**, merged image **1,948,368 bytes**. Verified archive: `build/firmware/e61002aed9c40e570742870bdd4b546322dcfc5d69b47a38a67832c597657bbf/`. Full-image SHA256: `e61002aed9c40e570742870bdd4b546322dcfc5d69b47a38a67832c597657bbf`; matching ELF SHA256: `2f203a24b3b00a74bdc35c4254e0d346de84c86370dea6bfeeb08c6bd02de2eb`. Build identity: `penalty-v1.1-8-g3ba1f63-dirty`; the compatibility filename still derives from unchanged `penalty/VERSION` (`1.5.0`) and is not the milestone identity. The first M3 device flash is recorded below. No commit, push, or release has been performed.

The app fits both previously confirmed factory ranges (`0x10000` with 3 MiB or `0x7f0000` capacity). On either device, recheck its identity and partition layout, then obtain approval to write **only the application at `0x10000`**, preserving its partition table, bootloader, NVS, and image/card/audio resources. Do not write the merged factory-layout image over a device's custom layout. Only one USB modem port was present at handoff; the two devices can be flashed in sequence after approval.

First-device flash (2026-10-03, explicitly approved): USB identity ending `72:60`, `/dev/cu.usbmodem101`, ESP32-C3 with 8 MiB flash. The current factory partition was confirmed at `0x10000`, size `0x7f0000`. Only the approved app was written; app SHA256 `76da684a2a004b1ff6be17a9d17ea4961cec527356e0de87d2f648b2bc820ea3`, erase range `0x10000–0x1dbfff`. Both write-time digest and a separate `verify_flash` passed. The full 4 KiB partition-table sector was byte-identical before/after. No bootloader, NVS, or resource range was written. A bounded 20-second startup observation showed the candidate's matching ELF identity, normal display/audio/button initialization, and no panic or reset loop. Serial monitoring is closed. This is installation/startup evidence only: visible menu, physical audio/input, multiplayer resources and two-device gameplay remain unverified. The second device is not yet updated to M3.

Second-device flash (2026-10-04, explicitly approved): USB identity ending `cb:bc`, `/dev/cu.usbmodem101`, ESP32-C3 with 8 MiB flash. The actual factory partition was confirmed at `0x10000`, size `0x300000`; image/card/audio partitions remain in the device's custom layout. The same archived M3 app as device one was written only at `0x10000`, with erase range `0x10000–0x1dbfff`. Write-time digest and independent `verify_flash` both passed. The full partition-table sector was byte-identical before/after; no bootloader, NVS or resource range was written. A 20-second startup observation confirmed ELF prefix `2f203a24b`, normal display/audio/button initialization and no panic/reset loop. Serial monitoring is closed. Both devices now have the identical M3 candidate installed. Two-device lobby/security/gameplay/role-swap acceptance, physical display/audio and multiplayer resource measurements remain NOT RUN.

Final M4 candidate (2026-10-04): app **1,893,024 bytes**, merged image **1,958,560 bytes**. Complete ESP-IDF 5.5.3 gate and independent archive verification passed. Archive: `build/firmware/cb69c4a83f42d00c1418cc7524d91af76192dd6474135173e7c871d7c29e825f/`. Full-image SHA256: `cb69c4a83f42d00c1418cc7524d91af76192dd6474135173e7c871d7c29e825f`. App SHA256: `b0d383bc00815b6c5036bfa1eb77dfd245865e6f1d595a9827ffc030d4816e7b`. Matching ELF SHA256: `87929982eb2246a03566b7c9a1b0116e6ae01cc46c0e4b44d3d2e334579fc6ea`. Build identity remains `penalty-v1.1-8-g3ba1f63-dirty`, with the unchanged `1.5.0` compatibility filename; the hashes distinguish M4 from M3. The M4 app fits both previously confirmed factory capacities. Reconfirm the target and layout before any approved app-only write at `0x10000`. A merged image at `0x0` would replace the layout and can reset stored data; it must not be used over either existing device layout. Both M4 device flashes are recorded below; both devices now have the identical M4 candidate installed. No commit, push, or publication has been performed. M3 user-observed acceptance above supersedes the installation-time pending main-path status; instrumented resource and endurance checks remain unverified.

First-device M4 flash (2026-10-04, explicitly approved): USB identity ending `72:60`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x7f0000`. Only the archived M4 app above was written; app SHA256 `b0d383bc00815b6c5036bfa1eb77dfd245865e6f1d595a9827ffc030d4816e7b`, erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector was byte-identical before/after; no bootloader, NVS, or resource range was written. A bounded 20-second startup observation confirmed ELF prefix `87929982e`, normal display/audio/button initialization, and no panic or reset loop. Serial monitoring is closed. This establishes installation/startup only, not physical UI/input/audio or multiplayer acceptance. At this first-device handoff, the second device was still on M3; the subsequent M4 installation is recorded below.

Second-device M4 flash (2026-10-04, explicitly approved): USB identity ending `cb:bc`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x300000`, with the existing image/card/audio resource partitions. Only the same archived M4 app as device one was written; app SHA256 `b0d383bc00815b6c5036bfa1eb77dfd245865e6f1d595a9827ffc030d4816e7b`, erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector was byte-identical before/after; no bootloader, NVS, or resource range was written. A bounded 20-second startup observation confirmed ELF prefix `87929982e`, normal display/audio/button initialization, and no panic or reset loop. Serial monitoring is closed. Both devices now have the identical M4 app installed. Physical rendering/input/audio, two-device full-match gameplay, authenticated recovery, cancellation, re-pairing, resource measurements and endurance remain unverified; installation/startup is not M4 gameplay acceptance. No commit, push, or publication has been performed.

M4 user-observed gameplay check (2026-10-04): after both devices received the identical M4 app above, the user was asked to create/join, compare the six-digit security values, have the creator toss the coin, and finish one full match while checking each kick's score and the final winner. The user replied that the results were consistent. Record PASS only for this reported full-match score/winner convergence. No detailed per-kick log, first-shooter/difficulty identity, early-finish/sudden-death coverage, reversed Create/Join run, recovery injection, or runtime measurements were reported. This supersedes the preceding installation-time pending full-match status, not the remaining physical acceptance items.

Controlled-disconnect development candidate (2026-10-05): app **1,894,848 bytes**, merged image **1,960,384 bytes**. Complete development gate and independent archive verification passed. Archive: `build/firmware/ec222e1c3d809c0b73a6d67d30348d799d915b92bf0cb93dddd5f77413e8cf38/`. Full-image SHA256: `ec222e1c3d809c0b73a6d67d30348d799d915b92bf0cb93dddd5f77413e8cf38`; app SHA256: `5fde5419450ed517d5b05a3d5a52f305d977e6986455897dc1a2e2bd9366ad65`; matching ELF SHA256: `e14e9310435c21b880b36017caacff47933c75d21586187d2a19e41964710b5f`. Build identity still uses `penalty-v1.1-8-g3ba1f63-dirty`; use these hashes and the `-dev-ble-faults` delivery suffix to distinguish the variant, not the unchanged `1.5.0` compatibility filename. This app fits both existing factory capacities. If later authorized, recheck each target/layout and write this app only at `0x10000` (sector-rounded range `0x10000–0x1defff`); do not write its merged image over either existing layout. Both development flashes are recorded below; both devices now have the identical controlled-disconnect development app installed. No commit, push or publication has been performed for this variant.

Normal-build regression (2026-10-05): the complete non-hook gate also passed; app **1,893,024 bytes**, merged image **1,958,560 bytes**. Verified archive: `build/firmware/bbcd57adda20455291f0ace8f4da09263d28427e55508cf832863a12894eb318/`; matching ELF SHA256 `efe366d81724f85691b22bf9571906eb2de5a5bab43dd5513779ace0cb7197cf`. The gate checked that the development option was disabled. An independent application-binary check confirmed the `DEV BLE TEST` marker exists in the development app and not the normal app. No USB Passport target was detected at the final read-only handoff; connect and separately authorize each device before writing.

First-device controlled-disconnect development flash (2026-10-05, explicitly approved): USB identity ending `72:60`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x7f0000`. Only the archived development app above was written: **1,894,848 bytes**, app SHA256 `5fde5419450ed517d5b05a3d5a52f305d977e6986455897dc1a2e2bd9366ad65`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector was byte-identical before/after; no bootloader, NVS or resource range was written. A bounded 20-second startup observation confirmed ELF prefix `e14e93104`, normal display/audio/button initialization and no panic or reset loop. Serial monitoring is closed. This establishes installation/startup only: the physical development panel, actual 10/35-second GAP disconnect/recovery and authenticated bond reencryption remain unverified. At this first-device handoff, the second device remained on the earlier non-hook M4 app; its subsequent development flash is recorded below. Prior full-match acceptance does not establish acceptance of this development variant.

Second-device controlled-disconnect development flash (2026-10-05, explicitly approved): USB identity ending `cb:bc`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x300000`, with the existing image/card/audio resource partitions. Only the same archived development app as device one was written: **1,894,848 bytes**, app SHA256 `5fde5419450ed517d5b05a3d5a52f305d977e6986455897dc1a2e2bd9366ad65`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector was byte-identical before/after; no bootloader, NVS or resource range was written. A bounded 20-second startup observation confirmed ELF prefix `e14e93104`, normal display/audio/button initialization and no panic or reset loop. Serial monitoring is closed. Both devices now have the identical development app installed. This establishes installation/startup only: the physical development panel, actual 10/35-second GAP disconnect/recovery, authenticated bond reencryption, cancellation and re-pairing remain unverified. No commit, push or publication has been performed.

Controlled-disconnect user-observed acceptance (2026-10-05): after both devices received the identical development app above, the user reported that the 10-second test recovered normally as expected, and the 30-second timeout test ended with no winner as expected. Record PASS for these two reported behaviors only. The development panel offers a 35-second discovery hold to exercise the existing 30-second match deadline; the user's "30-second test" describes the reported timeout outcome, not a new 30-second injection option. The initiating device/role, selected long-hold duration, measured timing, locked-action preservation, subsequent per-kick score convergence and exactly-once settlement were not separately reported. Opposite-device initiation, fresh-lobby re-pairing after timeout, cancellation, natural RF-loss and instrumented security/resource/endurance checks remain unverified. This supersedes the earlier installation-time pending status for the two observed behaviors; it does not establish complete M4 acceptance. No firmware change, flash, commit, push or publication was performed for this acceptance record.

M5 local completion (2026-10-05): normal and controlled-disconnect development complete gates, separate archive verification, fixed-state budgets, single-player/Launcher lifecycle contracts, and ASan/UBSan single-player plus 180-match recovery regressions passed. Both actual LVGL renderers passed 50 entry/exit cycles in the 24 KiB pool, with 12,248 bytes free after exit following warm-up; bilingual captures were visually reviewed. Host state sizes are model 328, duel 240, multiplayer core 1,280 and message 252 bytes; target compilation also enforces a 2 KiB multiplayer-core limit. These are not device heap measurements. The normal size report uses 141,588 bytes of DRAM (34,872 BSS + 10,668 data + 96,048 RAM text); the development report uses 141,604 bytes (34,888 BSS). Linker remaining RAM is not runtime free heap. No BSP, dependency-lock, partition or repository changelog change was made.

M5 normal candidate: app **1,894,336 bytes**, merged **1,959,872 bytes**; archive/full SHA256 `3d0efd73eb8c538cf4d20dc925f7ca63cb535691d056c89d156211bdf6bcf48a`; app SHA256 `be74bfed846664f627586724c60ba67dacaa475d327fc2ebb0e86f06bb260ffe`; matching ELF SHA256 `9580995e5cef2c96b9dfb2e58ee0a2fa71b3df23ccc175c55b61a2bcf079e470`. M5 development candidate: app **1,896,144 bytes**, merged **1,961,680 bytes**; archive/full SHA256 `6a535cad72bd973bc76e4cef57f20b0bbb4ac2be62686f2b9b5743cfa6459a74`; app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`; matching ELF SHA256 `9546517500ebfb3ebce03622ff1d2c4db7285fca4ad560b5e933a2f1e473648c`. Each immutable bundle is at `build/firmware/<full-sha256>/`; normal and development delivery copies remain separate. The `DEV BLE TEST` marker is absent from the normal app and present in the development app. Both apps fit the smaller existing 3 MiB factory partition. The `1.5.0` compatibility filename and dirty build descriptor remain unchanged; use hashes for candidate identity. Both devices now have the identical M5 development app, as recorded below; the normal candidate remains unflashed. Earlier reported full-match/recovery/no-contest results do not validate M5 telemetry or M6 endurance. No USB target was detected at the initial M5 build handoff. For later separately approved flashes, recheck the exact target/layout and write only its archived app at `0x10000`; do not write a merged image over either existing layout. No commit, push or publication was performed.

First-device M5 development flash (2026-10-05, explicitly approved): USB identity ending `72:60`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition read confirmed factory offset `0x10000`, size `0x7f0000`. Only the archived development app above was written: **1,896,144 bytes**, app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector remained byte-identical; no bootloader, NVS or resource range was written. A bounded 20-second startup check confirmed ELF prefix `954651750`, normal display/audio/button initialization, and no panic or reset loop. Serial monitoring is closed. This establishes installation/startup only; M5 multiplayer resource logs, physical controls/audio, two-device gameplay and M6 endurance remain unverified. At the first-device handoff, the second device still had the earlier M4 development app; its subsequent M5 flash is recorded below. Prior M4 10-second recovery/30-second no-contest observations do not validate the new M5 candidate. No commit, push or publication was performed.

Second-device M5 development flash (2026-10-05, explicitly approved): USB identity ending `cb:bc`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition read confirmed factory offset `0x10000`, size `0x300000`, plus the existing image/card/audio resource partitions. Only the same archived development app as device one was written: **1,896,144 bytes**, app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` both passed. The full 4 KiB partition-table sector remained byte-identical; no bootloader, NVS or resource range was written. A bounded 20-second startup check confirmed ELF prefix `954651750`, normal display/audio/button initialization, and no panic or reset loop. Serial monitoring is closed. Both devices now have the identical M5 development app. This establishes installation/startup only; physical single-player and multiplayer behavior, resource telemetry, Launcher/input/audio, security and M6 endurance remain unverified. Earlier M4 observations are not M5 acceptance. No commit, push or publication was performed.

M5 user-observed match check (2026-10-05): after both devices received the identical M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`), the user reported matching final win/loss outcomes. Record PASS for the reported final-outcome agreement in this match only. The user did not separately confirm per-kick score agreement, first-shooter/difficulty, early-finish/sudden-death coverage, reversed Create/Join roles, M5 disconnect injection/recovery, single-player/Launcher/input/audio compatibility or runtime resource measurements. This supersedes the installation-time pending match status only for the reported outcome; it does not establish complete M5–M6 device acceptance. The earlier M4 recovery/no-contest reports remain historical evidence for their own candidate. No firmware change, flash, commit, push or publication was performed for this acceptance record.

M5 reversed-role user-observed match check (2026-10-05): the user subsequently reported consistent results after swapping the devices' Create/Join roles. Both devices retain the M5 development app identified above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`). Record PASS for reported final-outcome agreement with reversed roles. This supersedes the previous pending status for that role-order check; it does not establish every first-shooter/difficulty/early-finish/sudden-death variant or security rejection case. Per-kick scores were not separately described. Current-candidate disconnect recovery, single-player/Launcher/input/audio compatibility, resource telemetry and endurance remain unverified. No firmware change, flash, commit, push or publication was performed.

M5 user-observed single-player compatibility check (2026-10-05): the user was asked to leave multiplayer, test Shoot Practice, Keeper Practice and single-player Shootout, and confirm controls, audio and OK-long return to the Penalty cover. The user confirmed normal operation. Record PASS for these reported checks on the unchanged M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`). The device identity and per-device execution/logs were not separately provided. This supersedes the earlier pending single-player controls/audio/cover-return checks only within that reported scope. It does not validate UP-long Launcher return, all gameplay variants, multiplayer input/audio, current-candidate recovery, resource telemetry or endurance. No firmware change, flash, commit, push or publication was performed.

M5 user-observed controlled 10-second recovery (2026-10-05): the user was asked to lock one action, open `DEV BLE TEST` with DOWN long, select the 10-second disconnect with OK, and check recovery/state/scoring. The user confirmed automatic recovery, no new coin toss, unchanged locked action and no duplicate scoring. Record PASS for these four reported checks on the unchanged M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`). Initiating device/role, measured timing, repeated trials, and recovery at every settlement boundary were not separately provided. This supersedes the earlier pending M5 short-recovery and locked-action/exactly-once checks within the reported trial only. It does not establish the M5 30-second no-contest deadline, opposite-device initiation, timeout re-pairing, natural RF loss, instrumented bond reencryption, resource telemetry or endurance. No firmware change, flash, commit, push or publication was performed.

M5 opposite-device controlled 10-second recovery (2026-10-05): the user was asked to initiate the same 10-second disconnect from the other device and verify automatic recovery, no new coin toss, unchanged locked action and no duplicate scoring. The user reported PASS. Both physical devices have now separately initiated the reported short-recovery test on the unchanged M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`); record these four outcomes as user-observed PASS for each initiation. This supersedes the earlier pending opposite-device short-disconnect check. Device-to-role order, measured timing, repeated trials and recovery at every settlement boundary were not independently supplied. The M5 long-disconnect/no-contest deadline, post-timeout re-pairing, natural RF loss, instrumented bond reencryption, resource telemetry and endurance remain unverified. No firmware change, flash, commit, push or publication was performed.

M5 user-observed controlled long-disconnect timeout (2026-10-05): the user was asked to start a fresh match, open `DEV BLE TEST` with DOWN long, click DOWN for the 35-second hold, and check that both devices terminate with no winner after approximately 30 seconds, without a forfeit or automatic resumption of the old match. The user reported PASS. Record these reported outcomes as PASS on the unchanged M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`). The 35-second discovery hold exercises the existing 30-second match deadline; no 30-second injection option was added. The initiating device/role and measured timing were not independently supplied. This supersedes the earlier pending M5 long-disconnect outcome only for the reported trial. Opposite-device long-disconnect initiation, post-timeout fresh-lobby re-pairing, natural RF loss, instrumented security, resource telemetry and endurance remain unverified. No firmware change, flash, commit, push or publication was performed.

M5 user-observed post-timeout fresh-lobby/rematch (2026-10-05): after the reported long-disconnect no-contest, the user was asked to exit, create/join a new lobby, check successful re-pairing, a `0:0` initial score and no retained locked action, then complete another match and verify per-kick score and final-outcome agreement. The user reported PASS. Record all these requested checks as user-observed PASS on the unchanged M5 development app above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`). This supersedes the earlier pending post-timeout re-pairing and per-kick score checks within the reported rematch only. No exact device-to-role order, first-shooter/difficulty/early-finish/sudden-death coverage, repeated-trial counts or instrumented timing/security/resources were supplied. Opposite-device long-disconnect initiation, settlement-boundary/natural-RF-loss coverage, cancellation, physical Launcher return, multiplayer input/audio, security/resource telemetry and endurance remain unverified. No firmware change, flash, commit, push or publication was performed.

### 15.1 First-device M5 resource sampling (2026-10-05)

The user approved passive serial sampling, connected device `4C:11:AE:2F:72:60` at `/dev/cu.usbmodem1101`, and reported completing ten cycles. Instructions were five Create cycles followed by five Join cycles, pairing/security confirmation/coin toss/one kick in each, then OK-long return to the cover on both devices. Resource records independently confirm ten complete multiplayer teardown sequences, not each role or kick. Candidate association follows the previously verified M5 installation above (app SHA256 `b9216bef3a12068526ca0410e2a93ecced91c8cd44311c4482253116b1b2eb1f`); this passive capture did not perform another image readback or restart-based ELF check.

Capture stopped automatically after ten complete records and closed the serial port. Total capture time was 1,078.06 seconds, including the initial wait; the first session start through the last exit spanned 485.956 seconds. No ROM boot or monitored fatal/teardown-failure pattern was captured. The filtered collection is not an audit of every warning/error.

| Measurement | Observed bytes unless noted |
| --- | --- |
| Post-exit heap, cycles 2–10 | Exactly 187,828 each; cycle 1 was 187,808 |
| Post-exit largest free block | 106,496 in all ten cycles; cover-entry snapshots agree |
| Post-exit LVGL free / largest | 16,764 / 14,068 in all ten cycles |
| Multiplayer / BLE sampled minimum free heap | 111,588 / 114,816 |
| Sampled minimum largest block | 102,400 |
| Boot-lifetime minimum free heap | 111,452; not a per-lobby statistic |
| Minimum stack high-water: multiplayer / BLE worker / NimBLE host / audio | 2,728 / 2,384 / 904 / 2,220 |
| Observed queue peak / capacity: input, BLE internal, public, send | 1/8, 4/16, 1/8, 1/4 entries |
| Input drops / maximum recorded input age | 0 / 79 ms; not end-to-end latency |

Record **PASS for first-device ten-cycle teardown stability**, with no progressive free-heap/LVGL decline after warm-up in this workload. Do not equate this with complete resource acceptance or leak absence under every workload. All recorded task headroom values exceed the provisional 768-byte target, but the NimBLE host's 904 bytes need worst-case/recovery coverage before stack tuning. BLE-ready heap ranged from 114,860 to 114,952 bytes, below the section 10 connected-idle 160 KB target; these startup snapshots and loaded-session sampled minima do not establish a controlled connected/UI/audio-idle measurement or same-page v1.5.0 delta. Keep that target open rather than silently relaxing it. No stack/buffer resizing or firmware change was made.

Local sanitized evidence: `/tmp/penalty-m5-resource.ZvhejG/`; `cycles.jsonl` SHA256 `2ae1f3297c6928ff1ecb0550b83b6d9c8856260cee3d4260dcf11074f73e938a`, `resources.jsonl` SHA256 `42f4905bc524e3b3b0b12084cfaa2f0974d2fe5cd450478e2dedf60b45aad21c`, `status.json` SHA256 `9492e60fbbd311ac2014d286c4ec445c77f3df1c52221ca2477fe0d4842749c9`. These temporary local files are not a durable release artifact. Second-device sampling, controlled idle/worst-case resources, fifty entry/exits, fifty reconnects, twenty complete matches and remaining M6 acceptance are still pending. No flash, commit, push or publication was performed.

### 15.2 Second-device M5 resource sampling (2026-10-05)

After the user connected device `98:C3:77:F4:CB:BC`, passive capture at `/dev/cu.usbmodem1101` reused the first-device collector with only its USB identity, evidence directory and label changed. No active reset or flash command was sent. An observed startup ELF prefix `954651750` matches the previously verified M5 development installation. Capture ended automatically after ten grouped teardown records in 337.99 seconds; first BLE start to last multiplayer exit spanned 271.869 seconds. Serial is closed, with no ROM boot or monitored fatal/teardown-failure pattern captured. The user reported ten completed rounds under the five-Create/five-Join, one-kick/exit instructions; resource records do not independently prove role/kick completion.

| Measurement | First device `72:60` | Second device `cb:bc` |
| --- | --- | --- |
| Exit free heap, records 2–10 (bytes) | 187,828 each | 187,460 each; first was 187,456 |
| Exit largest block (bytes) | 106,496 each | 106,496 except record 6: 102,400; restored at record 7 |
| Exit LVGL free / largest (bytes) | 16,764 / 14,068 each | 16,764 / 14,068 each |
| Multiplayer sampled minimum heap (bytes) | 111,588 | 111,212 |
| BLE sampled minimum heap (bytes) | 114,816 | 113,408 including restarted record 6 |
| Sampled minimum largest block (bytes) | 102,400 | 98,304 at record 6 |
| Boot-lifetime minimum heap (bytes) | 111,452 | 106,652; not per-lobby |
| Minimum multiplayer / BLE worker / host / in-multiplayer audio stack (bytes) | 2,728 / 2,384 / 904 / 2,220 | 2,728 / 2,392 / 896 / 2,240 |
| Input / internal / public / send queue peaks (entries) | 1 / 4 / 1 / 1 | 2 / 4 / 1 / 1; capacities 8 / 16 / 8 / 4 |
| Input queue overflow drops / maximum input age | 0 / 79 ms | 0 / 221 ms |

Both captures also recorded 936 bytes of audio-worker headroom when leaving the initial cover, outside the multiplayer-cycle audio values above. Input drop counters count queue overflow, not every stale/rejected input. Maximum queue age is not end-to-end response, ACK latency, animation skew or P95 acceptance.

**Count limitation at record 6:** eleven BLE starts and one discarded incomplete BLE grouping occurred within ten multiplayer exit records. At 221.661 seconds the prior ready BLE session stopped; at 221.662 seconds another BLE start replaced the collector's unfinished grouping. The subsequent exit at 223.189 seconds has ready heap zero and public/send peaks zero. Its multiplayer metrics cover the longer page lifetime, while BLE metrics cover only the restarted session. Therefore this record cannot establish another independently ready, completed-kick session. The user confirmed a prompt but was unsure whether the other device exiting first triggered it. Code permits a new BLE session inside the same multiplayer page through OK retry after failure; this is consistent with the sequence, not proof of its cause. Preserve the discarded ready-session metrics in the event file. Do not label the capture ten uninterrupted successful connections or silently omit the restart.

Record **PASS for second-device teardown stability across the ten recorded exits**: warm-up exit heap and LVGL free space remain constant, and the transient largest-block dip recovers. The stable 368-byte free-heap difference between devices is not a within-device progressive leak; its allocation cause was not established. This is not complete resource-budget, exit/retry UX, latency or endurance acceptance. Nonzero BLE-ready heap ranged from 114,484 to 114,588 bytes; the controlled connected/UI/audio-idle 160 KB target remains unverified. The 896-byte host stack headroom exceeds the provisional 768-byte target by only 128 bytes; retain worst-case coverage before tuning. No firmware, stack or buffer changes were made.

Local temporary sanitized evidence: `/tmp/penalty-m5-resource-second.tbMzrj/`; `cycles.jsonl` SHA256 `dbaffe14866b094315fc5b458f6a94a7a663204c3efa65045417e7fe4f5effb5`, `resources.jsonl` SHA256 `c08f2ecc39ea4a9e5bea2b77d201ac934489689da720fe8943bda2dd573fa6af`, `status.json` SHA256 `94eeaeae690fb11041207d9567396d92db9fe2e1e775afc7a22d2261f866e274`. These are not durable release artifacts. Controlled peer-first-exit/retry reproduction, idle/worst-case resources, fifty entry/exits, fifty reconnects, twenty complete matches and remaining M6 acceptance remain open. No flash, commit, push or publication was performed.

### 15.3 Menu and cover input adjustment (2026-10-05)

The user requested increased main/settings menu spacing, a smaller decorative pitch if needed, and bidirectional cyclic navigation. The preceding reported cover retry is fixed by retaining the deferred CLICK timestamp rather than replaying an older physical PRESS. Gameplay timing and stale-input protection remain unchanged. Main-menu row pitch is 32 pixels at y=116; its pitch viewport shrinks from 132 to 90 pixels. Settings row pitch is 40 pixels at y=84. Existing bilingual fonts and bracket selection markers remain. The overlong English settings hint now reads `UP/DOWN: SELECT  OK: SET`. Other pages restore default line spacing. No BLE protocol, match-rule, stack, LVGL pool, draw-buffer, BSP, partition or version-name change was made.

Both normal and development complete gates pass with ESP-IDF 5.5.3, including actual LVGL renderers and size reports; independent archive verification also passes. New model tests cover repeated wrapping in both menus and unchanged settings during navigation. The cover source contract rejects timestamp backdating. ASan/UBSan model tests pass. Bilingual screenshots cover every menu/settings selection with exact row pitch/height, footer separation and non-menu spacing checks. Both renderers pass 50 lifecycles with warmed-up LVGL free space unchanged at 12,248 bytes; this is desktop, not device, memory evidence. See the updated [game design](../game-design.md#portrait-entry) and [current validation](../validation.md#ble-v2-menureadability-adjustment-2026-10-05).

| Candidate | App / merged bytes | App SHA256 | Merged SHA256 / archive ID | Matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,894,400 / 1,959,936 | `e93a86b30f8447381f261f64b121ffe506c2da043307ce91785039e137bef97c` | `334e2043f1bb494323bf177b32a1546f5cc28ec774abf893d821b2caaa0ecae0` | `94b464d5d09c1d50d27df2a8d2763bcdae50e53cad946a1673fb6a2a1b33bcb0` |
| Development fault hooks | 1,896,224 / 1,961,760 | `bb3c72c4ed323e3ef7a8527894a9795b94f2761fdb9fb12f2eddf508c49f73fc` | `252c133de2e2d332a07b9d8249845720dc5ec9f82e5e70435dec1fff4af18521` | `b27a3caf3e819ebfde713dcfeaf9762e7fb0849a4320710610f22f8a553adcdb` |

Archives are under `build/firmware/<merged-sha256>/`. Canonical normal delivery remains `build/FoloToy-AI-Passport-Penalty-v1.5.0-full.bin`; development delivery is under `build/dev-ble-faults/` with the `-dev-ble-faults-full.bin` suffix. The unchanged `1.5.0` filename/dirty descriptor is not a new release identity. Both apps fit the previously verified smaller 3 MiB factory capacity. Only app-only installation at a freshly rechecked factory offset `0x10000` is appropriate for these existing devices; neither merged image is authorized to overwrite their layouts. Do not publish the development image.

```text
Build: PASS (both complete gates and independent archive verification)
Host tests: PASS (static/model, ASan/UBSan model, bilingual actual LVGL/layout/lifecycle)
Device tests: PASS for second-device development-app installation/verification/startup only; physical menu acceptance NOT RUN
Unverified: first short OK, physical menu spacing/selection/wraparound, long OK and Launcher regression; remaining M6 acceptance
```

At the initial build handoff, read-only USB enumeration detected device `cb:bc`; no port was opened or flashed. Earlier physical match/recovery/resource acceptance belongs to its original hashes and is not inherited by this increment.

Second-device menu-adjustment development flash (2026-10-05, explicitly approved): USB identity ending `cb:bc`, `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x300000`, with the existing image/card/audio resource partitions. Independent archive verification passed before writing only the development app above: **1,896,224 bytes**, app SHA256 `bb3c72c4ed323e3ef7a8527894a9795b94f2761fdb9fb12f2eddf508c49f73fc`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` passed. The full 4 KiB partition-table sector remained byte-identical; no bootloader, NVS or resource range was written. A bounded 20-second startup check confirmed matching ELF prefix `b27a3caf3`, normal display/audio/button initialization and no panic or reset loop. Serial monitoring is closed. This establishes installation/startup only; physical first-click, menu readability/selection/wraparound, long-OK and Launcher regression remain pending. First device `72:60` retains the previous M5 development app, so the two installed app hashes now differ. No firmware rebuild, commit, push or publication was performed during flashing.

### 15.4 Title/help cleanup (2026-10-05)

The user requested removal of the title navigation hint and decorative header, increased help spacing and removal of its probability line. The title now hides both labels in English and Chinese; its battery, selected brackets, long-OK hint, 32-pixel row pitch and cyclic navigation remain. The actual cover already hid the decorative header and continues to do so. Help now has three instruction lines at y=84 with 32-pixel row pitch and no percentage line. Settings/help/multiplayer retain their headers and operation hints. Generic panels explicitly restore header/footer visibility and default status spacing. This changes presentation only, not probabilities, timing bands, button routing or BLE.

Both complete ESP-IDF 5.5.3 gates and independent archive verification pass. Localization tests reject help percentage copy and require three lines. Actual LVGL checks cover hidden title/cover labels, retained settings/help hints, bilingual row pitch/height/width and dirty/full redraw equivalence. The new title-header absence assertion failed on the prior implementation and passes after the UI change. Both renderers pass 50 lifecycles with warmed-up LVGL free space unchanged at 12,248 bytes. No larger pool, draw buffer or stack was introduced. These are desktop tests, not device/endurance acceptance.

| Candidate | App / merged bytes | App SHA256 | Merged SHA256 / archive ID | Matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,894,400 / 1,959,936 | `7b58a3b401169b5bc6f276b8ea235ad9a89d466801a8e432a3c154fa112e833f` | `be7c1bcbdfe294ab5e643dd08d1d89b6b249dc6ce63f638ff5f7567538cd03ee` | `b0ed0355edaa182be148c56de70ec9b4c737344541c272493e875d2c3a3892e2` |
| Development fault hooks | 1,896,224 / 1,961,760 | `f4fff343fe9980b6a90918206f923ef030a47f9d4da810e9d6c8464ce62955f4` | `358c513f9b0c74280685626269c66f7e55c583ec07bdcd5da1f40f1a7d4d1359` | `d45b556053a3b77e937133c205c6e48cf924f68cb951cfca420fcb36f8c5369d` |

Archives remain under `build/firmware/<merged-sha256>/`; normal and development delivery paths stay separate as above. The unchanged compatibility filename/dirty descriptor is not a release identity. Both apps fit the existing smaller 3 MiB factory partition. For separately approved installation, freshly recheck device identity/layout and write only the archived app at its confirmed factory start, not a merged image over existing resources. Do not publish the development variant.

```text
Build: PASS (both complete gates and independent archive verification)
Host tests: PASS (static/localization, bilingual actual LVGL/layout/lifecycle)
Device tests: NOT RUN for this cleanup
Unverified: physical title/help display and transitions; remaining M6 acceptance
```

Read-only USB enumeration detected second device `cb:bc`; no port was opened during this cleanup. It retains the menu-adjustment app from section 15.3, while first device `72:60` retains the earlier M5 app. Neither installed image equals this new candidate. No flash, commit, push or publication was performed for this cleanup.

### 15.5 Player-size normalization (2026-10-05)

The user reported that blue was much larger than red. Both source canvases are 88 × 88, but ready-body alpha ≥ 128 bounds are red 34 × 65 and blue 43 × 78. All nine blue frames now draw at 213/256 (83.2%), without antialiasing, around local pivot (83,52), the stationary ball's world coordinate (131,147). Red, keeper/background/ball art, original PNGs/generated Flash assets, pure-C scene/model and BLE code are unchanged. The scaled canvas remains within the existing dirty region. LVGL uses bounded RGB565A8 transform strips of at most 1,920 bytes; the 24 KiB pool, display buffer and task stacks are unchanged.

The host-only reference transform independently requires this scale/pivot and checks fully opaque foreground pixels; red retains native-size checks. The regression fails on the prior blue renderer. Coverage asserts all nine poses, ready width 36 ± 1 and height 65 ± 1 pixels, alignment with red's top/ground line, and transformed-canvas containment. Actual LVGL previews check dirty/full redraw equality, six cue poses, contact/follow-through, both languages, blind multiplayer keeper and 50 entry/exit cycles per renderer. Visual inspection of desktop ready/contact captures shows comparable body size. Both complete ESP-IDF 5.5.3 gates and independent archive verification pass. Warmed-up free LVGL memory remains 12,248 bytes; maximum moving-frame flush remains 27,640 pixels. Device acceptance remains NOT RUN.

| Candidate | App / merged bytes | App SHA256 | Merged SHA256 / archive ID | Matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,894,496 / 1,960,032 | `e3c46e6d467ada9b5019bdbbeb64f5f2766ef430e6c00eb2243a8998c53179ed` | `d31bd45ba503db2f9090b580536870d02a6c06f009660e0a9e4b5febc4b69582` | `980c5fb2ed01f4047bdd474f047e806873fb16cb400882d1ed90f539a2c0ee81` |
| Development fault hooks | 1,896,320 / 1,961,856 | `4440dc24af48a1657512939767bdf6794dab35d1835d60641942f6ddea746c99` | `487cdd862ab5c5405a549c21c413ab98b2ad6c4e53e9d5aafc81b1dd142b7d4d` | `4ba452f2617744e00a6bcf0572a04a138af5d2e562f51455dd1e2e146ef84dea` |

Verified bundles are under `build/firmware/<merged-sha256>/`. Matching descriptor: `penalty-v1.1-9-g49947c1-dirty`; compatibility filenames remain unchanged and are not release identities. Both apps fit the smaller existing 3 MiB factory partition. Separately authorized installation must recheck the attached target/layout and write only the archived app at its confirmed factory offset, preserving other partitions. Do not write the merged image at `0x0` over existing resources or publish the development variant.

```text
Build: PASS (both complete gates and independent archive verification)
Host tests: PASS (static/model/art; actual LVGL scale/pivot/9-pose/dirty/lifecycle)
Device tests: PASS for second-device app installation/verification/bounded startup and user-reported red/blue size balance plus natural contact/follow-through
Unverified: first-device update/acceptance, exhaustive nine-pose/six-cue physical coverage, measured frame latency and outstanding M6 acceptance
```

At the original build handoff, no flash, commit, push or publication was performed, and physical sprite acceptance was pending. Subsequent installation and user-observed acceptance are recorded below.

Second-device player-size development flash (2026-10-05, explicitly approved): read-only enumeration and chip access confirmed USB identity ending `cb:bc`, `/dev/cu.usbmodem1101`, ESP32-C3 revision 1.1, 8 MiB flash. A fresh partition-table read confirmed factory at `0x10000`, capacity `0x300000`, and existing image/card/audio resources beginning at `0x310000`. Independent archive verification and app SHA256 confirmation passed before writing only the development app above: **1,896,320 bytes**, SHA256 `4440dc24af48a1657512939767bdf6794dab35d1835d60641942f6ddea746c99`, sector-rounded erase/write range `0x10000–0x1defff`. Write-time digest and independent `verify_flash` passed. The full 4 KiB partition-table sector remained byte-identical. Bootloader, NVS and resource ranges were not written; no full-chip erase occurred. A bounded reset/startup monitor with the matching ELF confirmed prefix `4ba452f26`, normal display/audio/button initialization and no panic or reset loop. Serial monitoring is closed. This establishes installation/startup only, not physical sprite, gameplay, BLE or endurance acceptance. First device `72:60` retains its earlier M5 development app; installed hashes differ. No firmware rebuild, commit, push or publication was performed during flashing.

User-observed sprite acceptance (2026-10-05): following the request to compare red/blue size in Shoot Practice and Keeper Practice and complete a kick to inspect blue contact/follow-through, the user reported normal and natural presentation. Record those requested visual checks as user-observed PASS on second device `cb:bc` with the unchanged development app SHA256 `4440dc24af48a1657512939767bdf6794dab35d1835d60641942f6ddea746c99`. This supersedes the pending size balance/contact/follow-through checks for that device and candidate only. Exhaustive physical nine-pose/six-cue coverage, measured frame latency, first-device update/acceptance, BLE regression and remaining M6/endurance checks were not reported. No firmware change, rebuild, flash, commit, push or publication was performed in this acceptance-record turn.

### 15.6 Room-identity jerseys (2026-10-05)

Accepted PvP mapping: creator shoots in red; joiner shoots in blue; both devices show that same shooter's kit and action frames, with a black keeper. Coin-toss order remains independent of room identity. Single-player mapping and the accepted 83.2% blue scale remain unchanged.

The old scene chose kit and kick frames from the local control role. A projection-only boolean now comes from the existing `duel.current_shooter` identity; scene rendering uses it for kit/frame selection. It is recomputed after snapshot restoration, not added to wire/storage formats. Multiplayer charge stays ready instead of using incompatible red-only windup or blue cue/contact indices. Locked waiting retains the foreground striker. Color-only changes invalidate the same 88 × 88 region. No protocol, RNG, coin, score, trajectory, settings, partition, stack, display buffer or LVGL pool changes were made. The host model remains 328 bytes.

The new two-view scene assertion fails on the previous implementation. Scene checks now cover both kits, control roles, aim/charge/wait, all six shot/dive cells and five outcomes. Projection checks cover both first-shooter orders and alternation; recovery checks validate kit identity after full-match/snapshot restoration. Actual LVGL tests check kit-colored pixels, both viewers/identities/languages across five phases, pixel-identical shared playing fields and color-only dirty/full redraw equivalence. Both renderers retain 12,248 free bytes after 50 warmed-up lifecycles. These are desktop checks; this new candidate has no physical acceptance.

Both complete gates and independent archive verification pass. Focused scene/projection/recovery checks also pass ASan/UBSan. Immutable candidate identities:

| Candidate | app / merged bytes | app SHA256 | merged SHA256 / archive ID | matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,894,592 / 1,960,128 | `afc8acc52a17acb5cb243ba27474676d8b0be0aeb007bcf91f6ca76ea8f5d067` | `cabbc83ac1cb51e9b3bcc12f2efae26be41a90ecbce2b92a40fc761511d0ccd2` | `6abec2d329d292663957cf785ffcb8d6babb45002b2514d92003d082a5f28c7f` |
| Development | 1,896,416 / 1,961,952 | `77effefcbbd24a20506b4a51dd43c509139fa645befb402dea19eac2d4603814` | `db91168148d84ccf07f8c2545774d365e1ee2f87d15546e45f88e24abe3e66af` | `7cd8d9a41a6ab1c002670bf20553c8ba88e25881cdcd1a9203043cdea80a7eb4` |

Archives are under `build/firmware/<merged-sha256>/`. Build description remains `penalty-v1.1-9-g49947c1-dirty`; unchanged `1.5.0` compatibility filenames do not identify this increment. Both apps fit the smaller existing 3 MiB factory partition. After separate installation approval and target/layout revalidation, write only the selected archived app at `0x10000` (sector range `0x10000–0x1defff`), not the merged image at `0x0`. Preserve bootloader, partition table, NVS and resources. Do not publish the development app.

```text
Build: PASS (normal and development complete gates; independent archives)
Host tests: PASS (static, actual LVGL, focused ASan/UBSan)
Device tests: PASS for both devices' installation/verification/bounded startup and user-observed jersey agreement
Unverified: explicit both-coin-order coverage, recovery, exhaustive animation poses and outstanding M6 checks
```

At build handoff, neither device contained this identity correction. The separately approved installations below supersede both installed-image statuses. Both devices now have the same verified development app, without replacing their layouts/resources. Compare creator-red/joiner-blue kicks, each first-shooter order, swapped Create/Join roles and recovery.

Second-device jersey-correction installation (2026-10-05, explicitly approved): the initial request named the first device, but read-only enumeration identified the connected USB device as `98:C3:77:F4:CB:BC`; the user then approved this connected device. Port `/dev/cu.usbmodem1101`, ESP32-C3 revision v1.1, 8 MiB Flash. The installed partition table passed MD5/bounds/non-overlap checks; factory remains at `0x10000`, size `0x300000`, with resources beginning at `0x310000`. Reverified the above development archive and app SHA256 `77effefcbbd24a20506b4a51dd43c509139fa645befb402dea19eac2d4603814`. Wrote only the **1,896,416-byte** app at `0x10000`, sector-aligned erase/write range **`0x10000–0x1defff`**. Write-time digest and independent `verify_flash` passed. The full 4 KiB partition-table sector is byte-identical before/after; bootloader, NVS and resource ranges were not written. A bounded startup window of at least 20 seconds matched ELF prefix `7cd8d9a41`, initialized display/audio/buttons normally, and showed no observed panic/reset loop. The monitor emitted a host-side missing default Xtensa addr2line warning for a saved ROM PC; this was not a device fault or a decoded crash. The monitor is closed. Local raw evidence is under `/tmp/penalty-jersey-flash.w1XcF7/`, including `partition-before.bin`, `partition-after.bin` and `log.FoloToy-AI-Passport.20261005204719.txt`. Installation/boot does not validate physical kit/action agreement. First device `72:60` is not updated; both-device acceptance remains pending. No rebuild, commit, push or publication was performed.

First-device jersey-correction installation (2026-10-05, separately approved): read-only enumeration and esptool confirmed `4C:11:AE:2F:72:60` on `/dev/cu.usbmodem1101`, ESP32-C3 revision v1.1, 8 MiB Flash. Installed partition-table MD5/bounds/non-overlap checks passed; factory remains at `0x10000`, size `0x7f0000`. Independently reverified the same development archive/app identity above. Wrote only the **1,896,416-byte** app SHA256 `77effefcbbd24a20506b4a51dd43c509139fa645befb402dea19eac2d4603814` at `0x10000`, sector-aligned range **`0x10000–0x1defff`**. Write-time digest and independent `verify_flash` passed. The full 4 KiB partition-table sector is byte-identical before/after; bootloader, NVS and other ranges were not written. At least 20 seconds of bounded startup observation matched ELF prefix `7cd8d9a41`, initialized display/audio/buttons normally and showed no observed panic/reset loop. The monitor used the matching ELF with the ESP32-C3 RISC-V toolchain; it is closed. Local evidence is under `/tmp/penalty-jersey-first-flash.417Wgz/`, including `partition-before.bin`, `partition-after.bin` and `log.FoloToy-AI-Passport.20261005210816.txt`. This receipt supersedes the previous first-device installed-image status: both devices now have the same app, but physical multiplayer kit/action agreement is not yet accepted. No rebuild, commit, push or publication was performed.

User-observed jersey acceptance (2026-10-05): after being asked to check both-player kicks, both-screen creator-red/joiner-blue agreement, black keeper and swapped Create/Join roles, the user reported normal presentation. Record the requested jersey checks as user-observed PASS on both devices' unchanged app SHA256 `77effefcbbd24a20506b4a51dd43c509139fa645befb402dea19eac2d4603814`. This supersedes the preceding pending jersey-gameplay status only within the requested scope. No per-case logs, explicit both-coin-order, recovery, exhaustive pose or endurance results were provided. It does not accept subsequent firmware changes.

### 15.7 Menu spacing and paired-round counters (2026-10-05)

The Shootout submenu and discovered-lobby list now use a dedicated menu panel with 40-pixel row pitch at y=84. Up to four rows fit above the footer; selected brackets and navigation remain. Lobby formatting no longer appends an empty final row. Connection/security/status/game panels retain zero additional line spacing. No LVGL pool, display-buffer or stack increase was made.

BLE UI converts synchronized one-based kick IDs to paired display rounds (1/2 → 1, 3/4 → 2), without changing the wire ID, snapshot, RNG, coin, scoring or match rules. AI regular result pages retain the current pair number until the next handoff; AI separately labeled sudden-death pairs and practice counters retain their semantics. English headers use R/SD; Chinese headers explicitly say paired round. Scores remain local-first. The pinned converter regenerates both Chinese sizes to add missing U+5408, with 247 localized glyphs covered.

The old AI result page fails the new regression. Unit checks cover both coin orders through regulation/sudden death, settlement/result/handoff boundaries and overflow-safe kick-to-pair arithmetic. Recovery tests compare both peers' display rounds. Actual bilingual LVGL checks cover all four submenu selections, lobby layout, spacing reset, both-player match phases, AI result/handoff headers, glyphs, clipping and dirty/full redraw equivalence. Both 50-cycle renderers still have 12,248 free bytes after warmed-up teardown. Desktop captures are not device acceptance.

Both complete gates and independent archive verification pass. The model also passes ASan/UBSan. Immutable candidate identities:

| Candidate | app / merged bytes | app SHA256 | merged SHA256 / archive ID | matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,895,088 / 1,960,624 | `2df5b9162f47d8762d7f71f148f75c09a39bcfbaa7c9990219a5e28ac7e2beb9` | `ac4cc225bb96aff0896230491fb5b92f9a3866f76044799c4dd207359faa0b5f` | `eaaa0dd878fef0e57f99cf758a0bec5a68652b9587da27cba947a4f8b0fd7883` |
| Development | 1,896,944 / 1,962,480 | `6e83eafda74fd2423477dc4cf442760eac3bf93b009028a87d2b03b3f7ae4603` | `307ccff8c7ed5c27afd65039ab438ec216da0f7bad9208053ceca5b68b8de780` | `1573932948742439dc0748219aee5a3ca384fe34697b5793fab3b77c1506f327` |

Archives are under `build/firmware/<merged-sha256>/`. Build description remains `penalty-v1.1-9-g49947c1-dirty`; use the hashes, not unchanged `1.5.0` compatibility filenames, to identify this increment. Both apps fit the smaller existing 3 MiB factory partition. After separate approval and target/layout revalidation, write only the selected archived app at `0x10000` (normal sector range `0x10000–0x1defff`; development `0x10000–0x1dffff`), preserving bootloader, partition table, NVS and resources. Do not use a merged image at `0x0` to replace existing layouts or publish the development app.

```text
Build: PASS (normal/development complete gates; independent archives)
Host tests: PASS (static, actual LVGL, model ASan/UBSan)
Device tests: PASS (both devices' app installation, independent verification and bounded startup only)
Unverified: physical submenu/lobby spacing, AI/PvP pair numbering through results/alternation/recovery and outstanding M6 checks
```

First-device installation (2026-10-05, explicitly authorized): read-only enumeration and esptool identify `4c:11:ae:2f:72:60` at `/dev/cu.usbmodem1101`. The live partition table passes MD5, bounds, unique-label and non-overlap checks; factory is still at `0x10000`, size `0x7f0000`. Only the archived development app above was written (1,896,944 bytes, sector range `0x10000–0x1dffff`). Write-time verification and independent `verify_flash` pass. The entire 4 KiB partition-table sector matches its pre-write copy; bootloader/NVS/PHY are outside the write range. Bounded startup identifies ELF prefix `157393294` and normal display/audio/button initialization, with no observed panic or restart loop. The matching-ELF RISC-V monitor exits successfully and releases the port. Raw local log: `/tmp/penalty-round-flash.L2y4mr/log.FoloToy-AI-Passport.20261005215006.txt`. No rebuild, commit, push or publication occurred during installation.

Device `72:60` now has this spacing/counter development app; `cb:bc` still has the accepted section 15.6 jersey candidate. Physical submenu/lobby spacing and pair numbering through both kicks/results, next-round transitions, AI/PvP and reconnect remain unverified. Update the second device only after separate approval and target/layout revalidation. Outstanding M6 cases remain unverified.

Second-device installation (2026-10-05) supersedes the preceding installed-image state. The initial write was blocked by approval review and did not run. After the user explicitly confirmed the exact second-device flash, read-only enumeration and esptool reconfirmed `98:c3:77:f4:cb:bc` at `/dev/cu.usbmodem1101`. The live partition table passes MD5, bounds, unique-label and non-overlap checks; factory remains at `0x10000`, size `0x300000`, with resources starting at `0x310000`. The same archived development app SHA256 `6e83eafda74fd2423477dc4cf442760eac3bf93b009028a87d2b03b3f7ae4603` (1,896,944 bytes) was written only at `0x10000`, sector range `0x10000–0x1dffff`. Write-time verification and independent `verify_flash` pass. The full 4 KiB partition-table sector is unchanged; bootloader/NVS/PHY/resources are outside the write range. Matching-ELF RISC-V startup observation exceeds 20 seconds, identifies ELF prefix `157393294`, and shows normal display/audio/button initialization with no observed panic/restart loop. The monitor exits successfully and closes its log: `/tmp/penalty-round-second-flash.Pngbf9/log.FoloToy-AI-Passport.20261005220959.txt`. Both devices now have the same spacing/counter development app. Physical menu and AI/PvP counter acceptance, recovery and outstanding M6 coverage remain unverified. No rebuild, commit, push or publication occurred during installation.

### 15.8 Unframed cover prompt and menu selection markers (2026-10-06)

The Chinese cover prompt now names the confirmation key instead of OK. Text uses the long-confirm hint's muted green `0xA2BC9E`. The existing label masks the artwork's old border with `0x093A20` background at (40,263), size 159 × 33, with 8-pixel top padding. Source art and all pixels outside this rectangle remain. Both languages retain the same font/baseline, including exceptional retry copy. Cover color/padding/background reset outside the cover. No new widgets, buffers, stacks or larger LVGL pool were added.

Title/settings/Shootout-submenu/lobby selections use `>> item <<`; unselected items have no marker. Retry/title summary choices use the same style with four separating spaces. Existing row pitches, wrapping navigation, input routing, paired rounds, jerseys, scoring and BLE rules remain. Existing 247-glyph subsets cover the wording and ASCII markers; no regeneration was needed.

The old cover fails the new exact-copy assertion. Focused i18n/font checks pass after the fix. Actual bilingual LVGL checks cover all title/settings/submenu selections, lobby markers, clipping, prompt color, every old border-edge pixel, other artwork pixels and style reset. Both complete gates pass, including existing round/jersey regressions, dirty/full redraw and two 50-cycle renderers with 12,248 free bytes after warmed-up teardown. Desktop PNGs were visually inspected; they are not physical acceptance. Both archives were independently verified:

| Candidate | app / merged bytes | app SHA256 | merged SHA256 / archive ID | matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,895,312 / 1,960,848 | `dfcb210477c0095fc4c6ef6bfe5a68463765bcf94d60179649a53b2b5693b557` | `69b96e300ed8e69a82090c997abef20ab5fba1d0c3bc452701898680a35b52c4` | `a69f03fd8baa423ec99c103192596fbab8c4b86e3c36731578b72658723a506c` |
| Development | 1,897,168 / 1,962,704 | `75b786599369b3c23587ba71b3d7a5d048d1d7206fadfaff3b6b86c2057521de` | `a3d7bf64de262f9a94da481cb239f6c857a49c93313215ea8d233572b5bb398f` | `7d03b1959b5dc471dcd54eb1278470542a9e999f753e25e058ddea1a76ddc4df` |

Archives: `build/firmware/<merged-sha256>/`. Descriptor remains `penalty-v1.1-9-g49947c1-dirty`. Both apps fit the smaller existing 3 MiB factory. After separate approval and target/layout revalidation, write only the selected archived app at `0x10000`: normal sector range `0x10000–0x1defff`, development `0x10000–0x1dffff`. Preserve bootloader, partition table, NVS and resources. Do not overwrite existing layouts with a merged image at `0x0` or publish the development app.

```text
Build: PASS (normal/development complete gates; independent archives)
Host tests: PASS (static, glyph/copy contracts and actual bilingual LVGL)
Device tests: PASS (cb:bc app installation, independent verification and bounded startup only)
Unverified: physical prompt/markers, Chinese rendering and outstanding section 15.7/M6 checks
```

No USB device was detected at handoff. Both devices remain on section 15.7's app; this new increment is not installed. No flash, commit, push or publication occurred.

Authorized installation (2026-10-06) supersedes that handoff state. The user requested flashing the connected device. Enumeration and esptool identify `98:c3:77:f4:cb:bc` at `/dev/cu.usbmodem1101`. Live partition MD5, bounds, unique labels and non-overlap pass; factory remains at `0x10000`, size `0x300000`, with resources beginning at `0x310000`. The independently verified development app above was written only at `0x10000` (1,897,168 bytes; SHA256 `75b786599369b3c23587ba71b3d7a5d048d1d7206fadfaff3b6b86c2057521de`; sector range `0x10000–0x1dffff`). Write-time verification and independent `verify_flash` pass. The complete 4 KiB partition-table sector matches the pre-write copy. Bootloader/NVS/PHY/resources are outside the write range. Matching-ELF RISC-V startup observation exceeds 20 seconds, identifies ELF prefix `7d03b1959`, and shows normal display/audio/button initialization without observed panic or restart loop. The monitor exits successfully and closes `/tmp/penalty-cover-flash.NzOfkd/log.FoloToy-AI-Passport.20261006013002.txt`.

Device `cb:bc` now has this prompt/selection app; `72:60` remains on section 15.7's spacing/counter app. Physical prompt, selection markers and Chinese rendering remain unverified, along with prior section 15.7/M6 checks. No rebuild, commit, push or publication occurred during installation.

### 15.9 Cover mark spacing and size (2026-10-06)

The user requested more space above the cover's Chinese game mark and slightly larger lettering. Retain the existing cream/shadow pixel style and wording. Draw a read-only RGB565 crop at source (56,232), size 128 × 19, original stride 480 bytes, at (43,238) with scale 307/256 (about 120%) and zero pivot. Mask the old mark with each existing grass row's background color. The mark moves down 6 pixels; the start prompt remains at its previous position. No source art, fonts, input, menu, scoring, BLE, partition, widget count, pool, draw buffer or stack changes occur.

The original cover fails the new mark-position regression. Actual bilingual LVGL tests assert mark position, larger bounds, centering, cream-pixel coverage and separation from the prompt. Pixel equality excludes only the old/new mark region and prior prompt mask. The upper artwork remains pixel-identical. Both complete ESP-IDF 5.5.3 gates pass, including prior regressions, dirty/full redraw and both 50-cycle renderers with 12,248 free bytes after warmed-up teardown. A desktop PNG was visually inspected; it is not device acceptance. Both archives were independently verified:

| Candidate | app / merged bytes | app SHA256 | merged SHA256 / archive ID | matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,895,504 / 1,961,040 | `4e850019458d88234b2a64dfbc46ad1105896940ca1898221a9b0f80bed2e5bf` | `cf242e5009cc116e93507a870b8d7924771c9fba95c07a0d93de52239607cedf` | `0388ac8bd0b8da160f0343ab39916972dcbc8c68ced6a166738aafa0cb077ea5` |
| Development | 1,897,360 / 1,962,896 | `e7b22e16fd6d0dddf6b208b4e419dbc2c8581ecfd0f3633726e5235086f6141e` | `7a987b7081190568a53946d3e60e792cc3f6206bd4588dc3646c96833a01f032` | `7f8d801d6a7ada5341ecab621aa6364f4de67c14a4ecac8949989f97e1b68733` |

Archives: `build/firmware/<merged-sha256>/`. Descriptor remains `penalty-v1.1-9-g49947c1-dirty`; compatibility filenames are not a release identity. Both apps fit the smaller existing 3 MiB factory. After separate authorization and live target/layout revalidation, write only the selected archived app at `0x10000`: normal sector range `0x10000–0x1defff`, development `0x10000–0x1dffff`. Preserve bootloader, partition table, NVS and resources. Do not flash a merged image at `0x0` over existing layouts or publish the development app.

```text
Build: PASS (normal/development complete gates; independent archives)
Host tests: PASS (static, actual bilingual LVGL and lifecycle regressions)
Device tests: PASS (cb:bc installation/verification/startup and user-confirmed cover mark size/spacing)
Unverified: outstanding section 15.7/15.8/M6 checks and later firmware increments
```

Read-only USB enumeration detects `98:c3:77:f4:cb:bc` at `/dev/cu.usbmodem1101`; the port was not opened. `cb:bc` remains on section 15.8's app and `72:60` on section 15.7's app. This increment is not installed. No flash, commit, push or publication occurred.

Authorized second-device installation (2026-10-06) supersedes that handoff state. The user explicitly requested flashing the second device. Read-only USB enumeration and esptool reconfirmed `98:c3:77:f4:cb:bc` at `/dev/cu.usbmodem1101`, ESP32-C3 with 8 MiB Flash. Live partition MD5, bounds, unique labels and non-overlap pass; factory remains at `0x10000`, size `0x300000`, resources beginning at `0x310000`. Independent archive/hash verification passes. The development app above (1,897,360 bytes; SHA256 `e7b22e16fd6d0dddf6b208b4e419dbc2c8581ecfd0f3633726e5235086f6141e`) was written only at `0x10000`, sector range `0x10000–0x1dffff`. Write-time verification and independent `verify_flash` pass. The full 4 KiB partition-table sector matches the pre-write copy. Bootloader/NVS/PHY/resources are outside the write range. Matching-ELF RISC-V startup observation exceeds 20 seconds, identifies ELF prefix `7f8d801d6`, and shows normal display/audio/button initialization with no observed panic/restart loop. The monitor exits successfully and closes `/tmp/penalty-cover-mark-flash.jguRVK/log.FoloToy-AI-Passport.20261006120321.txt`. `cb:bc` now has this cover-mark app; `72:60` remains unchanged on section 15.7's app. Physical cover acceptance and previous outstanding checks remain unverified. No rebuild, commit, push or publication occurred during installation.

User acceptance (2026-10-06): the user confirmed the requested physical cover mark size and upper spacing on this installed `cb:bc` app. This supersedes the preceding pending cover-acceptance status only for the same app SHA256. It does not validate later size-optimized firmware or the remaining gameplay/endurance matrix.

### 15.10 Footprint audit and lossless reduction (2026-10-06)

The user accepted section 15.9's physical cover and requested an installed-size audit plus code/size reduction. That installed development app was independently verified on `cb:bc`; its app SHA256 remains `e7b22e16fd6d0dddf6b208b4e419dbc2c8581ecfd0f3633726e5235086f6141e`. The current 3 MiB factory layout was read and checked during that installation. File bytes, sector-aligned writes and reserved partition capacity are distinct:

| Measurement | Installed development baseline | Optimized development candidate |
| --- | ---: | ---: |
| App bytes | 1,897,360 | 1,574,416 |
| App write sectors, bytes | 1,900,544 | 1,576,960 |
| Merged provisioning image, bytes | 1,962,896 | 1,639,952 |
| Reserved factory capacity, bytes | 3,145,728 | 3,145,728 (unchanged) |
| Capacity minus app bytes | 1,248,368 | 1,571,312 |
| ELF static D/IRAM usage, bytes | 141,596 | 131,098 |

The app saves 322,944 bytes (315.375 KiB, 17.02%). Firmware is stored/executed from Flash; it does not expand to ELF/debug-file size after installation. Transfer compression is temporary. The merged file includes bootloader/table/gaps and is not what was written in the app-only installation. Unwritten app capacity is not a filesystem allocation, and shortening a future write does not erase stale bytes farther into the existing app partition. Separate NVS/resources are excluded. Physical runtime heap and latency remain separate measurements.

The audit found 663,156 bytes of cover/gameplay pixels, 75,938 Chinese glyph bitmap bytes, default debug optimization and unused built-in LVGL themes. Baseline demo sources are not linked into the game; deleting them would not reduce the firmware. The implemented changes are:

- Select supported `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (`-Os`), preserving assertions, diagnostic logs and full 64-bit formatting.
- Disable default/simple/mono LVGL themes in firmware and the actual host renderer. Penalty already provides the visible styles.
- Remove only normalized fully transparent outer rows/columns of five red-shooter and seven keeper frames. Generated offsets preserve logical anchors, dirty bounds and keeper contact points. Gameplay pixel data falls from 509,556 to 404,592 bytes (104,964 saved). Blue canvases remain intact because cropping alters existing scaled sampling; balls retain dimensions used for shadows/radii.
- Generate both Chinese sizes as lossless non-prefiltered RLE, with the built-in decoder enabled in both configurations. Bitmaps fall from 75,938 to 58,034 bytes (17,904 saved before decoder/alignment costs). All 247 localized glyphs, metrics and 4-bpp opacity remain.

No game/coin/scoring/protocol/security/recovery/audio behavior, source PNG, pool, draw buffer, task stack or partition was changed. No new runtime image decompression buffer is required. Font decoding uses bounded glyph/row buffers. More aggressive image compression would require a different drawing/decoding path with new CPU/RAM acceptance; reducing colors, animation frames or diagnostics was not part of this behavior-preserving pass.

Both complete ESP-IDF 5.5.3 gates pass, including original model/protocol/recovery/resource regressions, font/art contracts, dirty/full redraw and both 50-cycle renderers. All 234 freshly generated final development captures match the pre-change PPM hashes; ten surplus historical normal-output captures are excluded. The host-only comparison manifest is `/tmp/penalty-size-screen-comparison.json`. A separate 21-frame pixel comparison with source PNGs verifies all visible RGB565 and normalized alpha. LVGL remains 24 KiB; warmed-up free bytes after exit improve from 12,248 to 17,128, with the same 27,640 maximum moving-frame pixels. This is host evidence, not physical timing or heap. Both final archives pass independent verification:

| Candidate | app / merged bytes | app SHA256 | merged SHA256 / archive ID | matching ELF SHA256 |
| --- | --- | --- | --- | --- |
| Normal | 1,572,864 / 1,638,400 | `e1cfb243d7b185751c8e0b5517ca1ec89bdc7497434e2071a44807f8398971bd` | `40d8af6805ea0871110ba1c0c4cb3f04c6b34f5feb8831a0d44a7dafaad36a60` | `73f0ef5482d682c88a080210c396321c7611cafbac3fe38f1fa4200f79bbdf28` |
| Development | 1,574,416 / 1,639,952 | `80c929df3934877d28ea66b9eac7a5c72e7e780cbea0ca2fd19a78f1f6a08353` | `95c107fc1cbca849d321bef6630132c51a60acc4890aea5e35fba4c2f7dd670b` | `1bac42a87923c5405f326c11ae6e686b34f61397b9475b2ad9d034aa5b747b02` |

Archives remain `build/firmware/<merged-sha256>/`; normal/development delivery paths and the compatibility descriptor are unchanged. Subject to separate flash authorization and live identity/layout confirmation, the compatible app-only sector ranges at `0x10000` are normal `0x10000–0x18ffff`, development `0x10000–0x190fff`. Preserve bootloader/table/NVS/PHY/resources. Do not install a merged image over an existing layout or publish the development app.

```text
Build: PASS (normal/development complete gates and independent archives)
Host tests: PASS (static, original regressions, actual LVGL and pixel comparisons)
Device tests: PASS (cb:bc optimized app installation, verification and bounded startup only)
Unverified: text/animations/audio, BLE roles/recovery and sustained runtime resources
```

Read-only enumeration still detects `cb:bc`; no port was opened this turn. `cb:bc` retains section 15.9's app; `72:60` retains section 15.7's app. No flash, commit, push or publication occurred.

Authorized second-device optimized-app installation (2026-10-06) supersedes the preceding installed state. USB enumeration and esptool reconfirmed `98:c3:77:f4:cb:bc` at `/dev/cu.usbmodem1101`, ESP32-C3, 8 MiB Flash. Live partition MD5/bounds/unique-label/non-overlap checks pass; factory remains `0x10000`/`0x300000`, with resources from `0x310000`. The independently verified development app above (1,574,416 bytes; SHA256 `80c929df3934877d28ea66b9eac7a5c72e7e780cbea0ca2fd19a78f1f6a08353`) was written only at `0x10000`, sector range `0x10000–0x190fff`. Write-time verification and independent `verify_flash` pass. The complete 4 KiB partition-table sector is byte-identical before/after; bootloader/NVS/PHY/resources are outside the write range. Matching-ELF startup observation exceeds 20 seconds and identifies prefix `1bac42a87`, with normal display/audio/button initialization and no observed panic/restart loop. The monitor closes successfully: `/tmp/penalty-size-flash.TH9WU4/log.FoloToy-AI-Passport.20261006122209.txt`.

The same startup-entry log point reports 224,796 free heap bytes versus 214,236 in section 15.9 (10,560 more), with the same 114,688-byte largest block. This is one startup sample, not BLE/endurance or peak-runtime acceptance. Physical text, animations, audible output and multiplayer/resource checks remain pending. `cb:bc` now runs this optimized app; `72:60` remains on section 15.7. No rebuild, commit, push or publication occurred during installation.

## 16. Review sources

- [Current Penalty model](../../../main/penalty_model.c) and [model interface](../../../main/penalty_model.h)
- [Current application lifecycle](../../../main/penalty_app.c) and [Launcher shell](../../../main/main.c)
- [Current BLE advertising demo](../../../main/demo_ble.c) and [`sdkconfig.defaults`](../../../sdkconfig.defaults)
- [Penalty v1.5.0 validation boundary](../validation.md)
- [AI development guide](../../../docs/development/ai-guide.md) and [hardware development guide](../../../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)
- [ESP-IDF 5.5.3: Bluetooth LE Connection](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/ble/get-started/ble-connection.html)
- [ESP-IDF v5.5.3 NimBLE Peripheral security and RAM options](https://github.com/espressif/esp-idf/blob/v5.5.3/examples/bluetooth/nimble/bleprph/README.md)
- [ESP-IDF 5.5.3: ESP32-C3 Power Management](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-reference/system/power_management.html)
