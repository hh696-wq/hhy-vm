# Design QA

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
real V1.1.0 examples replace the English concept placeholders. Header and footer
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
- Copy: Chinese and English labels describe the implemented v1.1.0 database extension lifecycle. The diagram does not claim Stream, opaque-handle, retry, or protocol-cancel support.

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
- Copy: Chinese and English cards show the exact six-line HHY 1.1.0 output, plus accurate source-build, release-archive, and PATH invocation guidance.

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
- Copy: Chinese and English content separates factual v1.0.0/v1.1.0 releases from five non-committed future windows and gives an acceptance gate for every proposed release.

### Comparison history

1. P1: the Learn manual had no language/VM evolution chapter. Added chapter 18 with a visual roadmap, release table, evolution principles, and explicit non-commitments.
2. P1: an initial future-only plan lacked release lineage. Added v1.0.0 and v1.1.0 as a separate released foundation using dates from repository tags, without treating them as future work.
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
