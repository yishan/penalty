<p align="right">
  <a href="community-submission.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Community submission

Prepared on 2026-09-15 for the official FoloToy AI Passport community. Local game version: **v0.3**. Category: **games**. Submission is authorized, but preparation alone does not mean public availability.

## Prepared cover updates — not uploaded

On 2026-09-16, a [label-free v0.4 community cover](../../assets/images/penalty/community-cover-v0.4-no-label.png) was generated at 1086 x 1448, exact portrait 3:4. The later [v1.1 candidate](../../assets/images/penalty/community-cover-v1.1-horizontal.png) preserves that historical file and applies the current horizontal pitch bands, black keeper kit, and exact Chinese game mark. Both remain illustrative artwork, not physical screenshots. Neither replacement has been uploaded; project 402, revision 735, and the last confirmed `pending` review state below are unchanged.

## Public materials

- Exact bilingual titles and descriptions, input filenames, SHA-256 checksums, and the final image-generation prompt are in [the submission manifest](../publishing/submission.json). It contains no credentials.
- English title: **Penalty: Five-Shot Challenge**.
- [Community cover](../../assets/images/penalty/community-cover-v0.3.png): 1086 x 1448 PNG, exact portrait 3:4, generated with the built-in imagegen tool from the existing cover. The rotation instructions were removed and the image is marked `AI ILLUSTRATION`. It is not a physical screenshot and does not replace the firmware's opening instructions.
- Firmware: `build/Penalty-community-v0.3-full.bin`, 919,968 bytes, freshly rebuilt and validated as a complete image for offset `0x0`. SHA-256: `26ca8313be811a0167fbec29b3881e94dfce50b27f03a749d63d017f9a7eaf60`. The complete gate passed; transcript: `build/penalty-publish-validation.log`. This copy matches the rebuilt `build/FoloToy-AI-Passport-full.bin`; the earlier portrait-cover acceptance copy remains unchanged.
- Extra gameplay images omitted. No device connection, flashing, or screenshot capture is required for this submission.
- Source URL omitted: the configured public remote is the upstream template; the local Penalty changes are not published there. No repository was created or pushed as part of this submission.

## Validation boundary

Build: PASS. Host tests: PASS. Device tests: NOT RUN. The public descriptions disclose pending on-device acceptance. Physical behavior and the current host ASan runtime remain unverified as detailed in [validation](validation.md).

## Receipt

Submitted successfully with `submit --auto`, then independently checked using `projects`. The server returned one project and the same submission receipt, with the uploaded firmware checksum matching the manifest.

- Project ID: **402**; slug: **penalty**.
- Revision ID: **735**; local game version: **v0.3**; server state version: **1**.
- Submitted: **2026-09-15 04:49:10 UTC**.
- Confirmed status: **pending** for both the project and revision; `reviewedAt` is null. **Submitted for review, not publicly published.** This is the status at submission verification, not ongoing monitoring.
- Sanitized [receipt](../publishing/receipt.json) contains no authorization data. Credentials remain outside the repository with owner-only permissions.
