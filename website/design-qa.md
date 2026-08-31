# Design QA

## Learning / Examples / Extensions alignment · 2026-08-29

final result: passed

- Source visual truth: `output/design-audit/nav-pages/01-home.png` (1440 × 1000 px; homepage editorial system)
- Desktop implementations: `output/design-audit/nav-pages/12-learn-compact.png`, `13-examples-compact.png`, `14-extensions-compact.png` (1280 × 720 browser captures; 1280 CSS viewport)
- Mobile implementation: `output/design-audit/nav-pages/15-learn-compact-mobile.png` (390 × 844 px; 390 × 844 CSS viewport)
- Routes and state: `/zh/learn`, `/zh/learn/practical-recipes`, `/zh/learn/extensions-roadmap`; light theme; default state

### Full-view and focused comparison evidence

- The homepage reference and all three desktop implementation captures were opened together in the same comparison input. The pages now share the homepage's centered content frame, 10 px corner radius, blue-gray border, white editorial cards, navy/blue hierarchy, and compact spacing rhythm.
- Focused checks covered the global navigation active state, documentation hero, sidebar frame, chapter cards, code panel, callout, and table. No replacement or generated raster assets were required; the existing HHY logo and Phosphor icons remain sharp and consistent.
- Fonts/copy remain the existing site truth. Colors use the shared `--ink`, `--blue`, `--line`, and page tokens. The current HHY content was not rewritten.

### Interaction and responsive verification

- Desktop navigation correctly reports exactly one active item for 学习, 示例, and 扩展.
- Mobile navigation successfully opens and routes to all three pages with the matching active state.
- At 390 px, the desktop manual sidebar is removed from the initial reading path, the content starts at the top, and document width equals the viewport.
- No browser console errors. TypeScript, ESLint, and the optimized Next.js build pass.

### Comparison history

1. P2: the three pages used a 1280 px documentation frame, loose unframed headings, 15 px card radii, and no top-navigation active state, visibly drifting from the 1180 px homepage editorial system.
2. Fix: unified frame width, border/radius tokens, cardized the documentation hero and sidebar, wrapped chapter headings in the same hero treatment, and added precise desktop/mobile active states.
3. P1 mobile: the full manual sidebar pushed content below 1400 px on phone-sized viewports.
4. Fix: removed the desktop sidebar from the phone reading path; the chapter content now begins at 104 px with no horizontal overflow.
5. P2 density: the first aligned pass still left hero and chapter cards visibly too tall.
6. Fix: reduced hero padding/type scale and chapter-card minimum heights/padding; final desktop hero is approximately 250 px on the learning index and 176 px on chapter pages, while mobile keeps readable line height and touch-safe spacing.
7. P2 learning-index density: chapter cards still consumed too much vertical space. Reduced the grid gap, section spacing, padding, text scale, and summary height. Final desktop guide cards align at exactly 159 px; mobile cards render at 161–163 px, with no summary truncation, overflow, or console errors.
8. P1 documentation navigation: the active chapter did not expose page-section anchors or follow the reading position. Added an auto-positioned, expandable desktop table of contents under the current chapter and a mobile “本页目录” dropdown. Browser checks confirm anchor hashes, 110 px section offsets, scroll-driven active states, current-chapter visibility, 390 px overflow safety, and zero console errors.

Residual P3: none identified in the requested alignment scope.

## Homepage equal-height hero · Flow / Pipe / System · 2026-08-29

final result: passed

- Source visual truth: `/Users/houhuiyang/.codex/generated_images/01a04dc0-631c-7d83-a186-572fe3b02773/exec-338b5831-0420-4d03-ad47-a62d6123f3e9.png` (1721 × 914 px)
- Desktop implementation: `implementation-hero-equal-columns-desktop.png` (1440 × 1005 px capture; 1440 × 1024 CSS viewport; DPR-normalized browser capture)
- Mobile implementation: `implementation-hero-equal-columns-mobile.png` (390 × 844 px; 390 × 844 CSS viewport)
- Route and state: `http://127.0.0.1:9800/zh`, light theme, default homepage state

### Full-view and focused comparison evidence

- The source and desktop implementation were opened together in the same comparison input. Both use one bordered split frame, a 42/58-style editorial division, aligned bottom proof rows, and the same content hierarchy.
- Browser geometry measured both desktop columns at exactly 648 px high, top 123 px and bottom 771 px. The document width equals the 1440 px viewport.
- The Flow / Pipe / System row was inspected as the focused fidelity region in both the source and browser capture. It uses Phosphor vector icons and the exact language principles from `docs/HHY_V1.md`.
- Typography, spacing, white/light-blue tokens, vector icon sharpness, and current SiteGraph Auditor copy were visually checked. The implementation intentionally uses the website's existing font stack and truthful current content.

### Interaction and responsive verification

- Mobile document width equals the 390 px viewport; no document-level horizontal overflow.
- Mobile global search opens and closes.
- Mobile navigation opens successfully.
- Browser console logs: no errors or warnings.
- TypeScript, ESLint, optimized Next.js build, and `git diff --check` pass.

### Comparison history

1. P1: the earlier hero used two detached regions and left the introduction visually underfilled relative to the SiteGraph project card.
2. P2: the initial filler strip looked like generic feature navigation and did not express the language itself.
3. Fix: rebuilt the hero as one equal-height frame with a shared divider and aligned proof baselines; replaced the filler strip with the canonical Flow / Pipe / System principles.
4. Post-fix evidence: desktop geometry confirms exact equal height; the 390 px layout stacks into two readable cards and keeps all primary interactions working.
5. P1: the first proof-row pass used equal heights but their top and bottom rules were not on the same baseline. The final browser measurement is 86 px on both sides with a 0.039 px subpixel top/bottom delta (effectively the same rendered line).
6. P1: the first featured-project panel showed JSON configuration instead of HHY source, then an incomplete HHY fragment. It now shows the real SiteGraph core flow from `try` through crawl, inventory, graph, report, atomic output, failure exit, and `catch`, with only repeated output writes explicitly omitted using a valid HHY `#` comment.
7. Post-fix evidence: desktop and 390 px mobile both report zero code overflow; browser console is empty.
8. P1: content-derived footer positioning still produced visible line drift at wider desktop viewports. Replaced the tuning with a structural layout: both 86 px footer rows are absolutely anchored to the bottom of their equal-height grid columns, with matching reserved padding.
9. Post-fix evidence: browser geometry reports exactly `0px` top, height, and bottom delta at 1440, 1920, and 2048 px desktop widths. At 390 px both rows correctly return to static flow, document width remains 390 px, and console logs remain empty.
10. P2: after structural footer anchoring, the HHY code panel touched the shared footer rule at some wide viewports. Added a desktop-only 24 px safety gap; browser geometry confirms exactly 24 px at 1440, 1920, and 2048 px while preserving 0 px footer-line delta and equal 86 px footer heights. Mobile keeps natural flow with no duplicate margin or overflow.
11. P2: the platform verification originally read as one plain text sentence. It now uses centered, separately grouped Phosphor Apple, Linux, and Windows icons with individual success states and subtle separators. Browser geometry confirms the group center exactly matches its parent center (`0px` delta), the shared footer line remains aligned, and the 390 px layout has no overflow.
12. User-requested simplification: removed the entire “Continue learning” rail and removed the homepage's trailing 82 px bottom padding. Browser geometry confirms the rail count is zero, the platform-proof bottom equals the homepage bottom, and the site footer begins with exactly `0px` intervening gap on desktop; mobile remains overflow-free.
13. User-requested chrome cleanup: removed the GitHub mark from the header link, removed desktop and mobile download buttons, and removed the GitHub mark from the footer while retaining the text links. Browser DOM checks report zero matching icons/buttons on desktop and mobile, no overflow, and an empty console.
14. User-requested icon correction: replaced both GitHub marks in the community card (Contribute and Visit GitHub repository) with the existing Phosphor Code icon. Browser checks confirm both vector icons render, with no overflow or console entries.
15. P1 content alignment: the hero verification incorrectly showed generic macOS/Linux/Windows labels while the public-release proof lists the actually verified targets. The icon row now matches exactly: macOS arm64, Linux arm64, and Linux x86_64. Desktop and mobile DOM checks confirm identical labels, zero group/document overflow, and no console entries.

Residual P3: the browser implementation is slightly denser than the generated concept so the complete header and beginning of the next section remain visible at 1024 px; hierarchy and component proportions remain faithful.

## Homepage editorial redesign · featured project correction · 2026-08-29

final result: passed

- Source visual truth: `/Users/houhuiyang/.codex/generated_images/01a04dc0-631c-7d83-a186-572fe3b02773/exec-862d9b1a-5087-49a1-8064-0f333cc4d811.png` (1487 × 1058 px)
- Desktop implementation: `implementation-desktop-featured-fixed.png` (1440 × 1760 px full-page capture; 1440 × 1024 CSS viewport)
- Mobile implementation: `implementation-mobile-featured-fixed.png` (390 × 844 px)
- Route and state: `http://127.0.0.1:9800/zh`, light theme, default homepage state

### Visual comparison

- The featured card now follows the selected concept's compact light-blue panel, single featured label, headline and two-line description hierarchy.
- The five URL → Frontier → Fetch → Parse → Report stages use equally weighted icon tiles and directional arrows.
- The code area is a light, bordered, line-numbered panel using the current SiteGraph Auditor configuration rather than fabricated sample output.
- The footer is reduced to the source design's single shield verification row; the implementation-only dark terminal and artifact strip were removed.
- The card uses content-driven height and no longer stretches into a large empty panel.

### Functional and responsive verification

- Desktop visual comparison completed against the source in the same inspection pass.
- Mobile viewport has `scrollWidth === innerWidth === 390`; the flow remains horizontally scrollable inside the card without creating document overflow.
- Mobile global search opens and closes successfully.
- Mobile navigation opens and exposes Learn, Examples, Extensions, Spec, GitHub, language, and download destinations.
- TypeScript, ESLint, optimized Next.js build, and `git diff --check` pass.

### Comparison history

1. P2: the first implementation used a dark terminal, an extra artifact strip, a top-right project link, and stretch alignment, causing the card to diverge from the source and leave excessive empty space.
2. Fix: adopted the source's light code surface, compact vertical rhythm, five equal flow tiles, one verification row, and content-sized card alignment.
3. Post-fix desktop and 390 px mobile evidence confirms the corrected hierarchy and no document-level horizontal overflow.

Residual P3: the code text intentionally uses the real current SiteGraph Auditor configuration, while the design concept used illustrative placeholder code.

final result: passed

- Source: `../assets/website/ChatGPT Image 2026年8月25日 14_17_10.png`
- Implementation: `http://127.0.0.1:8100/zh`
- Comparison viewport: 941 × 900
- Responsive viewport: 390 × 844
- Comparison artifact: `qa-comparison-first-view.png` (local, gitignored)

## Visual comparison

The implementation preserves the reference hierarchy, white and light-blue palette,
two-column hero, branded HHY artwork, bordered code sample, feature cards, learning
path, release status, and compact language-project footer. Chinese copy and the
real V1.1.1 examples replace the English concept placeholders. Header and footer
navigation were intentionally simplified to remove overlapping destinations.

## Functional verification

- Chinese and English home pages render.
- English “Get Started” opens `/en/learn/quick-start`.
- All ten Learn chapters are statically generated in both languages.
- Mobile navigation opens at 390 px with no horizontal overflow.
- Footer domain, email, repository, specification, and license links resolve to
  their intended targets.
- Browser console: no errors or warnings.
- Eleven documentation snippets pass the real `hhy check` command.
- ESLint, TypeScript, and the optimized Next.js production build pass.

## Comparison history

1. Initial pass matched the reference sections but had redundant header and footer
   destinations and excessive tablet-height wrapping.
2. Final pass reduced the header to Learn, Spec, GitHub, and the language switch;
   changed Download from the repository homepage to the verified latest Release;
   grouped the footer as Learn, Project, and Contact; retained the desktop
   philosophy grid until 820 px; and verified the 8100-port implementation.

## Extension loading architecture · 2026-08-26

final result: passed

- Source visual truth: `/var/folders/w0/9gh_1k4n3139f55jsjcr_zp40000gn/T/codex-clipboard-e167b32a-efbd-4a5e-a237-53d791fba288.png`
- Browser-rendered implementation: `/private/tmp/hhy-extension-flow-zh.png`
- Combined comparison: `/private/tmp/hhy-extension-flow-comparison.png`
- Route and state: `http://localhost:3000/zh/learn/extensions-roadmap`, extension-loading diagram, light theme
- Desktop viewport: 1720 × 900 CSS px at devicePixelRatio 2
- Source pixels: 1712 × 900; implementation component capture: 860 × 762
- Normalization: source downsampled to the implementation component width for the combined full-view comparison
- Responsive verification: 390 × 844 CSS px; no horizontal document overflow

### Fidelity surfaces

- Typography: retains the website sans/mono system while matching the reference's strong title, compact stage labels, and small technical annotations.
- Spacing and layout: six equal horizontal stages, numbered badges, directional arrows, error-return rail, four guarantee cards, and a compact footer caption. Mobile changes to one vertical flow.
- Colors and tokens: white/light-blue frame, blue-to-violet stage progression, restrained borders, and existing HHY blue tokens preserve both the reference hierarchy and site identity.
- Image and icon quality: all stage and guarantee symbols use the installed Phosphor vector icon library; there are no emoji, raster placeholders, or handcrafted SVG assets.
- Copy: Chinese and English labels describe the implemented v1.1.1 database extension lifecycle. The diagram does not claim Stream, opaque-handle, retry, or protocol-cancel support.

### Comparison history

1. P1: the earlier implementation used six generic text cards in a 3 × 2 grid and lacked the reference's architecture hierarchy. Replaced it with a six-stage icon-led horizontal flow.
2. P2: error behavior was initially reduced to a small caption. Added a dashed structured-error return rail connected to every stage.
3. P2: the first layout had no capability summary. Added Integrity, Protocol 1, four-callable registration, and structured-response cards grounded in the implementation.
4. Post-fix browser evidence confirms six stages and four guarantee cards in both languages, zero overflow at 390 px, and no browser console warnings or errors.

No focused crop was required beyond the component capture: all typography, icons, connectors, and summary cards remain readable in the normalized full-component comparison.

## CLI version terminal · 2026-08-26

final result: passed

- Source visual truth: `/var/folders/w0/9gh_1k4n3139f55jsjcr_zp40000gn/T/codex-clipboard-37d07c4d-2098-4217-b9dc-f7319969a19e.png`
- Browser-rendered implementation: `/private/tmp/hhy-cli-version-terminal-zh.jpg`
- Route and state: `http://localhost:3000/zh/learn/cli-reference`, version terminal card, light theme
- Desktop verification: default Codex in-app browser viewport
- Responsive verification: 390 × 844 CSS px; document scroll width equals viewport width

### Fidelity surfaces

- Typography: command and output use the existing mono font; title and caption preserve the documentation hierarchy.
- Spacing and layout: centered terminal window inside a pale-blue framed card with a separate explanatory caption, matching the reference structure.
- Colors and tokens: white terminal surface, restrained blue frame, neutral output text, and macOS traffic-light controls match the reference while retaining HHY site tokens.
- Image and icon quality: no rasterized terminal text or placeholder imagery; browser-rendered text remains sharp and selectable.
- Copy: Chinese and English cards show the exact six-line HHY 1.1.1 output, plus accurate source-build, release-archive, and PATH invocation guidance.

### Comparison history

1. P1: CLI reference had no visible version-output example. Added a dedicated version and release-identity section in both languages.
2. P2: a normal dark terminal block would not match the supplied light macOS reference. Added a separate light terminal-card treatment with title bar, command line, output, and caption.
3. Post-fix browser evidence confirms the complete output in English and Chinese, no console errors or warnings, and no horizontal overflow at 390 px.

## Language and VM evolution roadmap · 2026-08-26

final result: passed

- Source visual truth: `/var/folders/w0/9gh_1k4n3139f55jsjcr_zp40000gn/T/codex-clipboard-01ef0618-1668-4794-be84-63a92efa55d8.png`
- Browser-rendered implementation: `/private/tmp/hhy-language-vm-roadmap-zh.jpg`
- Route and state: `http://localhost:3000/zh/learn/language-vm-roadmap`, light theme, full roadmap diagram
- Desktop verification: default Codex in-app browser viewport
- Responsive verification: 390 × 844 CSS px; document scroll width equals viewport width

### Fidelity surfaces

- Typography: strong bilingual roadmap title, compact release labels, technical mono dates, readable card headings, and restrained small annotations follow the source hierarchy.
- Spacing and layout: two released-foundation cards precede a connected five-release timeline; the principle strip and caption mirror the source composition. Mobile uses one vertical timeline.
- Colors and tokens: white/light-blue surface, blue timeline, blue-to-violet final release, green released badges, and existing HHY tokens maintain release-state semantics.
- Image and icon quality: all roadmap symbols use the installed Phosphor vector library. No emoji, placeholder raster, or handcrafted SVG is used.
- Copy: Chinese and English content records v1.0.0/v1.1.0 history, presents v1.1.1 as the current performance and boundary-stability release, and limits future work to two gated directions.

### Comparison history

1. P1: the Learn manual had no language/VM evolution chapter. Added chapter 18 with a visual roadmap, release table, evolution principles, and explicit non-commitments.
2. P1: release lineage now retains v1.0.0 and v1.1.0, identifies v1.1.1 as current, and separates the two conditional future releases from shipped work.
3. P2: fixed calendar dates would overstate commitment. Future timing is labeled as recommended windows and each release is gated by measurable acceptance criteria.
4. Post-fix browser evidence confirms two released cards, five future cards, both languages, chapter-18 navigation from Learn, no console warnings/errors, and no horizontal overflow at 390 px.

### Future-release card restyle

- Rejected layout evidence: `/var/folders/w0/9gh_1k4n3139f55jsjcr_zp40000gn/T/codex-clipboard-ab688557-f338-4915-ac2e-258816197b72.png`
- Preferred visual language: `/var/folders/w0/9gh_1k4n3139f55jsjcr_zp40000gn/T/codex-clipboard-9b7ea306-e130-4431-8e39-3c350164e3b6.png`
- Updated browser implementation: `/private/tmp/hhy-language-vm-roadmap-compact-zh.jpg`

1. P1: five narrow vertical cards caused excessive line wrapping and made the future plan visually heavier than the released foundation.
2. P1: the card language was inconsistent with the compact, wide released-version cards immediately above it.
3. Fix: retained all five release plans and acceptance content, but adopted the preferred horizontal card anatomy: icon column, compact version/date/status row, title, and concise capability list.
4. Fix: desktop now uses two wide columns with the final v2.0 decision card spanning the row; mobile collapses to one column without changing reading order.
5. Post-fix evidence confirms five future cards, the full-width final card, bilingual content parity, and no browser console errors or warnings.

## About HHY luminous brand card · 2026-08-31

final result: passed

- Source visual truth: user-provided luminous HHY card with inset frame and concentric light rings.
- Route and state: `http://127.0.0.1:9800/zh/about`, light theme.
- The supplied HHY mark remains unchanged; the card uses the site's pale-blue palette, a bright inset frame, three concentric rings, soft depth, and a compact release badge.
- Desktop keeps copy and mark balanced. At 390 × 844, the mark appears first and the document has no horizontal overflow.
- Release state remains selectable text, the logo has alternative text, and primary actions remain functional links.
- Browser console is empty; ESLint, TypeScript, HHY example validation, and the optimized Next.js build pass.
